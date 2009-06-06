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


#include <stdio.h>

#include <svn_pools.h>
#include <svn_props.h>
#include <svn_ra.h>
#include <svn_repos.h>

#include <apr_hash.h>
#include <apr_pools.h>

#include "main.h"
#include "delta.h"
#include "list.h"
#include "log.h"
#include "property.h"
#include "utils.h"

#include "dump.h"


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/


/* Dumps a revision header using the given properties */
static void dump_revision_header(log_revision_t *revision, svn_revnum_t local_revnum, dump_options_t *opts)
{
	int props_length = 0;

	/* Determine length of revision properties */
	if (revision->message != NULL) {
		props_length += property_strlen("svn:log", revision->message);
	}
	if (revision->author != NULL) {
		props_length += property_strlen("svn:author", revision->author);
	}
	if (revision->date != NULL) {
		props_length += property_strlen("svn:date", revision->date);
	}
	if (props_length > 0) {
		props_length += PROPS_END_LEN;
	}

	printf("%s: %ld\n", SVN_REPOS_DUMPFILE_REVISION_NUMBER, local_revnum);
	printf("%s: %d\n", SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH, props_length);
	printf("%s: %d\n\n", SVN_REPOS_DUMPFILE_CONTENT_LENGTH, props_length);
	
	if (props_length > 0) {
		if (revision->message != NULL) {
			property_dump("svn:log", revision->message);
		}
		if (revision->author != NULL) {
			property_dump("svn:author", revision->author);
		}
		if (revision->date != NULL) {
			property_dump("svn:date", revision->date);
		}

		printf(PROPS_END"\n");
	}
}


/* Dumps an empty revision for padding the given number */
static void dump_padding_revision(svn_revnum_t rev)
{
	int props_length = 0;
	const char *message = "This is an empty revision for padding.";

	props_length += property_strlen("svn:log", message);
	props_length += PROPS_END_LEN;

	printf("%s: %ld\n", SVN_REPOS_DUMPFILE_REVISION_NUMBER, rev);
	printf("%s: %d\n", SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH, props_length);
	printf("%s: %d\n\n", SVN_REPOS_DUMPFILE_CONTENT_LENGTH, props_length);

	property_dump("svn:log", message);
	printf(PROPS_END"\n");
}


/* Creates (and possibly cleans up) the user prefix path.
   The new prefix will be allocated in the given pool. */
static void dump_create_user_prefix(dump_options_t *opts, apr_pool_t *pool)
{
	char *new_prefix, *s, *e;
	if (opts->prefix == NULL) {
		return;
	}

	new_prefix = apr_pcalloc(pool, strlen(opts->prefix)+1);
	s = e = opts->prefix;
	while ((e = strchr(s, '/')) != NULL) {
		/* Skip leading slashes */
		if (e - s < 1) {
			++s;
			continue;
		}

		/* Append to new prefix and dump */
		strncat(new_prefix, s, e - s);

		printf("%s: %s\n", SVN_REPOS_DUMPFILE_NODE_PATH, new_prefix);
		printf("%s: dir\n", SVN_REPOS_DUMPFILE_NODE_KIND);
		printf("%s: add\n\n", SVN_REPOS_DUMPFILE_NODE_ACTION);

		strcat(new_prefix, "/");
		s = e + 1;
	}

	strcat(new_prefix, s);
	opts->prefix = new_prefix;
}


/* Runs a diff against two revisions */
static char dump_do_diff(session_t *session, svn_revnum_t src, svn_revnum_t dest, char start_empty, const svn_delta_editor_t *editor, void *editor_baton, apr_pool_t *pool)
{
	const svn_ra_reporter2_t *reporter;
	void *report_baton;
	svn_error_t *err;
	apr_pool_t *subpool = svn_pool_create(pool);
#ifdef USE_TIMING
	stopwatch_t watch = stopwatch_create();
#endif

	DEBUG_MSG("diffing %d against %d (start_empty = %d)\n", dest, src, (int)start_empty);
#ifdef USE_SINGLEFILE_DUMP
	err = svn_ra_do_diff2(session->ra, &reporter, &report_baton, dest, (session->file ? session->file : ""), TRUE, TRUE, TRUE, session->encoded_url, editor, editor_baton, subpool);
#else
	err = svn_ra_do_diff2(session->ra, &reporter, &report_baton, dest, "", TRUE, TRUE, TRUE, session->encoded_url, editor, editor_baton, subpool);
#endif
	if (err) {
		svn_handle_error2(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		svn_pool_destroy(subpool);
		return 1;
	}

	err = reporter->set_path(report_baton, "", src, start_empty, NULL, subpool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		svn_pool_destroy(subpool);
		return 1;
	}

	err = reporter->finish_report(report_baton, subpool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		svn_pool_destroy(subpool);
		return 1;
	}

	svn_pool_destroy(subpool);
#ifdef USE_TIMING
	DEBUG_MSG("dump_do_diff done in %f seconds\n", stopwatch_elapsed(&watch));
#endif
	return 0;
}


/* Determines the correct end revision of a repository */
static char dump_determine_end(session_t *session, svn_revnum_t *rev)
{
	svn_error_t *err;
	svn_dirent_t *dirent;
	apr_pool_t *pool = svn_pool_create(session->pool);

	err = svn_ra_stat(session->ra, "",  *rev, &dirent, session->pool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, "ERROR: ");
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
static svn_node_kind_t dump_check_path(session_t *session, const char *path, svn_revnum_t rev)
{
	svn_error_t *err;
	svn_node_kind_t kind;
	apr_pool_t *pool = svn_pool_create(session->pool);

	err = svn_ra_check_path(session->ra, path, rev, &kind, pool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		svn_pool_destroy(pool);
		return svn_node_none;
	}

	svn_pool_destroy(pool);
	return kind;
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
		svn_handle_error2(err, stderr, FALSE, "ERROR: ");
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
	/* Nothing to do here */
}


/* Start the dumping process, using the given session and options */
char dump(session_t *session, dump_options_t *opts)
{
	list_t logs;
	char logs_fetched = 0, ret = 0;
	char start_mid = 0, show_local_rev = 1;
	svn_revnum_t global_rev, local_rev = -1;
	int list_idx;

	if ((opts->flags & DF_INCREMENTAL) && (opts->start != 0)) {
		start_mid = 1;
	}

	/* Determine the correct revision range */
	DEBUG_MSG("initial range: %ld:%ld\n", opts->start, opts->end);
	if (dump_determine_end(session, &opts->end)) {
		return 1;
	}
	if ((opts->start == 0) && (strlen(session->prefix) > 0)) {
		if (log_get_range(session, &opts->start, &opts->end, opts->verbosity)) {
			return 1;
		}
	} else {
		/* Check if path is present in given start revision */
		if (dump_check_path(session, "", opts->start) == svn_node_none) {
			fprintf(stderr, _("ERROR: URL '%s' not found in revision %ld\n"), session->url, opts->start);
			return 1;
		}
	}
	DEBUG_MSG("adjusted range: %ld:%ld\n", opts->start, opts->end);

	/*
	 * Check if we need to reparent the RA session. This is needed if we
	 * are only dumping the history of a single file. Else, svn_ra_do_diff()
	 * will nor work.
	 */
	if (session_check_reparent(session, opts->start)) {
		return 1;
	}

	/*
	 * Decide wether the whole repositry log should be fetched
	 * prior to dumping.
	 */
	if (start_mid) {
		if (log_fetch_all(session, 0, opts->end, &logs, opts->verbosity)) {
			return 1;
		}
		logs_fetched = 1;

		/* Set local revision number */
		local_rev = 0;
		while ((local_rev < logs.size) && (((log_revision_t *)logs.elements)[local_rev].revision < opts->start)) {
			++local_rev;
		}

		/* The first revision is a dry ru.n
		   This is because we need to get the data of the previous
		   revision first */
		opts->flags |= DF_DRY_RUN;
		--local_rev;
		opts->start = ((log_revision_t *)logs.elements)[local_rev].revision;
	} else {
		logs = list_create(sizeof(log_revision_t));

		/* Write dumpfile header */
		printf("%s: %d\n\n", SVN_REPOS_DUMPFILE_MAGIC_HEADER, 3);
		if ((opts->prefix == NULL) && (strlen(session->prefix) == 0)) {
			const char *uuid;
			if (dump_fetch_uuid(session, &uuid)) {
				list_free(&logs);
				return 1;
			}
			printf("UUID: %s\n\n", uuid);
		}

		/* There arent' any subdirectories at revision 0 */
		if ((strlen(session->prefix) > 0) && opts->start == 0) {
			opts->start = 1;
		}
	}

	/* Determine end revision if neccessary */
	if (logs_fetched) {
		opts->end = ((log_revision_t *)logs.elements)[logs.size-1].revision;
	}

	/* Pre-dumping initialization */
	global_rev = opts->start;
	if (!start_mid) {
		local_rev = global_rev == 0 ? 0 : 1;
		list_idx = 0;
	} else {
		list_idx = local_rev-1;
		if (opts->flags & DF_KEEP_REVNUMS) {
			local_rev = opts->start;
		} else if (strlen(session->prefix)) {
			/* Adjustment for missing revision 0 */
			++local_rev;
		}
	}

	if ((opts->flags & DF_KEEP_REVNUMS) || ((strlen(session->prefix) == 0) && (opts->start == 0))) {
		show_local_rev = 0;
	}

	/* Start dumping */
	do {
		svn_delta_editor_t *editor;
		void *editor_baton;
		svn_revnum_t diff_rev;
		apr_pool_t *revpool = svn_pool_create(session->pool);

		if (logs_fetched == 0) {
			log_revision_t log;
			if (log_fetch(session, global_rev, opts->end, &log, revpool)) {
				ret = 1;
				break;

			}
			list_append(&logs, &log);
			list_idx = logs.size-1;
		} else {
			++list_idx;
		}

		if ((opts->flags & DF_KEEP_REVNUMS) && !(opts->flags & DF_DRY_RUN)) {
			/* Padd with empty revisions if neccessary */
			while (local_rev < ((log_revision_t *)logs.elements)[list_idx].revision) {
				dump_padding_revision(local_rev);
				if (opts->verbosity == 0) {
					fprintf(stderr, _("* Padded revision %ld.\n"), local_rev);
				} else if (opts->verbosity > 0) {
					fprintf(stderr, _("------ Padded revision %ld <<<\n\n"), local_rev);
				}
				++local_rev;
			}
		}

		/* Dump the revision header */
		if (!(opts->flags & DF_DRY_RUN)) {
			dump_revision_header((log_revision_t *)logs.elements + list_idx, local_rev, opts);

			/* The first revision also sets up the user prefix */
			if (local_rev == 1) {
				dump_create_user_prefix(opts, session->pool);
			}
		}

		/* Determine the diff base */
		diff_rev = global_rev - 1;
		if (diff_rev < 0) {
			diff_rev = 0;
		}
		if (/*(strlen(session->prefix) > 0) &&*/ diff_rev < opts->start) {
#ifdef USE_SINGLEFILE_DUMP
			/* TODO: This isn't working well with single files
			 * and a revision range */
			if (session->file) {
				diff_rev = opts->end;
			} else {
				diff_rev = opts->start;
			}
#else
			diff_rev = opts->start;
#endif
		}
		DEBUG_MSG("global = %ld, diff = %ld, start = %ld\n", global_rev, diff_rev, opts->start);

		if (opts->verbosity > 0 && !(opts->flags & DF_DRY_RUN)) {
			fprintf(stderr, _(">>> Dumping new revision, based on original revision %ld\n"), ((log_revision_t *)logs.elements)[list_idx].revision);
		}

		/* Setup the delta editor and run a diff */
		delta_setup_editor(session, opts, &logs, (log_revision_t *)logs.elements + list_idx, local_rev, &editor, &editor_baton, revpool);
		if (dump_do_diff(session, diff_rev, ((log_revision_t *)logs.elements)[list_idx].revision, (global_rev == opts->start), editor, editor_baton, revpool)) {
			ret = 1;
			break;
		}

		if (opts->verbosity == 0 && !(opts->flags & DF_DRY_RUN)) {
			if (show_local_rev) {
				fprintf(stderr, _("* Dumped revision %ld (local %ld).\n"), ((log_revision_t *)logs.elements)[list_idx].revision, local_rev);
			} else {
				fprintf(stderr, _("* Dumped revision %ld.\n"), ((log_revision_t *)logs.elements)[list_idx].revision);
			}
		} else if (opts->verbosity > 0 && !(opts->flags & DF_DRY_RUN)) {
			fprintf(stderr, _("\n------ Dumped revision %ld <<<\n\n"), local_rev);
		}

		global_rev = ((log_revision_t *)logs.elements)[list_idx].revision+1;
		++local_rev;

		/* Make sure no other revisions then the first one
		   are dumped dry */
		opts->flags &= ~DF_DRY_RUN;

		apr_pool_destroy(revpool);
	} while (global_rev <= opts->end);

	list_free(&logs);
	return ret;
}
