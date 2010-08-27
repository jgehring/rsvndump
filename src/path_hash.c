/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008-2010 Jonas Gehring
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
 *      file: path_hash.c
 *      desc: A global hash that stores paths of specific revisions
 *
 *      The path_hash is used to track the repository layout at all
 *      previous revisions. This is useful when handling directory
 *      copies and deciding which nodes will actually be copied.
 *
 *      Each tree is a hash of hashes, and each entry represents
 *      a directory entry. The hash makes no difference between files
 *      and directories. After all, this isn't needed to check
 *      the parent-relation of a file and a directory.
 *
 *      The current implementation stores tree deltas instead of full
 *      layouts for each revision to minimize memory usage.
 *      Snapshots are take at regular intervals to speed up reconstruction.
 *      Only the two latest snapshots (and the revisions between them) are
 *      actually kept in memory. Previous records are stored in
 *      files containing the snapshot itself and the following deltas.
 *
 *      Furthermore, there's a small full-tree cache to speed up
 *      repeated queries (quite common whenever a directory is copied).
 */


#include <stdio.h>

#include <svn_path.h>
#include <svn_ra.h>

#include <apr_hash.h>
#include <apr_tables.h>

#include "main.h"

#include "delta.h"
#include "utils.h"

#include "path_hash.h"


/* Interval for taking snapshots of the full tree */
#define SNAPSHOT_DIST 512

/* Cache size for reconstructed repositories */
#define CACHE_SIZE 4

/* Codes for reading and writing the path history */
#define REV_SEPERATOR "=="
#define REV_ADD "+ "
#define REV_DELETE "- "


/*---------------------------------------------------------------------------*/
/* Local data structures                                                     */
/*---------------------------------------------------------------------------*/


/* A tree delta structure */
typedef struct {
	/*
	 * For nodes that are being added, a hash tree is used for every
	 * part of the path. For deletions, there's a simple array of strings.
	 */
	apr_pool_t *pool;
	apr_hash_t *added;
	apr_array_header_t *deleted;
} tree_delta_t;


/* Full-tree cache structure */
typedef struct {
	apr_pool_t *pool;
	svn_revnum_t revnum;
	apr_hash_t *tree;
} cached_tree_t;



/*---------------------------------------------------------------------------*/
/* Static variables                                                          */
/*---------------------------------------------------------------------------*/


/* Global pool */
static apr_pool_t *ph_pool = NULL;

/* Array storing all deltas */
static apr_array_header_t *ph_revisions = NULL;

/* Additional session prefix (for copyfrom_path information) */
static const char *ph_session_prefix = "";

/* Temporary directory */
static const char *ph_temp_dir = NULL;

/* The current (head) delta */
static tree_delta_t *ph_head = NULL;

/* Full-tree snapshots */
static apr_array_header_t *ph_snapshots = NULL;

/* History files */
static apr_array_header_t *ph_files = NULL;

/* Full-tree cache */
static apr_array_header_t *ph_cache = NULL;
static int ph_cache_pos = 0;


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/


/* Adds a path to a tree, using the tree's hash and pool for temporary allcations */
static apr_hash_t *path_hash_add(apr_hash_t *tree, const char *path, apr_pool_t *pool)
{
	const char *parent, *child;
	apr_hash_t *parent_tree, *child_tree;
	apr_pool_t *hash_pool = apr_hash_pool_get(tree);

	/* Determine tree of parent node via recursion */
	svn_path_split(path, &parent, &child, pool);
	if (!strcmp(parent, child)) {
		return tree;
	}
	parent_tree = path_hash_add(tree, parent, pool);

	/* Add the node to the parent tree if neede */
	child_tree = apr_hash_get(parent_tree, child, APR_HASH_KEY_STRING);
	if (child_tree == NULL) {
		child_tree = apr_hash_make(hash_pool);
		apr_hash_set(parent_tree, apr_pstrdup(hash_pool, child), APR_HASH_KEY_STRING, child_tree);
	}
	DEBUG_MSG("/%s", child);
	return child_tree;
}


/* Fetches a tree from a repository and adds every path to the given hash, recursively */
static char path_hash_add_tree_rec(session_t *session, apr_hash_t *tree, const char *path, svn_revnum_t revnum, apr_pool_t *pool)
{
	svn_error_t *err;
	apr_hash_t *dirents;
	apr_hash_index_t *hi;
	apr_pool_t *subpool = svn_pool_create(pool);

	if ((err = svn_ra_get_dir2(session->ra, &dirents, NULL, NULL, path, revnum, SVN_DIRENT_KIND, pool))) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		return 1;
	}

	/* Iterate over entries and add them to the tree using their full paths */
	for (hi = apr_hash_first(pool, dirents); hi; hi = apr_hash_next(hi)) {
		const char *entry;
		char *subpath;
		svn_dirent_t *dirent;
		apr_hash_this(hi, (const void **)(void *)&entry, NULL, (void **)(void *)&dirent);

		subpath = apr_psprintf(subpool, "%s/%s", path, entry);

		if (dirent->kind == svn_node_file) {
			DEBUG_MSG("path_hash: S++ ");
			path_hash_add(tree, subpath, pool);
			DEBUG_MSG("\n");
		} else if (dirent->kind == svn_node_dir) {
			DEBUG_MSG("path_hash: S++ ");
			path_hash_add(tree, subpath, pool);
			DEBUG_MSG("\n");
			path_hash_add_tree_rec(session, tree, subpath, revnum, subpool);
		}
	}

	svn_pool_destroy(subpool);
	return 0;
}


/* Fetches a tree from a repository and adds every path to the given hash */
static char path_hash_add_tree(session_t *session, apr_hash_t *tree, const char *path, svn_revnum_t revnum, apr_pool_t *pool)
{
	svn_error_t *err;
	svn_dirent_t *dirent;

	/*
	 * Check the node type first. If it is a file, we can simply add it.
	 * Otherwise, add the the directory contents recursively.
	 */
	if ((err = svn_ra_stat(session->ra, path, revnum, &dirent, pool))) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		return 1;
	}

	if (dirent->kind == svn_node_file) {
		DEBUG_MSG("path_hash: S++ ");
		path_hash_add(tree, path, pool);
		DEBUG_MSG("\n");
		return 0;
	}

	return path_hash_add_tree_rec(session, tree, path, revnum, pool);
}


/* Returns the given subtree for path or NULL if path is not present in the tree */
static apr_hash_t *path_hash_subtree(apr_hash_t *tree, const char *path, apr_pool_t *pool)
{
	const char *parent, *child;
	apr_hash_t *parent_tree;

	/* Determine tree of parent node via recursion */
	svn_path_split(path, &parent, &child, pool);
	if (!strcmp(parent, child)) {
		return tree;
	}
	parent_tree = path_hash_subtree(tree, parent, pool);
	if (parent_tree == NULL) {
		return NULL;
	}

	return apr_hash_get(parent_tree, child, APR_HASH_KEY_STRING);
}


/* Deletes a path (and thus, all its children) from a tree. This doesn't free
   the memory, though */
static void path_hash_delete(apr_hash_t *tree, const char *path, apr_pool_t *pool)
{
	const char *parent, *child;
	apr_hash_t *parent_tree;

	/* Determine tree of parent node subtree method */
	svn_path_split(path, &parent, &child, pool);
	if (!strcmp(parent, child)) {
		return;
	}
	parent_tree = path_hash_subtree(tree, parent, pool);

	/* Remove the child */
	if (parent_tree) {
		apr_hash_set(parent_tree, child, APR_HASH_KEY_STRING, NULL);
	}
}


/* Recursively copies the contents of one hash to another */
static void path_hash_copy_deep(apr_hash_t *dest, apr_hash_t *source, apr_pool_t *pool)
{
	apr_hash_index_t *hi;
	apr_pool_t *hash_pool = apr_hash_pool_get(dest);

	for (hi = apr_hash_first(pool, source); hi; hi = apr_hash_next(hi)) {
		const char *key;
		apr_hash_t *child_source, *child_dest;
		apr_hash_this(hi, (const void **)(void *)&key, NULL, (void **)(void *)&child_source);

		/* Create a new child hash under the same name and copy its contents */
		child_dest = apr_hash_make(hash_pool);
		path_hash_copy_deep(child_dest, child_source, pool);

		apr_hash_set(dest, apr_pstrdup(hash_pool, key), APR_HASH_KEY_STRING, child_dest);
	}
}


/* Reconstructs a revision from a file */
static apr_hash_t *path_hash_reconstruct_file(svn_revnum_t rev, apr_pool_t *pool)
{
	apr_hash_t *tree;
	apr_file_t *file = NULL;
	apr_status_t status;
	apr_pool_t *temp_pool;
	unsigned long i, filename_idx = (rev / SNAPSHOT_DIST);
	int rev_add_len, rev_delete_len, rev_sep_len;

	DEBUG_MSG("path_hash_test: reconstruct_file(%ld) started\n", rev);

	if (ph_files->nelts < filename_idx) {
		DEBUG_MSG("path_hash_reconstruct_file(%ld): file not available (%d < %d)\n", rev, ph_files->nelts, filename_idx);
		return NULL;
	}

	/* Try to open the file */
	temp_pool = svn_pool_create(pool);
	status = apr_file_open(&file, APR_ARRAY_IDX(ph_files, filename_idx, const char *), APR_READ | APR_BINARY, APR_OS_DEFAULT, temp_pool);
	if (status) {
		fprintf(stderr, _("ERROR: Unable to open temporary file %s\n"), APR_ARRAY_IDX(ph_files, filename_idx, const char *));
		svn_pool_destroy(temp_pool);
		return NULL;
	}

	DEBUG_MSG("path_hash_test: reconstruct_file(%ld) using file %s\n", rev, APR_ARRAY_IDX(ph_files, filename_idx, const char *));
	tree = apr_hash_make(pool);

	rev_add_len = strlen(REV_ADD);
	rev_delete_len = strlen(REV_DELETE);
	rev_sep_len = strlen(REV_SEPERATOR);

	/* Read revisions */
	i = filename_idx * SNAPSHOT_DIST;
	while (i <= rev) {
		char *buffer = utils_file_readln(temp_pool, file);
		if (buffer == NULL) {
			fprintf(stderr, _("ERROR: Unable to read from temporary file %s\n"), APR_ARRAY_IDX(ph_files, filename_idx, const char *));
			apr_file_close(file);
			svn_pool_destroy(temp_pool);
			return NULL;
		}

		if (!strncmp(buffer, REV_ADD, rev_add_len)) {
			DEBUG_MSG("path_hash_test: +++ %s\n", buffer + rev_add_len);
			path_hash_add(tree, buffer + rev_add_len, temp_pool);
		} else if (!strncmp(buffer, REV_DELETE, rev_delete_len)) {
			path_hash_delete(tree, buffer + rev_delete_len, temp_pool);
		} else if (!strncmp(buffer, REV_SEPERATOR, rev_sep_len)) {
			++i;
		}
	}

	apr_file_close(file);
	svn_pool_destroy(temp_pool);
	return tree;
}


/* Reconstructs the complete tree at the given revision, allocated in pool */
static apr_hash_t *path_hash_reconstruct(svn_revnum_t rev, apr_pool_t *pool)
{
	apr_hash_t *tree;
	apr_array_header_t *stack, *path;
	apr_hash_index_t *hi;
	apr_pool_t *temp_pool;
	unsigned long i, j, snapshot_idx = (rev / SNAPSHOT_DIST);

	if (ph_revisions->nelts < rev) {
		DEBUG_MSG("path_hash_reconstruct(%ld): revision not available (max = %d)\n", rev, ph_revisions->nelts);
		return NULL;
	}

	/* Maybe we need to read from a file */
	if (ph_snapshots->nelts > snapshot_idx && APR_ARRAY_IDX(ph_snapshots, snapshot_idx, apr_hash_t *) == NULL) {
		if (ph_snapshots->nelts > 2) {
			DEBUG_MSG("path_hash_reconstruct(%ld): using file\n", rev);
			return path_hash_reconstruct_file(rev, pool);
		} else {
			return NULL;
		}
	}

	tree = apr_hash_make(pool);
	temp_pool = svn_pool_create(pool);
	stack = apr_array_make(temp_pool, 0, sizeof(apr_hash_t *));
	path = apr_array_make(temp_pool, 0, sizeof(const char *));

	/* Restore the tree from the last snapshot if possible */
	i = 0;
	if (ph_snapshots->nelts > snapshot_idx) {
		path_hash_copy_deep(tree, APR_ARRAY_IDX(ph_snapshots, snapshot_idx, apr_hash_t *), temp_pool);
		i = (snapshot_idx * SNAPSHOT_DIST) + 1;
	} else if (rev > 0) {
		/* No snapshot here. Try to load the previous revision */
		apr_hash_t *previous = path_hash_reconstruct(rev-1, temp_pool);
		if (previous != NULL) {
			path_hash_copy_deep(tree, previous, temp_pool);
			i = rev-1;
		}
	}

	while (i <= rev) {
		tree_delta_t *delta = APR_ARRAY_IDX(ph_revisions, i++, tree_delta_t *);

		/* Skip padding revisions */
		if (delta == NULL) {
			continue;
		}

		/* Apply delta: deletions */
		for (j = 0; j < delta->deleted->nelts; j++) {
			path_hash_delete(tree, APR_ARRAY_IDX(delta->deleted, j, const char *), pool);
		}

		/* Continue if there are no additions */
		if (delta->added == NULL) {
			continue;
		}

		/* Apply delta: additions */
		APR_ARRAY_PUSH(stack, apr_hash_t *) = delta->added;
		APR_ARRAY_PUSH(path, const char *) = apr_pstrdup(pool, "/");
		while (stack->nelts > 0) {
			apr_hash_t *top_hash = *((apr_hash_t **)apr_array_pop(stack));
			const char *top_path = *((const char **)apr_array_pop(path));

			/*
			 * Push child nodes to the stack. If there aren't any
			 * left, add the path to the tree. The parents will be
			 * added automatically
			 */
			if (apr_hash_count(top_hash) == 0) {
				path_hash_add(tree, top_path, pool);
				continue;
			}

			for (hi = apr_hash_first(temp_pool, top_hash); hi; hi = apr_hash_next(hi)) {
				const char *key;
				apr_hash_t *value;
				apr_hash_this(hi, (const void **)(void *)&key, NULL, (void **)(void *)&value);

				APR_ARRAY_PUSH(stack, apr_hash_t *) = value;
				APR_ARRAY_PUSH(path, const char *) = svn_path_join(top_path, key, temp_pool);
			}
		}
	}

	svn_pool_destroy(temp_pool);
	return tree;
}


/* Copies a path from a specific revision (recursively) */
static char path_hash_copy(apr_hash_t *tree, const char *path, const char *from, svn_revnum_t revnum, apr_pool_t *pool)
{
	apr_hash_t *recon, *subtree, *newtree;
	apr_pool_t *recon_pool = svn_pool_create(pool);

	/* First, reconstruct tree at the given revision */
	recon = path_hash_reconstruct(revnum, recon_pool);
	if (recon == NULL) {
		DEBUG_MSG("path_hash: !!! reconstruction failed for %ld!\n", revnum);
		svn_pool_destroy(recon_pool);
		return 1;
	}

	/* Determine location of the path */
	subtree = path_hash_subtree(recon, from, recon_pool);
	if (subtree == NULL) {
		DEBUG_MSG("path_hash: !!! no subtree for %s@%ld\n", from, revnum);
		svn_pool_destroy(recon_pool);
		return 1;
	}

	/* Add the complete subtree */
	DEBUG_MSG("path_hash:     +++ ");
	newtree = path_hash_add(tree, path, pool);
	DEBUG_MSG("\n");
	path_hash_copy_deep(newtree, subtree, pool);

	svn_pool_destroy(recon_pool);
	return 0;
}


/* Writes a series of tree deltas to the given file */
static char path_hash_write(apr_hash_t *tree, apr_file_t *file, apr_pool_t *pool)
{
	apr_hash_index_t *hi;
	apr_array_header_t *recon_stack = apr_array_make(pool, 0, sizeof(apr_hash_t *));
	apr_array_header_t *recon_path = apr_array_make(pool, 0, sizeof(const char *));

	APR_ARRAY_PUSH(recon_stack, apr_hash_t *) = tree;
	APR_ARRAY_PUSH(recon_path, const char *) = "/";

	while (recon_stack->nelts > 0) {
		apr_hash_t *top_hash = *(apr_hash_t **)apr_array_pop(recon_stack);
		const char *top_path = *(const char **)apr_array_pop(recon_path);

		if (apr_hash_count(top_hash) == 0) {
			if (strcmp(top_path, "/")) {
				DEBUG_MSG("path_hash_write: +++ %s\n", top_path);
				apr_file_printf(file, "%s%s\n", REV_ADD, top_path);
			}
			continue;
		}

		for (hi = apr_hash_first(pool, top_hash); hi; hi = apr_hash_next(hi)) {
			const char *key;
			apr_hash_t *value;
			apr_hash_this(hi, (const void **)(void *)&key, NULL, (void **)(void *)&value);

			APR_ARRAY_PUSH(recon_stack, apr_hash_t *) = value;
			APR_ARRAY_PUSH(recon_path, const char *) = svn_path_join(top_path, key, pool);
		}
	}

	return 0;
}


/* Writes the first available snapshot an all following deltas to a temporary
   file and returns the file path. Additionally, the snapshots and the deltas
   will be freed */
static char *path_hash_write_deltas(apr_pool_t *pool)
{
	char *filename;
	int index;
	apr_status_t status;
	apr_hash_t *tree;
	apr_file_t *file = NULL;
	apr_pool_t *delta_pool = svn_pool_create(pool);

	filename = apr_psprintf(pool, "%s/XXXXXX", ph_temp_dir);
	status = apr_file_mktemp(&file, filename, APR_CREATE | APR_READ | APR_WRITE | APR_EXCL | APR_BINARY, pool);
	DEBUG_MSG("path_hash: temp_file = %s : %d\n", filename, status);
	if (status) {
		fprintf(stderr, _("ERROR: Unable to create temporary file (%d)\n"), status);
		return NULL;
	}

	/* Write the snapshot first */
	index = 0;
	while ((tree = APR_ARRAY_IDX(ph_snapshots, index, apr_hash_t *)) == NULL) {
		++index;
	}
	if (path_hash_write(tree, file, delta_pool)) {
		apr_file_close(file);
		return NULL;
	}

	svn_pool_destroy(apr_hash_pool_get(tree));
	APR_ARRAY_IDX(ph_snapshots, index, apr_hash_t *) = NULL;

	/* Write all deltas */
	index *= SNAPSHOT_DIST;
	index += 1;
	do {
		tree_delta_t *delta = APR_ARRAY_IDX(ph_revisions, index, tree_delta_t *);
		int j;

		apr_file_printf(file, "%s\n", REV_SEPERATOR);

		/* Skip padding revisions */
		if (delta == NULL) {
			++index;
			continue;
		}

		/* Deletions */
		for (j = 0; j < delta->deleted->nelts; j++) {
			DEBUG_MSG("path_hash_write: --- %s\n", APR_ARRAY_IDX(delta->deleted, j, const char *));
			apr_file_printf(file, "%s%s\n", REV_DELETE, APR_ARRAY_IDX(delta->deleted, j, const char *));
		}

		/* Additions */
		svn_pool_clear(delta_pool);
		if (path_hash_write(delta->added, file, delta_pool)) {
			apr_file_close(file);
			return NULL;
		}

		DEBUG_MSG("path_hash_write: --------------------------------------------------------------\n");

		svn_pool_destroy(delta->pool);
		APR_ARRAY_IDX(ph_revisions, index, tree_delta_t *) = NULL;
	} while ((index++ % SNAPSHOT_DIST) != 0);

	DEBUG_MSG("path_hash_write: ==============================================================\n");

	apr_file_close(file);
	svn_pool_destroy(delta_pool);
	return filename;
}


#ifdef DEBUG

/* Debugging */
static void path_hash_dump_prefix(const char *prefix, apr_hash_t *tree, apr_pool_t *pool)
{
#ifdef DEBUG_PHASH
	apr_hash_index_t *hi;
	apr_array_header_t *recon_stack = apr_array_make(pool, 0, sizeof(apr_hash_t *));
	apr_array_header_t *recon_path = apr_array_make(pool, 0, sizeof(const char *));

	DEBUG_MSG("%s: ----------------------------------------------\n", prefix);
	APR_ARRAY_PUSH(recon_stack, apr_hash_t *) = tree;
	APR_ARRAY_PUSH(recon_path, const char *) = "/";
	while (recon_stack->nelts > 0) {
		apr_hash_t *top_hash = *(apr_hash_t **)apr_array_pop(recon_stack);
		const char *top_path = *(const char **)apr_array_pop(recon_path);

		if (apr_hash_count(top_hash) == 0) {
			DEBUG_MSG("%s: %s\n", prefix, top_path);
			continue;
		}

		for (hi = apr_hash_first(pool, top_hash); hi; hi = apr_hash_next(hi)) {
			const char *key;
			apr_hash_t *value;
			apr_hash_this(hi, (const void **)(void *)&key, NULL, (void **)(void *)&value);

			APR_ARRAY_PUSH(recon_stack, apr_hash_t *) = value;
			APR_ARRAY_PUSH(recon_path, const char *) = svn_path_join(top_path, key, pool);
		}
	}
	DEBUG_MSG("%s: ----------------------------------------------\n", prefix);
#endif
}

static void path_hash_dump(apr_hash_t *tree, apr_pool_t *pool)
{
	path_hash_dump_prefix("path_hash", tree, pool);
}

#endif


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/


/* Initializes the path hash using the given pool */
void path_hash_initialize(const char *session_prefix, const char *temp_dir, apr_pool_t *parent_pool)
{
	if (ph_pool == NULL) {
		int i;

		/* Allocate global storage */
		ph_pool = svn_pool_create(parent_pool);
		ph_revisions = apr_array_make(ph_pool, 0, sizeof(tree_delta_t *));
		ph_session_prefix = apr_pstrdup(ph_pool, session_prefix);
		ph_temp_dir = apr_pstrdup(ph_pool, temp_dir);
		ph_snapshots = apr_array_make(ph_pool, 0, sizeof(apr_hash_t *));
		ph_files = apr_array_make(ph_pool, 0, sizeof(const char *));
		ph_cache = apr_array_make(ph_pool, CACHE_SIZE, sizeof(cached_tree_t));

		/* Initialize cache */
		for (i = 0; i < CACHE_SIZE; i++) {
			APR_ARRAY_IDX(ph_cache, i, cached_tree_t).tree = NULL;
		}
	}
}


/* Manually adds a new path to the head revision (without committing it) */
void path_hash_add_path(const char *path)
{
	apr_pool_t *pool = svn_pool_create(ph_pool);

	if (ph_head == NULL) {
		ph_head = apr_palloc(ph_pool, sizeof(tree_delta_t));
		ph_head->pool = svn_pool_create(ph_pool);
		ph_head->added = apr_hash_make(ph_head->pool);
		ph_head->deleted = apr_array_make(ph_head->pool, 0, sizeof(const char *));
	}

	DEBUG_MSG("path_hash: M++ ");
	path_hash_add(ph_head->added, path, pool);
	DEBUG_MSG("\n");

	svn_pool_destroy(pool);
}


/* Adds a new revision to the path hash */
char path_hash_commit(session_t *session, log_revision_t *log_full, int log_start, svn_revnum_t revnum, char adjust_missing_revnums)
{
	apr_hash_index_t *hi;
	apr_pool_t *pool = svn_pool_create(ph_pool);
	log_revision_t *log = log_full + log_start;

	if (ph_revisions->nelts > revnum) {
		DEBUG_MSG("path_hash: not commiting previous revision %ld\n", revnum);
		return 1;
	}

	if (log->changed_paths != NULL) {
#if defined(DEBUG_PHASH) && defined(DEBUG)
		DEBUG_MSG("path_hash: changed paths in revision %ld:\n", revnum);
		for (hi = apr_hash_first(pool, log->changed_paths); hi; hi = apr_hash_next(hi)) {
			const char *path;
			svn_log_changed_path_t *info;
			apr_hash_this(hi, (const void **)(void *)&path, NULL, (void **)(void *)&info);

			DEBUG_MSG("path_hash: %c %s", info->action, path);
			if (info->copyfrom_path) {
				int offset = strlen(ph_session_prefix);
				while (*(info->copyfrom_path+offset) == '/') {
					++offset;
				}

				DEBUG_MSG(" (from %s@%d [%s])\n", info->copyfrom_path, info->copyfrom_rev, info->copyfrom_path + offset);
			} else {
				DEBUG_MSG("\n");
			}
		}
#endif

		if (ph_head == NULL) {
			ph_head = apr_palloc(ph_pool, sizeof(tree_delta_t));
			ph_head->pool = svn_pool_create(ph_pool);
			ph_head->added = apr_hash_make(ph_head->pool);
			ph_head->deleted = apr_array_make(ph_head->pool, 0, sizeof(const char *));
		}

		/* Iterate over the changed paths of this revision and store them
		 * in the current tree delta */
		for (hi = apr_hash_first(pool, log->changed_paths); hi; hi = apr_hash_next(hi)) {
			const char *path;
			svn_log_changed_path_t *info;
			apr_hash_this(hi, (const void **)(void *)&path, NULL, (void **)(void *)&info);

			if (info->action == 'A') {
				if (info->copyfrom_path) {
					const char *copyfrom_path = delta_get_local_copyfrom_path(ph_session_prefix, info->copyfrom_path);
					if (copyfrom_path == NULL) {
						/* The copy source is not under the session root, so add the tree manually */
						if (path_hash_add_tree(session, ph_head->added, path, log->revision, pool)) {
							return 1;
						}
					} else {
						svn_revnum_t copyfrom_rev = info->copyfrom_rev;

						if (adjust_missing_revnums) {
							int backtrack = 0;
							svn_revnum_t last_rev = 0;

							/* Iterate through all the revs and find the one that corresponds to the copied one.
							 * We also adjust to the "last good" one if we hit a missing revision.
							 * This is to workaround a quirk in Codeplex's SVNBridge. */
							while (backtrack <= log_start && last_rev < copyfrom_rev) {
								svn_revnum_t rev = ((log_revision_t *)log_full)[backtrack++].revision;

								if (rev > copyfrom_rev) {
									DEBUG_MSG("Adjusted copy revision from %ld to %ld\n", copyfrom_rev, last_rev);
									copyfrom_rev = last_rev;
									break;
								}
								last_rev = rev;
							}
						}

						DEBUG_MSG("path_hash: +++ %s@%d -> %s\n", copyfrom_path, copyfrom_rev, path, ph_session_prefix);
						if (path_hash_copy(ph_head->added, path, copyfrom_path, copyfrom_rev, pool)) {
							/*
							 * The copy source could not be determined. However, it is
							 * important to have a consistent history, so add the whole tree
							 * manually.
							 */
							if (path_hash_add_tree(session, ph_head->added, path, log->revision, pool)) {
								return 1;
							}
						}
					}
				} else {
					DEBUG_MSG("path_hash: +++ ");
					path_hash_add(ph_head->added, path, pool);
					DEBUG_MSG("\n");
				}
			} else if (info->action == 'D') {
				DEBUG_MSG("path_hash: --- %s\n", path);
				APR_ARRAY_PUSH(ph_head->deleted, const char *) = apr_pstrdup(ph_head->pool, path);
			}
		}
	}

	/* Finally, add the new revision after possible padding */
	while (ph_revisions->nelts < revnum-1) {
		APR_ARRAY_PUSH(ph_revisions, tree_delta_t *) = NULL;
		DEBUG_MSG("path_hash: padded revision %d\n", ph_revisions->nelts);
	}
	APR_ARRAY_PUSH(ph_revisions, tree_delta_t *) = ph_head;
	ph_head = NULL;

	/* Add regular snapshots to speed up path reconstructions */
	if ((revnum % SNAPSHOT_DIST) == 0) {
		apr_hash_t *snapshot = path_hash_reconstruct(revnum, svn_pool_create(ph_pool));
		APR_ARRAY_PUSH(ph_snapshots, apr_hash_t *) = snapshot;

		/* Only keep the last two snapshots and the deltas between them
		   in memory. All other deltas will be saved to files. */
		if (ph_snapshots->nelts > 2) {
			char *filename = path_hash_write_deltas(pool);
			if (filename != NULL) {
				APR_ARRAY_PUSH(ph_files, const char *) = apr_pstrdup(ph_pool, filename);
			} else {
				/* TODO: Write error message */
				return 1;
			}
		}
	}

#if defined(DEBUG_PHASH) && defined(DEBUG)
	DEBUG_MSG("path_hash: for revision %ld:\n", revnum);
	path_hash_dump(path_hash_reconstruct(revnum, pool), pool);
#endif

	DEBUG_MSG("path_hash: commited revision %ld\n", revnum);
	svn_pool_destroy(pool);
	return 0;
}


/* Checks the parent relation of two paths at a given revision */
char path_hash_check_parent(const char *parent, const char *child, svn_revnum_t revnum, apr_pool_t *pool)
{
	int i;
	apr_hash_t *recon = NULL, *subtree;

	DEBUG_MSG("path_hash: check_parent(%s, %s, %ld): %ld revisions\n", parent, child, revnum, ph_revisions->nelts);

	/* Check the cache first */
	for (i = 0; i < CACHE_SIZE; i++) {
		cached_tree_t *entry = &APR_ARRAY_IDX(ph_cache, i, cached_tree_t);
		if (entry->tree != NULL && entry->revnum == revnum) {
			recon = entry->tree;
			break;
		}
	}

	/* If it isn't cached, reconstruct the tree at the given revision */
	if (recon == NULL) {
		recon = path_hash_reconstruct(revnum, pool);
		if (recon == NULL) {
			DEBUG_MSG("path_hash: unable to reconstruct hash for revision %ld\n", revnum);
			return 0;
		} else {
			/* Insert the tree into the cache */
			cached_tree_t *entry = &APR_ARRAY_IDX(ph_cache, ph_cache_pos, cached_tree_t);

			/* Clear old entry if neccessary */
			if (entry->tree) {
				svn_pool_destroy(entry->pool);
			}

			entry->revnum = revnum;
			entry->pool = svn_pool_create(ph_pool);
			entry->tree = apr_hash_make(entry->pool);
			path_hash_copy_deep(entry->tree, recon, pool);

			/* Move to next cache entry */
			if (++ph_cache_pos >= CACHE_SIZE) {
				ph_cache_pos = 0;
			}
		}
	}

	/* Find the subtree of the parent node */
	subtree = path_hash_subtree(recon, parent, pool);
	if (subtree == NULL) {
		DEBUG_MSG("path_hash: subtree for %s in %ld not found:\n", parent, revnum);
#if 0 && defined(DEBUG)
		path_hash_dump(recon, pool);
#endif
	}

	/* Find the subtree of the child */
	return (subtree && path_hash_subtree(subtree, child, pool) != NULL);
}

#ifdef DEBUG_PHASH

/* Testing: compare two hashes */
char path_hash_test_compare(apr_hash_t *orig, apr_hash_t *recon, apr_pool_t *pool)
{
	apr_hash_index_t *hi;
	apr_array_header_t *recon_stack = apr_array_make(pool, 0, sizeof(apr_hash_t *));
	apr_array_header_t *recon_path = apr_array_make(pool, 0, sizeof(const char *));
	char ret = 0;

	/* First, check if every path in orig is in recon */
	APR_ARRAY_PUSH(recon_stack, apr_hash_t *) = orig;
	APR_ARRAY_PUSH(recon_path, const char *) = "/";

	while (recon_stack->nelts > 0) {
		apr_hash_t *top_hash = *(apr_hash_t **)apr_array_pop(recon_stack);
		const char *top_path = *(const char **)apr_array_pop(recon_path);

		if (apr_hash_count(top_hash) == 0) {
			if (strcmp(top_path, "/")) {
				if (path_hash_subtree(recon, top_path, pool) == NULL) {
					DEBUG_MSG("path_hash_test: NOT IN RECON: %s\n", top_path);
					ret = 1;
				}
			}
			continue;
		}

		for (hi = apr_hash_first(pool, top_hash); hi; hi = apr_hash_next(hi)) {
			const char *key;
			apr_hash_t *value;
			apr_hash_this(hi, (const void **)(void *)&key, NULL, (void **)(void *)&value);

			APR_ARRAY_PUSH(recon_stack, apr_hash_t *) = value;
			APR_ARRAY_PUSH(recon_path, const char *) = svn_path_join(top_path, key, pool);
		}
	}

	/* Finally, the other way round */
	APR_ARRAY_PUSH(recon_stack, apr_hash_t *) = recon;
	APR_ARRAY_PUSH(recon_path, const char *) = "/";

	while (recon_stack->nelts > 0) {
		apr_hash_t *top_hash = *(apr_hash_t **)apr_array_pop(recon_stack);
		const char *top_path = *(const char **)apr_array_pop(recon_path);

		if (apr_hash_count(top_hash) == 0) {
			if (strcmp(top_path, "/")) {
				if (path_hash_subtree(orig, top_path, pool) == NULL) {
					DEBUG_MSG("path_hash_test: NOT IN ORIG: %s\n", top_path);
					ret = 1;
				}
			}
			continue;
		}

		for (hi = apr_hash_first(pool, top_hash); hi; hi = apr_hash_next(hi)) {
			const char *key;
			apr_hash_t *value;
			apr_hash_this(hi, (const void **)(void *)&key, NULL, (void **)(void *)&value);

			APR_ARRAY_PUSH(recon_stack, apr_hash_t *) = value;
			APR_ARRAY_PUSH(recon_path, const char *) = svn_path_join(top_path, key, pool);
		}
	}

	return ret;
}

/* A short implementation testing function */
void path_hash_test(session_t *session)
{
	svn_revnum_t i = 0;
	apr_pool_t *pool = svn_pool_create(ph_pool);

	/*
	 * The idea is pretty simple. All available revisions are first reconstructed
	 * and then retrieved manually. Afterwards, both hashes will be compared.
	 */
	while (i < ph_revisions->nelts) {
		apr_hash_t *orig;
		apr_hash_t *recon;

		/* Retrieve actual repository tree */
		orig = apr_hash_make(pool);
		path_hash_add_tree(session, orig, ph_session_prefix, i, pool);

		/* Reconstruct the tree using the path_hash */
		recon = path_hash_reconstruct(i, pool);
		if (recon == NULL) {
			DEBUG_MSG("path_hash_test: recon failed!\n");
			exit(1);
		}
//		path_hash_dump(recon, pool);

		DEBUG_MSG("path_hash_test: testing revision %ld\n", i);
		if (path_hash_test_compare(orig, recon, pool)) {
			break;
		}

		svn_pool_clear(pool);
		++i;
	}

	svn_pool_destroy(pool);
	exit(1);
}

#endif
