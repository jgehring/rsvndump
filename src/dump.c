/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008-2011 Jonas Gehring
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
#include <svn_ra.h>
#include <svn_repos.h>

#include <apr_hash.h>
#include <apr_pools.h>

#include "main.h"
#include "delta.h"
#include "log.h"
#include "logger.h"
#include "path_hash.h"
#include "property.h"

#include "dump.h"


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/


/* Dumps a revision header using the given properties */
static void dump_revision_header(apr_pool_t *pool, log_revision_t *revision, svn_revnum_t local_revnum, dump_options_t *opts)
{
	int props_length = 0;

	/* Determine length of revision properties */
	if (revision->message != NULL) {
		props_length += property_strlen(pool, "svn:log", revision->message);
	}
	if (revision->author != NULL) {
		props_length += property_strlen(pool, "svn:author", revision->author);
	}
	if (revision->date != NULL) {
		props_length += property_strlen(pool, "svn:date", revision->date);
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
static void dump_padding_revision(apr_pool_t *pool, svn_revnum_t rev)
{
	int props_length = 0;
	const char *message = "This is an empty revision for padding.";

	props_length += property_strlen(pool, "svn:log", message);
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
static char dump_do_diff(session_t *session, dump_options_t *opts, svn_revnum_t src, svn_revnum_t dest, int start_empty, const svn_delta_editor_t *editor, void *editor_baton, apr_pool_t *pool)
{
	const svn_ra_reporter2_t *reporter;
	void *report_baton;
	svn_error_t *err;
	apr_pool_t *subpool = svn_pool_create(pool);
#ifdef USE_TIMING
	stopwatch_t watch = stopwatch_create();
#endif

	DEBUG_MSG("diffing %d against %d (start_empty = %d)\n", dest, src, start_empty);
#ifdef USE_SINGLEFILE_DUMP
	err = svn_ra_do_diff2(session->ra, &reporter, &report_baton, dest, (session->file ? session->file : ""), TRUE, TRUE, TRUE, session->encoded_url, editor, editor_baton, subpool);
#else
	err = svn_ra_do_diff2(session->ra, &reporter, &report_baton, dest, "", TRUE, TRUE, !(opts->flags & DF_DRY_RUN), session->encoded_url, editor, editor_baton, subpool);
#endif
	if (err) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		svn_pool_destroy(subpool);
		return 1;
	}

	err = reporter->set_path(report_baton, "", src, start_empty, NULL, subpool);
	if (err) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		svn_pool_destroy(subpool);
		return 1;
	}

	err = reporter->finish_report(report_baton, subpool);
	if (err) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
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
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
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
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
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
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
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
	opts.flags = 0x00;
	opts.dump_format = 2;

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
	apr_array_header_t *logs = NULL;
	char logs_fetched = 0, ret = 0;
	char start_mid = 0, show_local_rev = 1;
	svn_revnum_t global_rev, local_rev = -1;
	int list_idx;

	/* Dumping with deltas requires dump format version 3 */
	if (opts->flags & DF_USE_DELTAS) {
		opts->dump_format = 3;
	}

	/*
	 * If start_mid is set, it is assumed we start somewhere (not at the beginning)
	 * of the history and don't need information about prior revisions inside
	 * the dump.
	 */
	if ((opts->flags & DF_INCREMENTAL) && (opts->start != 0)) {
		start_mid = 1;
	}

	/* Determine the correct revision range */
	DEBUG_MSG("initial range: %ld:%ld\n", opts->start, opts->end);
	if (dump_determine_end(session, &opts->end)) {
		return 1;
	}
	if ((opts->start == 0) && (strlen(session->prefix) > 0)) {
		if (log_get_range(session, &opts->start, &opts->end)) {
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
	 * will not work.
	 */
	if (session_check_reparent(session, opts->start)) {
		return 1;
	}

	logs = apr_array_make(session->pool, 0, sizeof(log_revision_t));
	/*
	 * delta_check_copy() assumes list indexes and local revisions to be equal,
	 * so insert a empty revision '0' if a subdirectory is being dumped
	 */
	if (strlen(session->prefix) > 0) {
		log_revision_t dummy;
		dummy.revision = 0;
		dummy.author = NULL;
		dummy.date = NULL;
		dummy.message = NULL;
		dummy.changed_paths = NULL;
		APR_ARRAY_PUSH(logs, log_revision_t) = dummy;
	}

	path_hash_initialize(session->prefix, opts->temp_dir, session->pool);
	if (property_store_init(opts->temp_dir, session->pool) != 0) {
		return 1;
	}

	/*
	 * Decide whether the whole repository log should be fetched
	 * prior to dumping.
	 */
	if (start_mid) {
		if (log_fetch_all(session, 0, opts->end, logs)) {
			return 1;
		}
		logs_fetched = 1;

		/* Jump to local revision and fill the path hash for previous revisions */
		L2(_("Preparing path hash... "));
		local_rev = 0;
		while ((local_rev < (long int)logs->nelts) && (APR_ARRAY_IDX(logs, local_rev, log_revision_t).revision < opts->start)) {
			svn_revnum_t phrev = ((opts->flags & DF_KEEP_REVNUMS) ? APR_ARRAY_IDX(logs, local_rev, log_revision_t).revision : local_rev);
			if (path_hash_commit(session, opts, &APR_ARRAY_IDX(logs, local_rev, log_revision_t), phrev, logs)) {
				return 1;
			}
			++local_rev;
		}
		L2(_("done\n"));

		/* The first revision is a dry run.
		   This is because we need to get the data of the previous
		   revision first in order to properly apply the received deltas. */
		opts->flags |= DF_INITIAL_DRY_RUN;
		if (strlen(session->prefix) == 0) {
			--local_rev;
		}
		opts->start = APR_ARRAY_IDX(logs, local_rev, log_revision_t).revision;
	} else {
		/* There aren't any subdirectories at revision 0 */
		if ((strlen(session->prefix) > 0) && opts->start == 0) {
			opts->start = 1;
		}
	}

	/* Write dumpfile header */
	if (!(opts->flags & DF_NO_INCREMENTAL_HEADER) || !start_mid) {
		printf("%s: %d\n\n", SVN_REPOS_DUMPFILE_MAGIC_HEADER, opts->dump_format);
		if ((opts->prefix == NULL) && (strlen(session->prefix) == 0)) {
			const char *uuid;
			if (dump_fetch_uuid(session, &uuid)) {
				return 1;
			}
			printf("UUID: %s\n\n", uuid);
		}
	}

	/* Determine end revision if neccessary */
	if (logs_fetched) {
		opts->end = APR_ARRAY_IDX(logs, logs->nelts-1, log_revision_t).revision;
		DEBUG_MSG("logs_fetched, opts->end set to %ld\n", opts->end);
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
		}
	}
	DEBUG_MSG("start_mid = %d, list_idx = %d\n", start_mid, list_idx);

	if ((opts->flags & DF_KEEP_REVNUMS) || ((strlen(session->prefix) == 0) && (opts->start == 0))) {
		show_local_rev = 0;
	}

	/* Start dumping */
	do {
		svn_delta_editor_t *editor;
		void *editor_baton;
		svn_revnum_t diff_rev;
		apr_pool_t *revpool = svn_pool_create(session->pool);

		DEBUG_MSG("dump loop start: local_rev = %ld, global_rev = %ld, list_idx = %d\n", local_rev, global_rev, list_idx);

		if (logs_fetched == 0) {
			log_revision_t log;
			L2(_("Fetching log for original revision %ld... "), global_rev);
			if (log_fetch_single(session, global_rev, opts->end, &log, revpool)) {
				ret = 1;
				L2(_("failed\n"));
				break;
			}
			APR_ARRAY_PUSH(logs, log_revision_t) = log;
			list_idx = logs->nelts-1;
			L2(_("done\n"));
		} else {
			++list_idx;
		}

		if ((opts->flags & DF_KEEP_REVNUMS) && !(opts->flags & DF_INITIAL_DRY_RUN)) {
			/* Padd with empty revisions if neccessary */
			while (local_rev < APR_ARRAY_IDX(logs, list_idx, log_revision_t).revision) {
				dump_padding_revision(revpool, local_rev);
				if (loglevel == 0) {
					L0(_("* Padded revision %ld.\n"), local_rev);
				} else if (loglevel > 0) {
					L1(_("------ Padded revision %ld <<<\n\n"), local_rev);
				}
				/* The first revision sets up the user prefix */
				if (local_rev == 1) {
					dump_create_user_prefix(opts, session->pool);
				}

				if (path_hash_commit_padding()) {
					ret = 1;
					break;
				}
				++local_rev;
			}

			if (ret != 0) {
				break;
			}
		}

		/* Dump the revision header */
		if (!(opts->flags & DF_INITIAL_DRY_RUN)) {
			dump_revision_header(revpool, &APR_ARRAY_IDX(logs, list_idx, log_revision_t), local_rev, opts);

			/* The first revision sets up the user prefix */
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

		if (!(opts->flags & DF_INITIAL_DRY_RUN)) {
			L1(_(">>> Dumping new revision, based on original revision %ld\n"), APR_ARRAY_IDX(logs, list_idx, log_revision_t).revision);
		} else {
			L1(_("Fetching base revision... "));
		}

		/* Setup the delta editor and run a diff */
		delta_setup_editor(session, opts, logs, &APR_ARRAY_IDX(logs, list_idx, log_revision_t), local_rev, &editor, &editor_baton, revpool);
		if (dump_do_diff(session, opts, diff_rev, APR_ARRAY_IDX(logs, list_idx, log_revision_t).revision, (global_rev == opts->start), editor, editor_baton, revpool)) {
			ret = 1;
			break;
		}

		/* Insert revision into path_hash */
		if (!(opts->flags & DF_INITIAL_DRY_RUN) || strlen(session->prefix) != 0) {
			if (path_hash_commit(session, opts, &APR_ARRAY_IDX(logs, list_idx, log_revision_t), local_rev, logs)) {
				ret = 1;
				break;
			}
#ifdef DEBUG_PHASH
			if (path_hash_verify(session, local_rev, APR_ARRAY_IDX(logs, list_idx, log_revision_t).revision) != 0) {
				ret = 1;
				break;
			}
#endif
		}

		if (loglevel == 0 && !(opts->flags & DF_INITIAL_DRY_RUN)) {
			if (show_local_rev) {
				L0(_("* Dumped revision %ld (local %ld).\n"), APR_ARRAY_IDX(logs, list_idx, log_revision_t).revision, local_rev);
			} else {
				L0(_("* Dumped revision %ld.\n"), APR_ARRAY_IDX(logs, list_idx, log_revision_t).revision);
			}
		} else if (loglevel > 0) {
			if (!(opts->flags & DF_INITIAL_DRY_RUN)) {
				L1(_("\n------ Dumped revision %ld <<<\n\n"), local_rev);
			} else {
				L1(_("done\n"));
			}
		}

		global_rev = APR_ARRAY_IDX(logs, list_idx, log_revision_t).revision+1;
		++local_rev;

		/* Make sure no other revisions then the first one
		   are dumped dry */
		opts->flags &= ~DF_INITIAL_DRY_RUN;

		apr_pool_destroy(revpool);
	} while (global_rev <= opts->end);

	delta_cleanup();
	return ret;
}
