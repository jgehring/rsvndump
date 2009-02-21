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
 *      file: wsvn.h
 *      desc: Wrapper functions for Subversion API 
 */


#ifndef WSVN_H 
#define WSVN_H


#include <stdio.h>

#include <svn_types.h>

#include "dump.h"
#include "list.h"
#include "logentry.h"
#include "node.h"


#define HEAD_REVISION (-1)


/* Initializes the Subversion API */
extern char wsvn_init(dump_options_t *opts);

/* Frees all resources of the Subversion API */
extern void wsvn_free();

/* Encodes an uri */
extern char *wsvn_uri_encode(const char *path);

/* Retrieves a brief changelog of the revision after the given one */
extern char wsvn_next_log(logentry_t *current, logentry_t *next);

/* Stats a repository node */
extern char wsvn_stat(node_t *node, svn_revnum_t rev); 

/* Writes the contents of a repository node to a given stream */
extern char wsvn_cat(node_t *node, svn_revnum_t rev, FILE *output);

/* Fetches all properties of a node */
extern char wsvn_get_props(node_t *node, list_t *list, svn_revnum_t rev);

/* Fetches all properties of a working copy node */
extern char wsvn_get_wc_props(node_t *node, list_t *list);

/* Recursively lists the contents of a directory */
extern char wsvn_find(node_t *node, list_t *list, svn_revnum_t rev);

/* Returns a changeset as a dynamic list of node_ts */
extern char wsvn_get_changeset(logentry_t *entry, list_t *list);

/* Fetches additional information about a repository */
extern char wsvn_repo_info(char *path, char **url, char **prefix, char **uuid, svn_revnum_t *headrev);

/* Updates the repository to the specified revision */
extern char wsvn_update(svn_revnum_t rev);


#endif
