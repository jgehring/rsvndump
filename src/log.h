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
 *      file: log.h
 *      desc: Convenient functions for gathering revision logs
 */


#ifndef LOG_H_
#define LOG_H_


#include <svn_types.h>

#include <apr_hash.h>
#include <apr_pools.h>

#include "list.h"
#include "session.h"


/* Revision log structure */
typedef struct {
	svn_revnum_t		revision;
	const char		*author;
	const char		*date;
	const char		*message;
	apr_hash_t		*changed_paths;
} log_revision_t;


/* Determines the first and last revision of the session root */
extern char log_get_range(session_t *session, svn_revnum_t *start, svn_revnum_t *end);

/* Fetches a single revision log */
extern char log_fetch_single(session_t *session, svn_revnum_t rev, svn_revnum_t end, log_revision_t *log, apr_pool_t *pool);

/* Fetches all revision logs for a given revision range */
extern char log_fetch_all(session_t *session, svn_revnum_t start, svn_revnum_t end, list_t *list);


#endif
