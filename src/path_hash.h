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
 *      file: path_hash.h
 *      desc: A global hash that stores paths of specific revisions
 */


#ifndef PATH_HASH_H_
#define PATH_HASH_H_


#include <svn_pools.h>

#include "log.h"


/* Initializes the path hash using the given pool */
extern void path_hash_initialize(apr_pool_t *parent_pool);

/* Adds a new revision to the path hash */
extern void path_hash_commit(log_revision_t *log, svn_revnum_t revnum);

/* Checks the parent relation of two paths at a given revision */
extern char path_hash_check_parent(const char *parent, const char *child, svn_revnum_t revision, apr_pool_t *pool);


#endif
