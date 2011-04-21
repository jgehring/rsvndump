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
 *      file: path_hash.h
 *      desc: A global hash that stores paths of specific revisions
 */


#ifndef PATH_HASH_H_
#define PATH_HASH_H_


#include <svn_pools.h>

#include "dump.h"
#include "list.h"
#include "log.h"
#include "session.h"


/* Initializes the path hash using the given pool */
extern void path_hash_initialize(const char *session_prefix, const char *temp_dir, apr_pool_t *parent_pool);

/* Manually adds a new path to the head revision (without committing it) */
extern void path_hash_add_path(const char *path);

/* Adds a new revision to the path hash */
extern char path_hash_commit(session_t *session, dump_options_t *opts, log_revision_t *log, svn_revnum_t revnum, list_t *logs);

/* Checks the parent relation of two paths at a given revision */
extern char path_hash_check_parent(const char *parent, const char *child, svn_revnum_t revision, apr_pool_t *pool);

/* Checks if a path is present at a given revision */
extern char path_hash_check(const char *path, svn_revnum_t revision, apr_pool_t *pool);

#ifdef DEBUG_PHASH
 extern void path_hash_test(session_t *session);
#endif


#endif
