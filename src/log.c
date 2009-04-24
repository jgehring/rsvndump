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
 *      file: log.c
 *      desc: Convenient functions for gathering revision logs
 */


#include <svn_path.h>
#include <svn_pools.h>
#include <svn_ra.h>

#include <apr_tables.h>

#include "main.h"
#include "list.h"
#include "session.h"

#include "log.h"


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/


/* Callback for svn_ra_get_log() */
static svn_error_t *log_receiver(void *baton, apr_hash_t *changed_paths, svn_revnum_t revision, const char *author, const char *date, const char *message, apr_pool_t *pool)
{
	log_revision_t *log = (log_revision_t *)baton;

	DEBUG_MSG("log_receiver(): invoked for revision %ld\n", revision);

	log->revision = revision;
	log->author = apr_pstrdup(pool, author);
	log->date = apr_pstrdup(pool, date);
	log->message = apr_pstrdup(pool, message);
	if (changed_paths != NULL) {
		log->changed_paths = apr_hash_copy(pool, changed_paths);
	} else {
		log->changed_paths = NULL;
	}

	return SVN_NO_ERROR;
}


/* Callback for svn_ra_get_log() */
static svn_error_t *log_receiver_list(void *baton, apr_hash_t *changed_paths, svn_revnum_t revision, const char *author, const char *date, const char *message, apr_pool_t *pool)
{
	list_t *list = (list_t *)baton;
	log_revision_t log;

	log_receiver(&log, changed_paths, revision, author, date, message, pool);
	list_append(list, &log);

	return SVN_NO_ERROR;
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/

/* Fetches a single revision log */
char log_fetch(session_t *session, svn_revnum_t rev, svn_revnum_t end, log_revision_t *log, apr_pool_t *pool)
{
	svn_error_t *err;
	apr_array_header_t *paths;
	apr_pool_t *subpool;

	/* We just need the root */
	subpool = svn_pool_create(pool);
	paths = apr_array_make(subpool, 1, sizeof (const char *));
	APR_ARRAY_PUSH(paths, const char *) = svn_path_canonicalize(".", subpool);

	if ((err = svn_ra_get_log(session->ra, paths, rev, end, 1, TRUE, FALSE, log_receiver, log, pool))) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_destroy(subpool);
		return 1;
	}

	svn_pool_destroy(subpool);
	return 0;
}


/* Fetches all revision logs for a given revision range */
char log_fetch_all(session_t *session, svn_revnum_t start, svn_revnum_t end, list_t *list, char verbosity)
{
	svn_error_t *err;
	apr_array_header_t *paths;
	apr_pool_t *pool;

	*list = list_create(sizeof(log_revision_t));

	/* We just need the root */
	pool = svn_pool_create(session->pool);
	paths = apr_array_make(pool, 1, sizeof (const char *));
	APR_ARRAY_PUSH(paths, const char *) = svn_path_canonicalize(".", pool);

	if (verbosity > 0) {
		fprintf(stderr, _("Fetching logs... "));
	}

	if ((err = svn_ra_get_log(session->ra, paths, start, end, 0, TRUE, FALSE, log_receiver_list, list, pool))) {
		if (verbosity > 0) {
			fprintf(stderr, "\n");
		}
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		list_free((list_t *)list);
		svn_error_clear(err);
		svn_pool_destroy(pool);
		return 1;
	}

	if (verbosity > 0) {
		fprintf(stderr, _("done\n"));
	}

	svn_pool_destroy(pool);
	return 0;
}
