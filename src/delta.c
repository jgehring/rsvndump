/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008-2009 Jonas Gehring
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
 *      file: delta.c
 *      desc: The delta editor
 */


#include <sys/stat.h>
#include <unistd.h>

#include <svn_delta.h>
#include <svn_pools.h>
#include <svn_repos.h>
#include <svn_props.h>

#include <apr_hash.h>
#include <apr_file_io.h>

#include "main.h"
#include "dump.h"
#include "list.h"
#include "log.h"
#include "property.h"
#include "session.h"
#include "utils.h"

#include "delta.h"


/*---------------------------------------------------------------------------*/
/* Local data structures                                                     */
/*---------------------------------------------------------------------------*/


/* Main delta editor baton */
typedef struct {
	session_t		*session;
	dump_options_t		*opts;
	list_t			*logs;
	log_revision_t		*log_revision;
	svn_revnum_t		local_revnum;
} de_baton_t;


/* Enumerates valid node actions */
enum node_actions {
	NA_CHANGE,
	NA_ADD,
	NA_DELETE,
	NA_REPLACE
};


/* Node baton */
typedef struct {
	de_baton_t		*de_baton;
	apr_pool_t		*pool;
	char			*path;
	char			*filename;
	char			*old_filename;
	int			action;
	svn_node_kind_t		kind;
	apr_hash_t		*properties;
	char			*copyfrom_path;
	svn_revnum_t		copyfrom_revision;
	char			use_copy;
	char			dumped;
} de_node_baton_t;


/*---------------------------------------------------------------------------*/
/* Static variables                                                          */
/*---------------------------------------------------------------------------*/


/*
 * If the dump output is not using deltas, we need to keep a local copy of
 * every file in the repository. The delta_hash hash defines a mapping
 * repository paths to temporary files for this purpose
 */
static apr_pool_t *delta_pool = NULL;
static apr_hash_t *delta_hash = NULL;
#ifdef USE_TIMING
 static float tm_de_apply_textdelta = 0.0f;
#endif


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/


/* Creates a new node baton */
static de_node_baton_t *delta_create_node(de_node_baton_t *parent, apr_pool_t *pool)
{
	de_node_baton_t *node = apr_palloc(pool, sizeof(de_node_baton_t));
	node->de_baton = parent->de_baton;
	node->pool = pool;
	node->properties = apr_hash_make(pool);
	node->copyfrom_path = NULL;
	node->filename = NULL;
	node->old_filename = NULL;
	/* We don't need to dump this node if it has been copied */
	if (parent->use_copy) {
		node->dumped = 1;
		node->use_copy = 1;
	} else {
		node->dumped = 0;
		node->use_copy = 0;
	}
	return node;
}


/* Checks if a node can be dumped as a copy */
static char delta_check_copy(de_node_baton_t *node)
{
	session_t *session = node->de_baton->session;
	dump_options_t *opts = node->de_baton->opts;
	list_t *logs = node->de_baton->logs;
	svn_revnum_t local_revnum = node->de_baton->local_revnum;

	/* First, check if the source is reachable, i.e. can be found under
	   the current session root */
	if (!strncmp(session->prefix, node->copyfrom_path, strlen(session->prefix))) {
		svn_revnum_t r, rr = -1;
		svn_revnum_t mind = LONG_MAX;

		/* If we sync the revision numbers, the copy-from revision is correct */
		if (opts->flags & DF_KEEP_REVNUMS) {
			node->use_copy = 1;
			return 0;
		}

		/* This is good news: we already dumped the source. Let's figure
		   out at which revision */

		/* Find the best matching revision.
		   This will work, because if we have not dumped the requested
		   revision itself, the source of the copy has not changed between
		   the best matching and the requested revision. */
		for (r = local_revnum-1; r > 0; r--) {
			/* Yes, the +1 is needed */
			svn_revnum_t d = (node->copyfrom_revision - (((log_revision_t *)logs->elements)[r].revision))+1;
			DEBUG_MSG("delta_check_copy: req: %ld cur: %ld, local: %ld\n", node->copyfrom_revision, (((log_revision_t *)logs->elements)[r].revision), r);
			/* TODO: This can be optimized: Once we notice that the distance to the
			   requested revision gets bigger, it should be safe to break out of this
			   loop. */
			if (d >= 0 && d < mind) {
				mind = d;
				rr = r;
				if (d <= 1) {
					break;
				}
			}
		}

		node->copyfrom_revision = rr;
		node->use_copy = 1;
		DEBUG_MSG("delta_check_copy: using local %ld\n", rr);
	} else {
		/* Hm, this is bad. we have to ignore the copy operation and
		   simulate it by simple dumping it the node as being added.
		   This will work fine for single files, but directories
		   must be dumped recursively. */
		node->action = NA_ADD;
		node->use_copy = 0;
	}

	return 0;
}


/* Dumps a node */
static char delta_dump_node(de_node_baton_t *node)
{
	session_t *session = node->de_baton->session;
	dump_options_t *opts = node->de_baton->opts;
	char *path = node->path;
	unsigned long prop_len, content_len;

	/* If the node is a directory and no properties have been changed,
	   we don't need to dump it */
	if ((node->action != NA_ADD) && (node->kind == svn_node_dir) && (apr_hash_count(node->properties) == 0)) {
		node->dumped = 1;
		return 0;
	}

	/* Dump node path */
	if (opts->prefix != NULL) {
		printf("%s: %s%s\n", SVN_REPOS_DUMPFILE_NODE_PATH, opts->prefix, path);
	} else {
		printf("%s: %s\n", SVN_REPOS_DUMPFILE_NODE_PATH, path);
	}

	/* Dump node kind */
	if (node->action != NA_DELETE) {
		printf("%s: %s\n", SVN_REPOS_DUMPFILE_NODE_KIND, node->kind == svn_node_file ? "file" : "dir");
	}

	/* Dump action */
	printf("%s: ", SVN_REPOS_DUMPFILE_NODE_ACTION ); 
	switch (node->action) {
		case NA_CHANGE:
			printf("change\n"); 
			break;
		case NA_ADD:
			printf("add\n"); 
			break;
		case NA_DELETE:
			printf("delete\n"); 
			break;
		case NA_REPLACE:
			printf("replace\n"); 
			break;
	}

	/* If the node has been deleted, we can finish now */
	if (node->action == NA_DELETE) {
		printf("\n\n");
		node->dumped = 1;
		return 0;
	}

	/* Check for potential copy */
	if (node->copyfrom_path != NULL) {
		delta_check_copy(node);
		if (node->use_copy) {
			int offset = strlen(session->prefix);
			while (*(node->copyfrom_path+offset) == '/') {
				++offset;
			}
			printf("%s: %ld\n", SVN_REPOS_DUMPFILE_NODE_COPYFROM_REV, node->copyfrom_revision);
			if (opts->prefix != NULL) {
				printf("%s: %s%s\n", SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH, opts->prefix, node->copyfrom_path+offset);
			} else {
				printf("%s: %s\n", SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH, node->copyfrom_path+offset);
			}

			/* No further processing needed here */
			printf("\n\n");
			node->dumped = 1;
			return 0;
		}
	}

	/* Dump properties & content */
	prop_len = 0;
	content_len = 0;

	/* Dump property size */
	apr_hash_index_t *hi;
	for (hi = apr_hash_first(node->pool, node->properties); hi; hi = apr_hash_next(hi)) {
		const char *key;
		char *value;
		apr_hash_this(hi, (const void **)&key, NULL, (void **)&value);
		prop_len += property_strlen(key, value);
	}
	if ((prop_len > 0) || (node->action = NA_ADD)) {
		prop_len += PROPS_END_LEN;
		printf("%s: %lu\n", SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH, prop_len);
	}

	/* Dump content size */
	if (node->kind == svn_node_file) {
		struct stat st;
		if (stat(node->filename, &st)) {
			DEBUG_MSG("dump_delta_node: FATAL: cannot stat %s\n", node->filename);
			return 1;
		}
		content_len = st.st_size;

		if (opts->flags & DF_USE_DELTAS) {
			printf("%s: true\n", SVN_REPOS_DUMPFILE_TEXT_DELTA);
		}
		printf("%s: %lu\n", SVN_REPOS_DUMPFILE_TEXT_CONTENT_LENGTH, content_len);
	}
	printf("%s: %lu\n\n", SVN_REPOS_DUMPFILE_CONTENT_LENGTH, (unsigned long)prop_len+content_len);

	/* Dump properties */
	if ((prop_len > 0) || (node->action = NA_ADD)) {
		for (hi = apr_hash_first(node->pool, node->properties); hi; hi = apr_hash_next(hi)) {
			const char *key;
			char *value;
			apr_hash_this(hi, (const void **)&key, NULL, (void **)&value);
			property_dump(key, value);
		}
		printf(PROPS_END);
	}

	/* Dump content */
	if (node->kind == svn_node_file) {
		FILE *f;
		char *buffer = malloc(2049);
		size_t s;

		f = fopen(node->filename, "rb");
		if (f == NULL) {
			fprintf(stderr, _("ERROR: Failed to open %s.\n"), node->filename);
			free(buffer);
			return 1;
		}
		while ((s = fread(buffer, 1, 2048, f))) {
			fwrite(buffer, 1, s, stdout);
		}
		free(buffer);
		
		/* Close and remove temporary file */
		fclose(f);
		if (opts->flags & DF_USE_DELTAS) {
			unlink(node->filename);
		}
	}

	printf("\n\n");
	node->dumped = 1;
	return 0;
}


/* Subversion delta editor callback */
static svn_error_t *de_set_target_revision(void *edit_baton, svn_revnum_t target_revision, apr_pool_t *pool)
{
	/* The revision header has already been dumped, so there's nothing to
	   do here */
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_open_root(void *edit_baton, svn_revnum_t base_revision, apr_pool_t *dir_pool, void **root_baton)
{
	de_node_baton_t parent;
	de_node_baton_t *node;
	
	/* A little hackish, but there's no parent node yet */
	parent.de_baton = edit_baton;
	parent.use_copy = 0;
	node = delta_create_node(&parent, dir_pool);

	/*
	 * The revision header has already been dumped, so there's nothing to
	 * do for the root node
	 */
	node->dumped = 1;
	*root_baton = node;
#ifdef USE_TIMING
	tm_de_apply_textdelta = 0.0f;
#endif
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_delete_entry(const char *path, svn_revnum_t revision, void *parent_baton, apr_pool_t *pool)
{
	de_node_baton_t *parent = (de_node_baton_t *)parent_baton;
	dump_options_t *opts = parent->de_baton->opts;
	
	DEBUG_MSG("de_delete_entry(%s@%ld)\n", path, revision);

	/* Check if the parent dump needs to be dumped */
	if (!parent->dumped) {
		delta_dump_node(parent);
	}

	/* A deletion can be dumped without additional notification */
	if (opts->prefix != NULL) {
		printf("%s: %s%s\n", SVN_REPOS_DUMPFILE_NODE_PATH, opts->prefix, path);
	} else {
		printf("%s: %s\n", SVN_REPOS_DUMPFILE_NODE_PATH, path);
	}

	printf("%s: delete\n\n\n", SVN_REPOS_DUMPFILE_NODE_ACTION); 

	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_add_directory(const char *path, void *parent_baton, const char *copyfrom_path, svn_revnum_t copyfrom_revision, apr_pool_t *dir_pool, void **child_baton)
{
	de_node_baton_t *parent = (de_node_baton_t *)parent_baton;
	de_node_baton_t *node;

	/* Check if the parent node needs to be dumped */
	if (!parent->dumped) {
		delta_dump_node(parent);
	}

	node = delta_create_node(parent, dir_pool);
	node->kind = svn_node_dir;
	node->path = apr_pstrdup(dir_pool, path);
	node->action = NA_ADD;

	/*
	 * Check for copy. This needs to be done manually, since svn_ra_do_diff
	 * does not supply any copy information to the delta editor
	 */
	char *hpath = apr_palloc(dir_pool, strlen(path)+1);
	hpath[0] = '/';
	strcpy(hpath+1, path);
	svn_log_changed_path_t *log = apr_hash_get(node->de_baton->log_revision->changed_paths, hpath, APR_HASH_KEY_STRING);
	if (log != NULL) {
		if (log->copyfrom_path != NULL) {
			node->copyfrom_path = apr_pstrdup(dir_pool, log->copyfrom_path);
		}
		node->copyfrom_revision = log->copyfrom_rev;
	}

	*child_baton = node;
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_open_directory(const char *path, void *parent_baton, svn_revnum_t base_revision, apr_pool_t *dir_pool, void **child_baton)
{
	de_node_baton_t *parent = (de_node_baton_t *)parent_baton;
	de_node_baton_t *node;

	/* Check if the parent node needs to be dumped */
	if (!parent->dumped) {
		delta_dump_node(parent);
	}

	node = delta_create_node(parent, dir_pool);
	node->kind = svn_node_dir;;
	node->path = apr_pstrdup(dir_pool, path);
	node->action = NA_CHANGE;

	*child_baton = node;
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_change_dir_prop(void *dir_baton, const char *name, const svn_string_t *value, apr_pool_t *pool)
{
	/* We're only interested in regular properties */
	if (svn_property_kind(NULL, name) != svn_prop_regular_kind) {
		return SVN_NO_ERROR;
	}

	apr_hash_set(((de_node_baton_t *)dir_baton)->properties, apr_pstrdup(pool, name), APR_HASH_KEY_STRING, apr_pstrdup(pool, value->data));

	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_close_directory(void *dir_baton, apr_pool_t *pool)
{
	de_node_baton_t *node = (de_node_baton_t *)dir_baton;

	/* Check if the this node needs to be dumped */
	if (!node->dumped) {
		delta_dump_node(node);
	}

	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_absent_directory(const char *path, void *parent_baton, apr_pool_t *pool)
{
	DEBUG_MSG("absent_directory(%s)\n", path);
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_add_file(const char *path, void *parent_baton, const char *copyfrom_path, svn_revnum_t copyfrom_revision, apr_pool_t *file_pool, void **file_baton)
{
	de_node_baton_t *parent = (de_node_baton_t *)parent_baton;
	de_node_baton_t *node;

	/* Check if the parent node needs to be dumped */
	if (!parent->dumped) {
		delta_dump_node(parent);
	}

	DEBUG_MSG("de_add_file(%s)\n", path);

	node = delta_create_node(parent, file_pool);
	node->kind = svn_node_file;
	node->path = apr_pstrdup(file_pool, path);
	node->action = NA_ADD;

	/*
	 * Check for copy. This needs to be done manually, since svn_ra_do_diff
	 * does not supply any copy information to the delta editor
	 */
	char *hpath = apr_palloc(file_pool, strlen(path)+1);
	hpath[0] = '/';
	strcpy(hpath+1, path);
	svn_log_changed_path_t *log = apr_hash_get(node->de_baton->log_revision->changed_paths, hpath, APR_HASH_KEY_STRING);
	if (log != NULL) {
		if (log->copyfrom_path != NULL) {
			DEBUG_MSG("copyfrom_path = %s\n", log->copyfrom_path);
			node->copyfrom_path = apr_pstrdup(file_pool, log->copyfrom_path);
		}
		node->copyfrom_revision = log->copyfrom_rev;
	}

	*file_baton = node;
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_open_file(const char *path, void *parent_baton, svn_revnum_t base_revision, apr_pool_t *file_pool, void **file_baton)
{
	de_node_baton_t *parent = (de_node_baton_t *)parent_baton;
	de_node_baton_t *node;

	DEBUG_MSG("de_open_file(%s)\n", path);

	/* Check if the parent node needs to be dumped */
	if (!parent->dumped) {
		delta_dump_node(parent);
	}

	node = delta_create_node(parent, file_pool);
	node->kind = svn_node_file;
	node->path = apr_pstrdup(file_pool, path);
	node->action = NA_CHANGE;

	*file_baton = node;
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_apply_textdelta(void *file_baton, const char *base_checksum, apr_pool_t *pool, svn_txdelta_window_handler_t *handler, void **handler_baton)
{
	apr_file_t *src_file = NULL, *dest_file = NULL;
	svn_stream_t *src_stream, *dest_stream;
	de_node_baton_t *node = (de_node_baton_t *)file_baton;
	dump_options_t *opts = node->de_baton->opts;
#ifdef USE_TIMING
	stopwatch_t watch = stopwatch_create();
#endif

	/* Create a new temporary file to write to */
	node->filename = apr_palloc(pool, strlen(opts->temp_dir)+8);
	sprintf(node->filename, "%s/XXXXXX", opts->temp_dir);
	apr_file_mktemp(&dest_file, node->filename, APR_CREATE | APR_READ | APR_WRITE | APR_EXCL, pool);
	dest_stream = svn_stream_from_aprfile2(dest_file, FALSE, pool);

	if (opts->flags & DF_USE_DELTAS) {
		/* When dumping in delta mode, we just need to save the delta */
		svn_txdelta_to_svndiff(dest_stream, pool, handler, handler_baton);
	} else {
		/* Else, we need to update our local copy */
		char *filename = apr_hash_get(delta_hash, node->path, APR_HASH_KEY_STRING);
		if (filename == NULL) {
			src_stream = svn_stream_empty(pool);
		} else {
			apr_file_open(&src_file, filename, APR_READ, 0600, pool);
			src_stream = svn_stream_from_aprfile2(src_file, FALSE, pool);
		}

		svn_txdelta_apply(src_stream, dest_stream, NULL, node->path, pool, handler, handler_baton);
		apr_hash_set(delta_hash, apr_pstrdup(delta_pool, node->path), APR_HASH_KEY_STRING, apr_pstrdup(delta_pool, node->filename));
		node->old_filename = filename;
	}

#ifdef USE_TIMING
	tm_de_apply_textdelta += stopwatch_elapsed(&watch);
#endif
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_change_file_prop(void *file_baton, const char *name, const svn_string_t *value, apr_pool_t *pool)
{
	/* We're only interested in regular properties */
	if (svn_property_kind(NULL, name) != svn_prop_regular_kind) {
		return SVN_NO_ERROR;
	}

	DEBUG_MSG("de_change_file_prop(%s) %s = %s\n", ((de_node_baton_t *)file_baton)->path, name, value->data);

	apr_hash_set(((de_node_baton_t *)file_baton)->properties, apr_pstrdup(pool, name), APR_HASH_KEY_STRING, apr_pstrdup(pool, value->data));

	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_close_file(void *file_baton, const char *text_checksum, apr_pool_t *pool)
{
	de_node_baton_t *node = (de_node_baton_t *)file_baton;

	/* Remove the old file if neccessary */
	if (node->old_filename) {
		unlink(node->old_filename);
	}

	/* Check if the this node needs to be dumped */
	if (!node->dumped) {
		delta_dump_node(node);
	}
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_absent_file(const char *path, void *parent_baton, apr_pool_t *pool)
{
	DEBUG_MSG("absent_file(%s)\n", path);
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_close_edit(void *edit_baton, apr_pool_t *pool)
{
#ifdef USE_TIMING
	DEBUG_MSG("apply_text_delta: %f seconds\n", tm_de_apply_textdelta);
#endif
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_abort_edit(void *edit_baton, apr_pool_t *pool)
{
	DEBUG_MSG("abort_edit\n");
//	exit(1);
	return SVN_NO_ERROR;
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/


/* Sets up a delta editor for dumping a revision */
void delta_setup_editor(session_t *session, dump_options_t *options, list_t *logs, log_revision_t *log_revision, svn_revnum_t local_revnum, svn_delta_editor_t **editor, void **editor_baton, apr_pool_t *pool)
{
	*editor = svn_delta_default_editor(pool);
	(*editor)->set_target_revision = de_set_target_revision;
	(*editor)->open_root = de_open_root;
	(*editor)->delete_entry = de_delete_entry;
	(*editor)->add_directory = de_add_directory;
	(*editor)->open_directory = de_open_directory;
	(*editor)->add_file = de_add_file;
	(*editor)->open_file = de_open_file;
	(*editor)->apply_textdelta = de_apply_textdelta;
	(*editor)->close_file = de_close_file;
	(*editor)->close_directory = de_close_directory;
	(*editor)->change_file_prop = de_change_file_prop;
	(*editor)->change_dir_prop = de_change_dir_prop;
	(*editor)->close_edit = de_close_edit;
	(*editor)->absent_directory = de_absent_directory;
	(*editor)->absent_file = de_absent_file;
	(*editor)->abort_edit = de_abort_edit;

	de_baton_t *baton = apr_palloc(pool, sizeof(de_baton_t));
	baton->session = session;
	baton->opts = options;
	baton->logs = logs;
	baton->log_revision = log_revision;
	baton->local_revnum = local_revnum;
	*editor_baton = baton;

	/* Check if the global file hash needs to be created */
	if (!(options->flags & DF_USE_DELTAS) && delta_pool == NULL) {
		delta_pool = svn_pool_create(session->pool);
		delta_hash = apr_hash_make(delta_pool);
	}
}
