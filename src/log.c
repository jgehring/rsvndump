/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008-2012 Jonas Gehring
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

#include "main.h"
#include "logger.h"

#include "log.h"


/*---------------------------------------------------------------------------*/
/* Local data structures                                                     */
/*---------------------------------------------------------------------------*/


/* A baton for log_receiver() */
typedef struct {
	log_revision_t	*log;
	session_t	*session;
	apr_pool_t	*pool;
} log_receiver_baton_t;


/* A baton for log_receiver_list() */
typedef struct {
	apr_array_header_t *list;
	session_t	*session;
	apr_pool_t	*pool;
} log_receiver_list_baton_t;


/* A baton for log_receiver_revnum() */
typedef struct {
	svn_revnum_t revnum;
} log_receiver_revnum_baton_t;


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/


/* Callback for svn_ra_get_log() */
static svn_error_t *log_receiver(void *baton, apr_hash_t *changed_paths, svn_revnum_t revision, const char *author, const char *date, const char *message, apr_pool_t *pool)
{
	apr_hash_index_t *hi;
	log_receiver_baton_t *data = (log_receiver_baton_t *)baton;
	size_t prefixlen = strlen(data->session->prefix);

	data->log->revision = revision;
	data->log->author = session_obfuscate_once(data->session, data->pool, apr_pstrdup(data->pool, author));
	data->log->date = apr_pstrdup(data->pool, date);
	data->log->message = session_obfuscate_once(data->session, data->pool, apr_pstrdup(data->pool, message));
	data->log->changed_paths = apr_hash_make(data->pool);

	DEBUG_MSG("log_receiver: got log for revision %ld\n", revision);

	/* Deep-copy the changed_paths hash */
	if (changed_paths == NULL) {
		DEBUG_MSG("changed_paths is NULL\n");
		return SVN_NO_ERROR;
	}
	for (hi = apr_hash_first(pool, changed_paths); hi; hi = apr_hash_next(hi)) {
		const char *key;
		svn_log_changed_path_t *svalue, *dvalue;
		apr_hash_this(hi, (const void **)&key, NULL, (void **)&svalue);
		key = session_obfuscate(data->session, pool, key);

		/* Skip this entry? */
		/* It needs to be skipped if the key+1 doesn't match the prefix or if it does match and the next character isn't a slash */
		if ((strlen(key) < 1) || strncmp(data->session->prefix, key+1, prefixlen) ||
			(prefixlen && strlen(key+1) > prefixlen && (key+1)[prefixlen] != '/') ) {
			DEBUG_MSG("%c %s [skipped]\n", svalue->action, key);
			continue;
		}

		dvalue = apr_palloc(data->pool, sizeof(svn_log_changed_path_t));
		dvalue->action = svalue->action;
		if (svalue->copyfrom_path != NULL) {
			if (*svalue->copyfrom_path == '/') {
				dvalue->copyfrom_path = apr_pstrdup(data->pool, svalue->copyfrom_path + 1);
			} else {
				dvalue->copyfrom_path = apr_pstrdup(data->pool, svalue->copyfrom_path);
			}
		} else {
			dvalue->copyfrom_path = NULL;
		}
		dvalue->copyfrom_path = session_obfuscate(data->session, data->pool, dvalue->copyfrom_path);
		dvalue->copyfrom_rev = svalue->copyfrom_rev;

		/* Strip the prefix (or the leading slash) from the path */
		if (*data->session->prefix != '\0') {
			key += prefixlen + 1;
		}
		if (*key == '/') {
			key += 1;
		}
		apr_hash_set(data->log->changed_paths, apr_pstrdup(data->pool, key), APR_HASH_KEY_STRING, dvalue);

		/* A little debugging */
		DEBUG_MSG("%c %s", dvalue->action, key);
		if (dvalue->copyfrom_path != NULL) {
			DEBUG_MSG(" (from %s@%ld)", dvalue->copyfrom_path, dvalue->copyfrom_rev);
		}
		DEBUG_MSG("\n");
	}

	return SVN_NO_ERROR;
}


/* Callback for svn_ra_get_log() */
static svn_error_t *log_receiver_list(void *baton, apr_hash_t *changed_paths, svn_revnum_t revision, const char *author, const char *date, const char *message, apr_pool_t *pool)
{
	log_receiver_list_baton_t *data = (log_receiver_list_baton_t *)baton;
	log_revision_t log;
	log_receiver_baton_t receiver_baton;

	receiver_baton.log = &log;
	receiver_baton.session = data->session;
	receiver_baton.pool = data->pool;
	SVN_ERR(log_receiver(&receiver_baton, changed_paths, revision, author, date, message, pool));

	APR_ARRAY_PUSH(data->list, log_revision_t) = log;

	L2("\r\033[0K%s%ld", _("Fetching logs... "), revision);
	if (loglevel >= 2) {
		fflush(stderr);
	}
	return SVN_NO_ERROR;
}


/* Callback for svn_ra_get_log() */
static svn_error_t *log_receiver_revnum(void *baton, apr_hash_t *changed_paths, svn_revnum_t revision, const char *author, const char *date, const char *message, apr_pool_t *pool)
{
	log_receiver_revnum_baton_t *data = (log_receiver_revnum_baton_t *)baton;
	data->revnum = revision;
	return SVN_NO_ERROR;
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/


/* Determines the first and last revision of the session root */
char log_get_range(session_t *session, svn_revnum_t *start, svn_revnum_t *end)
{
	svn_error_t *err;
	apr_array_header_t *paths;
	apr_pool_t *subpool;
	log_receiver_revnum_baton_t baton;

	/* We just need the root */
	subpool = svn_pool_create(session->pool);
	paths = apr_array_make(subpool, 1, sizeof (const char *));
	APR_ARRAY_PUSH(paths, const char *) = svn_path_canonicalize(".", subpool);

	L1(_("Determining start and end revision... "));
	if ((err = svn_ra_get_log(session->ra, paths, *start, *end, 1, FALSE, TRUE, log_receiver_revnum, &baton, subpool))) {
		L1(_("failed\n"));
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		svn_pool_destroy(subpool);
		return 1;
	}
	*start = baton.revnum;

	if ((err = svn_ra_get_log(session->ra, paths, *end, *start, 1, FALSE, TRUE, log_receiver_revnum, &baton, subpool))) {
		L1(_("failed\n"));
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		svn_pool_destroy(subpool);
		return 1;
	}
	*end = baton.revnum;
	L1(_("done\n"));

	svn_pool_destroy(subpool);
	return 0;
}


/* Fetches a single revision log */
char log_fetch_single(session_t *session, svn_revnum_t rev, svn_revnum_t end, log_revision_t *log, apr_pool_t *pool)
{
	svn_error_t *err;
	apr_array_header_t *paths;
	apr_pool_t *subpool;
	log_receiver_baton_t baton;

	/* We just need the root */
	subpool = svn_pool_create(pool);
	paths = apr_array_make(subpool, 1, sizeof (const char *));
	APR_ARRAY_PUSH(paths, const char *) = svn_path_canonicalize(".", subpool);

	baton.log = log;
	baton.session = session;
	baton.pool = pool;

	if ((err = svn_ra_get_log(session->ra, paths, rev, end, 1, TRUE, TRUE, log_receiver, &baton, subpool))) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		svn_pool_destroy(subpool);
		return 1;
	}

	svn_pool_destroy(subpool);
	return 0;
}


/* Fetches all revision logs for a given revision range */
char log_fetch_all(session_t *session, svn_revnum_t start, svn_revnum_t end, apr_array_header_t *list)
{
	svn_error_t *err;
	apr_array_header_t *paths;
	apr_pool_t *pool;
	log_receiver_list_baton_t baton;

	/* We just need the root */
	pool = svn_pool_create(session->pool);
	paths = apr_array_make(pool, 1, sizeof (const char *));
	APR_ARRAY_PUSH(paths, const char *) = svn_path_canonicalize(".", pool);

	baton.list = list;
	baton.session = session;
	baton.pool = session->pool;

	L1(_("Fetching logs... "));
	if ((err = svn_ra_get_log(session->ra, paths, start, end, 0, TRUE, TRUE, log_receiver_list, &baton, pool))) {
		L1(_("failed\n"));
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		svn_pool_destroy(pool);
		return 1;
	}
	if (loglevel == 2) {
		L2("\r\033[0K%s%s", _("Fetching logs... "), _("done\n"));
	} else {
		L1(_("done\n"));
	}

	svn_pool_destroy(pool);
	return 0;
}
