/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008 Jonas Gehring
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
 *      file: node.h
 *      desc: Represents a node that has been changed in a revision
 */


#ifndef NODE_H
#define NODE_H


#include <svn_types.h>

#include "dump.h"
#include "list.h"


/* Valid node kinds */
typedef enum {
	NK_NONE = -1,
	NK_DIRECTORY,
	NK_FILE
} nodekind_t;


/* Valid node actions */
typedef enum {
	/* This order specifies the output order in the dump */
	NA_CHANGE,
	NA_ADD,
	NA_DELETE,
	NA_REPLACE
} nodeaction_t;


/* Main node structure */
typedef struct {
	char 		*path;
	nodekind_t	kind;
	nodeaction_t 	action;
	int		size;
	char		has_props;
	char 		*copy_from_path;
	svn_revnum_t	copy_from_rev;
/*	char 		*localpath; */
	/* -1: no copy, 0: explicit dump copy, 1: use copy_from_path and copy_from_rev  */
	char 		use_copy;
} node_t;


/* Creates a new node with default values */
extern node_t node_create();

/* Destroys a node, freeing char-pointers if they are not NULL */
extern void node_free(node_t *node);

/* Compares two nodes */
extern int nodecmp(const void *a, const void *b);

/* Dumps the contents of a node */
extern char node_dump(node_t *node, dump_options_t *opts, list_t *logs, svn_revnum_t revnum, svn_revnum_t local_revnum);


#endif
