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
 *      file: path_repo.h
 *      desc: Versioned storage for file paths
 */


#ifndef PATH_REPO_H_
#define PATH_REPO_H_


#include <svn_pools.h>

#include "dump.h"
#include "log.h"
#include "session.h"


typedef struct path_repo_t path_repo_t;


/* Creates a new path repository in the given directory */
extern path_repo_t *path_repo_create(const char *tmpdir, apr_pool_t *pool);

/* Schedules the given path for addition */
extern void path_repo_add(path_repo_t *repo, const char *path, apr_pool_t *pool);

/* Schedules the given path for deletion */
extern void path_repo_delete(path_repo_t *repo, const char *path, apr_pool_t *pool);

/* Commits all scheduled actions, using the given revision number */
extern int path_repo_commit(path_repo_t *repo, svn_revnum_t revision, apr_pool_t *pool);

/* Discards all scheduled actions */
extern int path_repo_discard(path_repo_t *repo, apr_pool_t *pool);

/* Commits a SVN log entry, using the given revision number */
extern int path_repo_commit_log(path_repo_t *repo, session_t *session, dump_options_t *opts, log_revision_t *log, svn_revnum_t revision, apr_array_header_t *logs, apr_pool_t *pool);

/* Checks the parent relation of two paths at a given revision */
extern signed char path_repo_check_parent(path_repo_t *repo, const char *parent, const char *child, svn_revnum_t revision, apr_pool_t *pool);

#ifdef DEBUG

extern void path_repo_test_all(path_repo_t *repo, session_t *session, apr_pool_t *pool);

#endif


#endif
