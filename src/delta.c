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
	char			*path;
	char			*filename;
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
static de_node_baton_t *delta_create_node(de_baton_t *baton, apr_pool_t *pool)
{
	de_node_baton_t *node = apr_palloc(pool, sizeof(de_node_baton_t));
	node->de_baton = baton;
	node->properties = apr_hash_make(pool);
	node->copyfrom_path = NULL;
	node->use_copy = 0;
	node->dumped = 0;
	return node;
}


/* Dumps a node */
static char delta_dump_node(de_node_baton_t *node)
{
	session_t *session = node->de_baton->session;
	dump_options_t *opts = node->de_baton->opts;
	char *path = node->path;

	/* If the node is a directory and no properties have been changed,
	   we don't need to dump it */
//	if (node->action != NA_ADD && node->kind == svn_node_dir && node->props.size == 0) {
//		node->dumped = 1;
//		return 0;
//	}

	if (session->prefix_is_file) {
		path = strrchr(session->encoded_url, '/')+1;
	}
	if (opts->prefix != NULL) {
		printf("%s: %s%s\n", SVN_REPOS_DUMPFILE_NODE_PATH, opts->prefix, path);
	} else {
		printf("%s: %s\n", SVN_REPOS_DUMPFILE_NODE_PATH, path);
	}

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
	de_node_baton_t *node = delta_create_node(edit_baton, dir_pool);
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

	node = delta_create_node(parent->de_baton, dir_pool);
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

	node = delta_create_node(parent->de_baton, dir_pool);
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

	apr_hash_set(((de_node_baton_t *)dir_baton)->properties, name, APR_HASH_KEY_STRING, value->data);

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

	node = delta_create_node(parent->de_baton, file_pool);
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

	/* Check if the parent node needs to be dumped */
	if (!parent->dumped) {
		delta_dump_node(parent);
	}

	node = delta_create_node(parent->de_baton, file_pool);
	node->kind = svn_node_file;
	node->path = apr_pstrdup(file_pool, path);
	node->action = NA_CHANGE;

	*file_baton = node;
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_apply_textdelta(void *file_baton, const char *base_checksum, apr_pool_t *pool, svn_txdelta_window_handler_t *handler, void **handler_baton)
{
	apr_file_t *file;
	svn_stream_t *stream;
	de_node_baton_t *node = (de_node_baton_t *)file_baton;
	dump_options_t *opts = node->de_baton->opts;
#ifdef USE_TIMING
	stopwatch_t watch = stopwatch_create();
#endif

	/* Create a new temporary file to write to */
	node->filename = apr_palloc(pool, strlen(opts->temp_dir)+8);
	sprintf(node->filename, "%s/XXXXXX", opts->temp_dir);
	apr_file_mktemp(&file, node->filename, APR_CREATE | APR_READ | APR_WRITE | APR_EXCL, pool);
	stream = svn_stream_from_aprfile2(file, FALSE, pool);

	if (opts->flags & DF_USE_DELTAS) {
		/* When dumping in delta mode, we just need to save the delta */
		svn_txdelta_to_svndiff(stream, pool, handler, handler_baton);
	} else {
		/* Else, we need to update our local copy */
		svn_stream_t *src;
		char *filename = apr_hash_get(delta_hash, node->path, APR_HASH_KEY_STRING);
		if (filename == NULL) {
			src = svn_stream_empty(pool);
		} else {
			apr_file_open(&file, filename, APR_READ | APR_WRITE | APR_EXCL, 0600, pool);
			src = svn_stream_from_aprfile2(file, FALSE, pool);
		}

		svn_txdelta_apply(src, stream, NULL, node->path, pool, handler, handler_baton);
		apr_hash_set(delta_hash, node->path, APR_HASH_KEY_STRING, apr_pstrdup(delta_pool, node->filename));

		/* We can safely remove the previous file now */
		if (filename) {
			unlink(filename);
		}
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

	apr_hash_set(((de_node_baton_t *)file_baton)->properties, name, APR_HASH_KEY_STRING, value->data);

	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_close_file(void *file_baton, const char *text_checksum, apr_pool_t *pool)
{
	de_node_baton_t *node = (de_node_baton_t *)file_baton;

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
