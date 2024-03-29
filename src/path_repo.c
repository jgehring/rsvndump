/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008-present Jonas Gehring
 *
 *      This program is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, either version 3 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *      file: path_repo.c
 *      desc: Versioned storage for file paths
 */


#include <assert.h>

#include <apr_tables.h>

#include <svn_ra.h>

#include "main.h"

#include "delta.h"
#include "logger.h"
#include "mukv.h"
#include "utils.h"

#include "critbit89/critbit.h"
#ifdef USE_SNAPPY
	#include "snappy-c/snappy.h"
#endif

#include "path_repo.h"


#define SNAPSHOT_INTERVAL (1<<10)  /* Interval for full-tree snapshots */
#define CACHE_SIZE 4               /* Number of cached full trees */


/*---------------------------------------------------------------------------*/
/* Local data structures                                                     */
/*---------------------------------------------------------------------------*/


typedef struct {
	svn_revnum_t revision;
	cb_tree_t tree;
} pr_cache_entry_t;


typedef struct {
	char action;
	char *path;
} pr_delta_entry_t;


struct path_repo_t {
	apr_pool_t *pool;
	mukv_t *db;

	apr_pool_t *delta_pool;
	apr_array_header_t *delta;
	int delta_len;
	cb_tree_t tree;
	svn_revnum_t head;

	apr_array_header_t *cache;   /* FIFO cache */
	int cache_index;

#ifdef USE_SNAPPY
	struct snappy_env snappy_env;
#endif

#ifdef DEBUG
	size_t delta_bytes;
	size_t delta_bytes_raw;
	int cache_hits;
	int cache_misses;
	apr_time_t recon_time;
	apr_time_t store_time;
#endif
};


/*---------------------------------------------------------------------------*/
/* Local functions                                                           */
/*---------------------------------------------------------------------------*/


/* Clears remaining memory of the path repo */
static apr_status_t pr_cleanup(void *data)
{
	path_repo_t *repo = data;
	int i;

#ifdef DEBUG
	L1("path_repo: snapshot interval:   %d\n", SNAPSHOT_INTERVAL);
	L1("path_repo: cache size:          %d\n", CACHE_SIZE);
	L1("path_repo: stored deltas:       %d kB\n", repo->delta_bytes / 1024);
	L1("path_repo: stored deltas (raw): %d kB\n", repo->delta_bytes_raw / 1024);
	L1("path_repo: total recon time:    %ld ms\n", apr_time_msec(repo->recon_time));
	L1("path_repo: total store time:    %ld ms\n", apr_time_msec(repo->store_time));
	L1("path_repo: cache miss rate:     %.2f%% (%d of %d)\n", 100.0f*repo->cache_misses / (repo->cache_hits+repo->cache_misses), repo->cache_misses, (repo->cache_hits+repo->cache_misses));
#endif

	cb_tree_clear(&repo->tree);
	for (i = 0; i < repo->cache->nelts; i++) {
		if (APR_ARRAY_IDX(repo->cache, i, pr_cache_entry_t).tree.root) {
			cb_tree_clear(&APR_ARRAY_IDX(repo->cache, i, pr_cache_entry_t).tree);
		}
	}

	mukv_close(repo->db);

#ifdef USE_SNAPPY
	snappy_free_env(&repo->snappy_env);
#endif
	return APR_SUCCESS;
}


/* Initializes the revision cache */
static void pr_cache_init(path_repo_t *repo, int size)
{
	int i;
	repo->cache = apr_array_make(repo->pool, size, sizeof(pr_cache_entry_t));
	for (i = 0; i < size; i++) {
		APR_ARRAY_PUSH(repo->cache, pr_cache_entry_t).revision = -1;
		APR_ARRAY_IDX(repo->cache, i, pr_cache_entry_t).tree = cb_tree_make();
	}
	repo->cache_index = 0;
}


/* Callback for pr_tree_to_array() */
struct pr_ttoa_data {
	apr_array_header_t *arr;
	size_t path_len;
	apr_pool_t *pool;
};
static int pr_tree_to_array_cb(const char *elem, void *arg) {
	struct pr_ttoa_data *data = arg;
	if (data->path_len == 0 || elem[data->path_len] == 0 || elem[data->path_len] == '/') {
		APR_ARRAY_PUSH(data->arr, const char *) = apr_pstrdup(data->pool, elem);
	}
	return 0;
}

/* Returns all children of path in the given tree as an array */
static apr_array_header_t *pr_tree_to_array(cb_tree_t *tree, const char *path, apr_pool_t *pool)
{
	struct pr_ttoa_data data;
	data.arr = apr_array_make(pool, 0, sizeof(char *));
	data.pool = pool;
	data.path_len = strlen(path);
	if (cb_tree_walk_prefixed(tree, path, pr_tree_to_array_cb, &data) != 0) {
		return NULL;
	}
	return data.arr;
}


/* Encodes a whole tree to a series of add operations */
static int pr_encode(cb_tree_t *tree, char **data, size_t *len, apr_pool_t *pool)
{
	int i;
	char *dptr;
	apr_array_header_t *arr = pr_tree_to_array(tree, "", pool);
	if (arr == NULL) {
		return -1;
	}

	/* Compute length */
	*len = 0;
	for (i = 0; i < arr->nelts; i++) {
		*len += 2 + strlen(APR_ARRAY_IDX(arr, i, char *));
	}

	/* Encode */
	*data = apr_palloc(pool, *len);
	dptr = *data;
	for (i = 0; i < arr->nelts; i++) {
		*dptr++ = '+';
		strcpy(dptr, APR_ARRAY_IDX(arr, i, char *));
		dptr += strlen(APR_ARRAY_IDX(arr, i, char *));
		*dptr++ = '\0';
	}
	return 0;
}


/* Applies a serialized tree delta to a tree */
static int pr_delta_apply(cb_tree_t *tree, const char *data, int len, apr_pool_t *pool)
{
	const char *dptr = data;
	while (dptr - data < len) {
		if (*dptr == '+') {
			cb_tree_insert(tree, dptr+1);
		} else {
			cb_tree_delete(tree, dptr+1);
		}
		dptr += 2 + strlen(dptr+1);
	}
	return 0;
}


/* Reconstructs a tree for the given revision */
static int pr_reconstruct(path_repo_t *repo, cb_tree_t *tree, svn_revnum_t revision, apr_pool_t *pool)
{
	mdatum_t key, val;
	svn_revnum_t r;
	char *dptr;
	size_t dsize;
#ifdef DEBUG
	apr_time_t start = apr_time_now();
#endif

	/* Start at position of last snapshot and apply deltas */
	r = (revision & ~(SNAPSHOT_INTERVAL-1));
	while (r <= revision) {
		key.dptr = apr_itoa(pool, r);
		key.dsize = strlen(key.dptr);

		if (mukv_exists(repo->db, key)) {
			val = mukv_fetch(repo->db, key, pool);
			if (val.dptr == NULL) {
				fprintf(stderr, _("Error fetching tree delta for revision %ld\n"), r);
				return -1;
			}
#ifdef USE_SNAPPY
			if (!snappy_uncompressed_length(val.dptr, val.dsize, &dsize)) {
				return -1;
			}
			dptr = malloc(dsize);
			if (snappy_uncompress(val.dptr, val.dsize, dptr) != 0) {
				free(dptr);
				return -1;
			}
#else
			dptr = val.dptr;
			dsize = val.dsize;
#endif
			if (pr_delta_apply(tree, dptr, dsize, pool) != 0) {
				fprintf(stderr, _("Error applying tree delta for revision %ld\n"), r);
				return -1;
			}

#ifdef USE_SNAPPY
			free(dptr);
#endif
		}
		++r;
	}

#ifdef DEBUG
	repo->recon_time += (apr_time_now() - start);
#endif
	return 0;
}


/* Returns a tree for the given revision */
static cb_tree_t *pr_tree(path_repo_t *repo, svn_revnum_t revision, apr_pool_t *pool)
{
	cb_tree_t *tree;
	int i;

	/* Check if tree is cached */
	for (i = 0; i < repo->cache->nelts; i++) {
		if (APR_ARRAY_IDX(repo->cache, i, pr_cache_entry_t).revision == revision) {
			tree = &APR_ARRAY_IDX(repo->cache, i, pr_cache_entry_t).tree;
#ifdef DEBUG
			repo->cache_hits++;
#endif
			break;
		}
	}

	/* Reconstruct tree if needed */
	if (i >= repo->cache->nelts) {
#ifdef DEBUG
		repo->cache_misses++;
#endif
		tree = &APR_ARRAY_IDX(repo->cache, repo->cache_index, pr_cache_entry_t).tree;
		if (tree->root != NULL) {
			cb_tree_clear(tree);
		}
		if (pr_reconstruct(repo, tree, revision, pool) != 0) {
			return NULL;
		}

		APR_ARRAY_IDX(repo->cache, repo->cache_index, pr_cache_entry_t).revision = revision;
		if (++repo->cache_index >= repo->cache->nelts) {
			repo->cache_index = 0;
		}
	}

	return tree;
}


/* Fetches paths from the repository and stores them into the given array */
static int pr_fetch_paths_rec(apr_array_header_t *paths, const char *path, svn_revnum_t rev, session_t *session, apr_pool_t *pool)
{
	svn_error_t *err;
	apr_hash_t *dirents;
	apr_hash_index_t *hi;

	if ((err = svn_ra_get_dir2(session->ra, &dirents, NULL, NULL, path, rev, SVN_DIRENT_KIND, pool))) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		return -1;
	}

	for (hi = apr_hash_first(pool, dirents); hi; hi = apr_hash_next(hi)) {
		const char *entry;
		char *subpath;
		svn_dirent_t *dirent;
		apr_hash_this(hi, (const void **)&entry, NULL, (void **)&dirent);

		/* Add the full path of the entry */
		if (strlen(path) > 0) {
			subpath = apr_psprintf(pool, "%s/%s", path, entry);
		} else {
			subpath = apr_pstrdup(pool, entry);
		}

		if (dirent->kind == svn_node_file) {
			APR_ARRAY_PUSH(paths, char *) = subpath;
		} else if (dirent->kind == svn_node_dir) {
			APR_ARRAY_PUSH(paths, char *) = subpath;
			pr_fetch_paths_rec(paths, subpath, rev, session, pool);
		}
	}
	return 0;
}

/* Fetches paths from the repository and stores them into the given array */
static int pr_fetch_paths(apr_array_header_t *paths, const char *path, svn_revnum_t rev, session_t *session, apr_pool_t *pool)
{
	svn_error_t *err;
	svn_dirent_t *dirent;

	/* Check node type */
	if ((err = svn_ra_stat(session->ra, path, rev, &dirent, pool))) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		return -1;
	}

	if (dirent == NULL) {
		DEBUG_MSG("pr_fetch_paths(): Non-existent: %s@%ld\n", path, rev);
		return 0;
	}

	APR_ARRAY_PUSH(paths, char *) = apr_pstrdup(pool, path);
	if (dirent->kind == svn_node_file) {
		return 0;
	}
	return pr_fetch_paths_rec(paths, path, rev, session, pool);
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/


/* Creates a new path repository in the given directory */
path_repo_t *path_repo_create(const char *tmpdir, apr_pool_t *pool)
{
	apr_pool_t *subpool = svn_pool_create(pool);
	path_repo_t *repo = apr_pcalloc(subpool, sizeof(path_repo_t));
	const char *db_path;

	repo->pool = subpool;
	repo->delta_pool = svn_pool_create(repo->pool);

	repo->tree = cb_tree_make();
	repo->delta = apr_array_make(repo->pool, 1, sizeof(pr_delta_entry_t));
	pr_cache_init(repo, CACHE_SIZE);

	/* Open database */
	db_path = apr_psprintf(pool, "%s/paths.db", tmpdir);
	repo->db = mukv_open(db_path, repo->pool);
	if (repo->db == NULL) {
		fprintf(stderr, _("Error creating path database (%s)\n"), strerror(errno));
		return NULL;
	}

#ifdef USE_SNAPPY
	if (snappy_init_env(&repo->snappy_env) != 0) {
		fprintf(stderr, _("Error initializing snappy compressor\n"));
		return NULL;
	}
#endif

	apr_pool_cleanup_register(repo->pool, repo, pr_cleanup, apr_pool_cleanup_null);
	return repo;
}


/* Schedules the given path for addition */
int path_repo_add(path_repo_t *repo, const char *path, apr_pool_t *pool)
{
	pr_delta_entry_t *e = &APR_ARRAY_PUSH(repo->delta, pr_delta_entry_t);
	e->action = '+';
	e->path = apr_pstrdup(repo->delta_pool, path);
	repo->delta_len += (2 + strlen(path));

	if (cb_tree_insert(&repo->tree, e->path) != 0) {
		return -1;
	}

	(void)pool; /* Prevent compiler warnings */
	return 0;
}


/* Schedules the given path for deletion */
int path_repo_delete(path_repo_t *repo, const char *path, apr_pool_t *pool)
{
	apr_array_header_t *paths = pr_tree_to_array(&repo->tree, path, pool);
	int i;

	for (i = 0; i < paths->nelts; i++) {
		char *p = APR_ARRAY_IDX(paths, i, char *);
		pr_delta_entry_t *e = &APR_ARRAY_PUSH(repo->delta, pr_delta_entry_t);
		e->action = '-';
		e->path = apr_pstrdup(repo->delta_pool, p);
		repo->delta_len += (2 + strlen(p));

		cb_tree_delete(&repo->tree, e->path);
	}
	return 0;
}


/* Commits all scheduled actions, using the given revision number */
int path_repo_commit(path_repo_t *repo, svn_revnum_t revision, apr_pool_t *pool)
{
	mdatum_t key, val;
	int i;
	char *dptr = NULL;
#ifdef USE_SNAPPY
	size_t dsize;
#endif
#ifdef DEBUG
	apr_time_t start = apr_time_now();
#endif
	int snapshot = (revision > 0 && (revision % SNAPSHOT_INTERVAL == 0));

	/* Skip empty revisions if there's no snapshot pending */
	if (repo->delta_len <= 0 && !snapshot) {
		repo->head = revision;
		return 0;
	}

	/* Encode data if necessary */
	if (!snapshot) {
		val.dptr = apr_palloc(pool, repo->delta_len);
		val.dsize = repo->delta_len;
		dptr = val.dptr;

		for (i = 0; i < repo->delta->nelts; i++) {
			pr_delta_entry_t *e = &APR_ARRAY_IDX(repo->delta, i, pr_delta_entry_t);

			*dptr++ = e->action;
			strcpy(dptr, e->path);
			dptr += strlen(e->path);
			*dptr++ = '\0';
		}
	} else {
		if (pr_encode(&repo->tree, &val.dptr, &val.dsize, pool) != 0) {
			fprintf(stderr, _("Error encoding tree data for snapshot\n"));
			return -1;
		}
	}

#ifdef DEBUG
	repo->delta_bytes_raw += val.dsize;
#endif

#ifdef USE_SNAPPY
	dptr = apr_palloc(pool, snappy_max_compressed_length(val.dsize));
	if (snappy_compress(&repo->snappy_env, val.dptr, val.dsize, dptr, &dsize) != 0) {
		fprintf(stderr, _("Error compressing tree data for revision %ld\n"), revision);
		return -1;
	}
	val.dptr = dptr;
	val.dsize = dsize;
#endif

#ifdef DEBUG
	repo->delta_bytes += val.dsize;
#endif

	key.dptr = apr_itoa(pool, revision);
	key.dsize = strlen(key.dptr);
	if (mukv_store(repo->db, key, val) != 0) {
		fprintf(stderr, _("Error storing paths for revision %ld\n"), revision);
		return -1;
	}

	repo->head = revision;
	repo->delta_len = 0;
	apr_array_clear(repo->delta);
	svn_pool_clear(repo->delta_pool);
#ifdef DEBUG
	repo->store_time += (apr_time_now() - start);
#endif
	return 0;
}


/* Discards all scheduled actions */
int path_repo_discard(path_repo_t *repo, apr_pool_t *pool)
{
	repo->delta_len = 0;
	apr_array_clear(repo->delta);
	svn_pool_clear(repo->delta_pool);

	/* Revert to previous head */
	cb_tree_clear(&repo->tree);
	return pr_reconstruct(repo, &repo->tree, repo->head, pool);
}


/* Commits a SVN log entry, using the given revision number */
int path_repo_commit_log(path_repo_t *repo, session_t *session, dump_options_t *opts, log_revision_t *log, svn_revnum_t revision, apr_array_header_t *logs, apr_pool_t *pool)
{
	apr_hash_index_t *hi;
	apr_array_header_t *paths;
	int i, j;

	if (log->changed_paths == NULL) {
		/* Commit empty revision */
		return path_repo_commit(repo, revision, pool);
	}

	/* Sort changed paths */
	/* TODO: More advanced sorting necessary? */
	paths = apr_array_make(pool, apr_hash_count(log->changed_paths), sizeof(const char *));
	for (hi = apr_hash_first(pool, log->changed_paths); hi; hi = apr_hash_next(hi)) {
		const char *path;
		apr_hash_this(hi, (const void **)&path, NULL, NULL);
		APR_ARRAY_PUSH(paths, const char *) = path;
	}
	utils_sort(paths);

	/* Fill current delta */
	for (i = 0; i < paths->nelts; i++) {
		const char *path = APR_ARRAY_IDX(paths, i, const char *);
		svn_log_changed_path_t *info = apr_hash_get(log->changed_paths, path, APR_HASH_KEY_STRING);

		if (info->copyfrom_path == NULL) {
			DEBUG_MSG("path_repo_commit(%ld): %c %s\n", revision, info->action, path);
		} else {
			DEBUG_MSG("path_repo_commit(%ld): %c %s (from %s@%ld)\n", revision, info->action, path, info->copyfrom_path, info->copyfrom_rev);
		}

		if (info->action == 'D' || info->action == 'R') {
			path_repo_delete(repo, path, pool);
		}
		if (info->action != 'A' && info->action != 'R') {
			continue;
		}

		/* info->action == 'A' || info->action == 'R' */
		if (info->copyfrom_path == NULL) {
			path_repo_add(repo, path, pool);
		} else {
			apr_array_header_t *cpaths;
			const char *copyfrom_path = delta_get_local_copyfrom_path(session->prefix, info->copyfrom_path);

			if (copyfrom_path == NULL) {
				cpaths = apr_array_make(pool, 1, sizeof(char *));
				if (pr_fetch_paths(cpaths, path, log->revision, session, pool) != 0) {
					fprintf(stderr, _("Error fetching tree for revision %ld\n"), log->revision);
					return -1;
				}

				for (j = 0; j < cpaths->nelts; j++) {
					path_repo_add(repo, APR_ARRAY_IDX(cpaths, j, char *), pool);
				}
			} else {
				svn_revnum_t copyfrom_rev = delta_get_local_copyfrom_rev(info->copyfrom_rev, opts, logs, revision);
				cb_tree_t *tree = pr_tree(repo, copyfrom_rev, pool);
				if (tree == NULL) {
					return -1;
				}

				cpaths = pr_tree_to_array(tree, copyfrom_path, pool);
				assert(cpaths->nelts > 0);

				if (cpaths->nelts == 1) {
					/* Single file copied */
					path_repo_add(repo, path, pool);
				} else {
					unsigned int copyfrom_path_len = strlen(copyfrom_path);
					for (j = 0; j < cpaths->nelts; j++) {
						const char *relpath = APR_ARRAY_IDX(cpaths, j, char *);
						assert(strlen(relpath) >= copyfrom_path_len);
						relpath = apr_psprintf(pool, "%s%s", path, relpath + copyfrom_path_len);
						path_repo_add(repo, relpath, pool);
					}
				}
			}
		}
	}

	/* Commit */
	return path_repo_commit(repo, revision, pool);
}


/* Checks if a path exists at a given revision */
extern signed char path_repo_exists(path_repo_t *repo, const char *path, svn_revnum_t revision, apr_pool_t *pool)
{
	cb_tree_t *tree;
	if (revision < 0) {
		return 0;
	}

	tree = pr_tree(repo, revision, pool);
	if (tree == NULL) {
		return -1;
	}
	if (cb_tree_contains(tree, path)) {
		return 1;
	}
	return 0;
}


/* Checks the parent relation of two paths at a given revision */
signed char path_repo_check_parent(path_repo_t *repo, const char *parent, const char *child, svn_revnum_t revision, apr_pool_t *pool)
{
	char *path;
	cb_tree_t *tree;

	if (revision < 0) {
		return 0;
	}

	tree = pr_tree(repo, revision, pool);
	if (tree == NULL) {
		return -1;
	}
	path = apr_psprintf(pool, "%s/%s", parent, child);
	if (cb_tree_contains(tree, path)) {
		return 1;
	}
	return 0;
}

#ifdef DEBUG

/* Verifies a given revision */
int path_repo_test(path_repo_t *repo, session_t *session, svn_revnum_t revision, svn_revnum_t svn_rev, apr_pool_t *pool)
{
	apr_array_header_t *paths_recon;
	apr_array_header_t *paths_orig;
	cb_tree_t tree = cb_tree_make();
	int i, ret = 0;

	/* Retrieve reconstructed tree */
	if (pr_reconstruct(repo, &tree, revision, pool) != 0) {
		fprintf(stderr, _("Error reconstructing tree for revision %ld\n"), revision);
		return 1;
	}
	paths_recon = pr_tree_to_array(&tree, "", pool);

	/* Retrieve actual tree -- assume the session is rooted at a directory */
	paths_orig = apr_array_make(pool, 0, sizeof(char *));
	pr_fetch_paths(paths_orig, "", svn_rev, session, pool);
	utils_sort(paths_orig);

	/* Skip empty root element from original tree (HACK!) */
	if (paths_orig->nelts > 0 && strlen(APR_ARRAY_IDX(paths_orig, 0, char *)) == 0) {
		paths_orig->elts += sizeof(char *);
		paths_orig->nelts--;
	}

	/* Compare trees */
	if (paths_recon->nelts != paths_orig->nelts) {
		fprintf(stderr, "r%ld: #recon = %d != %d = #orig\n", revision, paths_recon->nelts, paths_orig->nelts);
		ret = 1;
	}
	for (i = 0; i < paths_recon->nelts; i++) {
		char *p = APR_ARRAY_IDX(paths_recon, i, char *);
		if (utils_search(p, paths_orig) == NULL) {
			fprintf(stderr, "r%ld: in recon, not in orig: %s\n", revision, p);
			ret = 1;
		}
	}
	for (i = 0; i < paths_orig->nelts; i++) {
		char *p = APR_ARRAY_IDX(paths_orig, i, char *);
		if (utils_search(p, paths_recon) == NULL) {
			fprintf(stderr, "r%ld: in orig, not in recon: %s\n", revision, p);
			ret = 1;
		}
	}

	cb_tree_clear(&tree);
	return ret;
}


/* Verifies all revisions stored in the path repository */
int path_repo_test_all(path_repo_t *repo, session_t *session, apr_pool_t *pool)
{
	svn_revnum_t rev = 0;
	apr_pool_t *revpool = svn_pool_create(pool);
	int ret = 0;

	L0("Checking path_repo until revision %ld...\n", repo->head);
	while (ret == 0 && ++rev < repo->head) {
		mdatum_t key;

		/* Skip all revisions that haven't been committed */
		key.dptr = apr_itoa(pool, rev);
		key.dsize = strlen(key.dptr);
		if (!mukv_exists(repo->db, key)) {
			continue;
		}

		ret = path_repo_test(repo, session, rev, rev, revpool);
		svn_pool_clear(revpool);
	}
	return ret;
}

#endif /* DEBUG */
