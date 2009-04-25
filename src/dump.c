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
 *      file: dump.c
 *      desc: Main working place
 */


#include <sys/stat.h>

#include <svn_delta.h>
#include <svn_pools.h>
#include <svn_props.h>
#include <svn_ra.h>
#include <svn_repos.h>

#include <apr_hash.h>
#include <apr_pools.h>

#include "main.h"
#include "list.h"
#include "log.h"

#include "dump.h"


/*---------------------------------------------------------------------------*/
/* Local data structures                                                     */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Static variables                                                          */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/


/* Dumps a revision header using the given properties */
static void dump_revision_header(dump_options_t *opts)
{
#if 0
	int props_length;

	props_length = 0;
	property_k

	if (opts->keep_revnums) {
		printf("%s: %ld\n", SVN_REPOS_DUMPFILE_REVISION_NUMBER, entry->revision);	
	} else {
		printf("%s: %ld\n", SVN_REPOS_DUMPFILE_REVISION_NUMBER, local_revnum);	
	}
	printf("%s: %d\n", SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH, props_length);
	printf("%s: %d\n\n", SVN_REPOS_DUMPFILE_CONTENT_LENGTH, props_length);
#endif
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


/* Sets up a delta editor for dumping a revision */
static void dump_setup_editor(svn_delta_editor_t **editor, apr_pool_t *pool)
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
}


/* Runs a diff against two revisions */
static char dump_do_diff(session_t *session, svn_revnum_t src, svn_revnum_t dest, const svn_delta_editor_t *editor, void *editor_baton, apr_pool_t *pool)
{
	const svn_ra_reporter2_t *reporter;
	void *report_baton;
	svn_error_t *err;
	apr_pool_t *subpool = svn_pool_create(pool);

	DEBUG_MSG("%s\n", session->encoded_url);
	err = svn_ra_do_diff2(session->ra, &reporter, &report_baton, dest, "", TRUE, TRUE, TRUE, session->encoded_url, editor, editor_baton, subpool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_destroy(subpool);
		return 1;
	}

	err = reporter->set_path(report_baton, "", src, TRUE, NULL, subpool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_destroy(subpool);
		return 1;
	}

	err = reporter->finish_report(report_baton, subpool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_destroy(subpool);
		return 1;
	}

	svn_pool_destroy(subpool);
	return 0;
}


/* Determines the HEAD revision of a repository */
static char dump_determine_head(session_t *session, svn_revnum_t *rev)
{
	svn_error_t *err;
	svn_dirent_t *dirent;
	apr_pool_t *pool = svn_pool_create(session->pool);

	err = svn_ra_stat(session->ra, "",  -1, &dirent, session->pool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_destroy(pool);
		return 1;
	}
	if (dirent == NULL) {
		fprintf(stderr, _("ERROR: URL '%s' not found in HEAD revision\n"), session->url);
		svn_pool_destroy(pool);
		return 1;
	}

	*rev = dirent->created_rev;
	svn_pool_destroy(pool);
	return 0;
}


/* Checks if a path is present in a given revision */
static char dump_check_path(session_t *session, const char *path, svn_revnum_t rev)
{
	svn_error_t *err;
	svn_node_kind_t kind;
	apr_pool_t *pool = svn_pool_create(session->pool);

	err = svn_ra_check_path(session->ra, path, rev, &kind, pool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_destroy(pool);
		return 1;
	}

	svn_pool_destroy(pool);
	return (kind == svn_node_none ? 1 : 0);
}


/* Fetches the UUID of a repository */
static char dump_fetch_uuid(session_t *session, const char **uuid)
{
	svn_error_t *err;
	apr_pool_t *pool = svn_pool_create(session->pool);
#if (SVN_VER_MAJOR == 1) && (SVN_VER_MINOR >= 6)
	err = svn_ra_get_uuid2(session->ra, uuid, pool);
#else
	err = svn_ra_get_uuid(session->ra, uuid, pool);
#endif
	if (err) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_destroy(pool);
		return 1;
	}
	svn_pool_destroy(pool);
	return 0;
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/


/* Creates and intializes a new dump_options_t object */
dump_options_t dump_options_create()
{
	dump_options_t opts;

	opts.temp_dir = NULL;
	opts.prefix = NULL;
	opts.verbosity = 0;
	opts.flags = 0x00;

	opts.start = 0;
	opts.end = -1; /* HEAD */

	return opts;
}


/* Frees a dump_options_t object */
void dump_options_free(dump_options_t *opts)
{
	if (opts->temp_dir != NULL) {
		free(opts->temp_dir);
	}
	if (opts->prefix != NULL) {
		free(opts->prefix);
	}
}


/* Start the dumping process, using the given session and options */
char dump(session_t *session, dump_options_t *opts)
{
	list_t logs;
	char logs_fetched = 0;
	svn_revnum_t global_rev, local_rev;
	int list_idx;

	/* First, determine or check the start and end revision */
	if (opts->end == -1) {
		if (dump_determine_head(session, &opts->end)) {
			return 1;
		}
		if (opts->start == 0) {
			if (log_get_range(session, &opts->start, &opts->end, opts->verbosity)) {
				return 1;
			}
		} else {
			/* Check if path is present in given start revision */
			if (dump_check_path(session, ".", opts->start)) {
				fprintf(stderr, _("ERROR: URL '%s' not found in revision %ld\n"), session->url, opts->start);
				return 1;
			}
		}
	} else {
		/* Check if path is present in given start revision */
		if (dump_check_path(session, ".", opts->start)) {
			fprintf(stderr, _("ERROR: URL '%s' not found in revision %ld\n"), session->url, opts->start);
			return 1;
		}
		/* Check if path is present in given end revision */
		if (dump_check_path(session, ".", opts->end)) {
			fprintf(stderr, _("ERROR: URL '%s' not found in revision %ld\n"), session->url, opts->end);
			return 1;
		}
	}

	/*
	 * Decide wether the whole repositry log should be fetched
	 * prior to dumping. This is needed if the dump is incremental and
	 * the start revision is not 0.
	 */
	if ((opts->flags & DF_INCREMENTAL) && (opts->start != 0)) {
		if (log_fetch_all(session, opts->start, opts->end, &logs, opts->verbosity)) {
			return 1;
		}
		logs_fetched = 1;
	} else {
		logs = list_create(sizeof(log_revision_t));
	}

	/* Determine end revision if neccessary */
	if (logs_fetched) {
		opts->end = ((log_revision_t *)logs.elements)[logs.size-1].revision;
	}
	else if (opts->end == -1) {
	}

	/* Determine start revision if neccessary */
	if (opts->start == 0) {
		if (strlen(session->prefix) > 0) {
			/* There arent' any subdirectories at revision 0 */
			opts->start = 1;
		}
	}

	/* Write dumpfile header */
	printf("%s: ", SVN_REPOS_DUMPFILE_MAGIC_HEADER); 
	if (opts->flags & DF_USE_DELTAS) {
		printf("3\n\n");
	} else {
		printf("2\n\n");
	}
	if (opts->flags & DF_DUMP_UUID) {
		const char *uuid;
		if (dump_fetch_uuid(session, &uuid)) {
			list_free(&logs);
			return 1;
		}
		printf("UUID: %s\n\n", uuid);
	}

	/* Pre-dumping initialization */
	global_rev = opts->start;
	local_rev = 0;
	list_idx = 0;

	/* Start dumping */
	do {
		svn_delta_editor_t *editor;
		apr_pool_t *revpool = svn_pool_create(session->pool);

		if (logs_fetched == 0) {
			log_revision_t log;
			if (log_fetch(session, global_rev, opts->end, &log, revpool)) {
				list_free(&logs);
				return 1;

			}
			list_append(&logs, &log);
			list_idx = logs.size-1;
		} else {
			++list_idx;
		}

		/* Setup the delta editor and run a diff */
		dump_setup_editor(&editor, revpool);
		if (dump_do_diff(session, global_rev, ((log_revision_t *)logs.elements)[list_idx].revision, editor, NULL, revpool)) {
			list_free(&logs);
			return 1;
		}

		++global_rev;
		++local_rev;
		apr_pool_destroy(revpool);
	} while (global_rev <= opts->end);

	return 0;
}
