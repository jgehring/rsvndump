/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008-present Jonas Gehring
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
 *      file: delta.h
 *      desc: The delta editor
 */


#ifndef DELTA_H_
#define DELTA_H_


#include <svn_types.h>

#include <apr_tables.h>

#include "dump.h"
#include "log.h"
#include "session.h"


/* Bundles information passed to delta_setup_editor() */
typedef struct {
	session_t *session;
	dump_options_t *options;
	struct path_repo_t *path_repo;
	struct property_storage_t *property_storage;
	apr_array_header_t *logs;
} delta_editor_info_t;


/* Determines the local copyfrom_path (returns NULL if it can't be reached) */
const char *delta_get_local_copyfrom_path(const char *prefix, const char *path);

/* Determines the local copyfrom_revision number */
svn_revnum_t delta_get_local_copyfrom_rev(svn_revnum_t original, dump_options_t *opts, apr_array_header_t *logs, svn_revnum_t local_revnum);

/* Sets up a delta editor for dumping a revision */
extern void delta_setup_editor(delta_editor_info_t *info, log_revision_t *log_revision, svn_revnum_t local_revnum, svn_delta_editor_t **editor, void **editor_baton, apr_pool_t *pool);

/* Cleans up global resources */
extern void delta_cleanup();


#endif
