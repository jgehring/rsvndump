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

#include "main.h"
#include "dump.h"
#include "list.h"

#include "delta.h"


/*---------------------------------------------------------------------------*/
/* Local data structures                                                     */
/*---------------------------------------------------------------------------*/


/* Main delta editor baton */
typedef struct {
	dump_options_t		*opts;
	list_t			*logs;
	svn_revnum_t		local_revnum;
} de_baton_t;


/* Node baton */
typedef struct {
	de_baton_t		*de_baton;
	char			*path;
	char			*filename;
	list_t			properties;
} node_baton_t;


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/


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
	DEBUG_MSG("dump_open_root()\n");
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_delete_entry(const char *path, svn_revnum_t revision, void *parent_baton, apr_pool_t *pool)
{
	DEBUG_MSG("delete_entry(%s)\n");
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_add_directory(const char *path, void *parent_baton, const char *copyfrom_path, svn_revnum_t copyfrom_revision, apr_pool_t *dir_pool, void **child_baton)
{
	DEBUG_MSG("add_directory(%s)\n", path);
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_open_directory(const char *path, void *parent_baton, svn_revnum_t base_revision, apr_pool_t *dir_pool, void **child_baton)
{
	DEBUG_MSG("open_directory(%s)\n");
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_change_dir_prop(void *dir_baton, const char *name, const svn_string_t *value, apr_pool_t *pool)
{
	DEBUG_MSG("change_dir_prop(%s)\n", name);
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_close_directory(void *dir_baton, apr_pool_t *pool)
{
	DEBUG_MSG("close_direcotry()\n");
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
	DEBUG_MSG("add_file(%s)\n", path);
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_open_file(const char *path, void *parent_baton, svn_revnum_t base_revision, apr_pool_t *file_pool, void **file_baton)
{
	DEBUG_MSG("open_file(%s)\n", path);
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_apply_textdelta(void *file_baton, const char *base_checksum, apr_pool_t *pool, svn_txdelta_window_handler_t *handler, void **handler_baton)
{
	svn_stream_t *stream;
	DEBUG_MSG("apply_textdelta()\n");
	svn_stream_for_stdout(&stream, pool);
	svn_txdelta_to_svndiff(stream, pool, handler, handler_baton);
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_change_file_prop(void *file_baton, const char *name, const svn_string_t *value, apr_pool_t *pool)
{
	DEBUG_MSG("change_file_prop()\n");
	return SVN_NO_ERROR;
}


/* Subversion delta editor callback */
static svn_error_t *de_close_file(void *file_baton, const char *text_checksum, apr_pool_t *pool)
{
	DEBUG_MSG("close_file()\n");
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
	DEBUG_MSG("close_edit\n");
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
void delta_setup_editor(dump_options_t *options, list_t *logs, svn_revnum_t local_revnum, svn_delta_editor_t **editor, void **editor_baton, apr_pool_t *pool)
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
	baton->opts = options;
	baton->logs = logs;
	baton->local_revnum = local_revnum;
	*editor_baton = baton;
}
