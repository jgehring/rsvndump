/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008-2010 Jonas Gehring
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
 *      file: utils.h
 *      desc: Miscellaneous utility functions
 */


#ifndef UTILS_H
#define UTILS_H


#include "main.h"

#include <apr_file_io.h>
#include <apr_pools.h>


#ifdef USE_TIMING

/* Structure for time measurement */
typedef struct {
	int mds;
} stopwatch_t;


/* Creates a stopwatch with the current time  */
extern stopwatch_t stopwatch_create();

/* Returns the number of seconds passed */
extern float stopwatch_elapsed(stopwatch_t *watch);

#endif /* USE_TIMING */

/* Returns a canonicalized path that has been allocated using strdup() */
extern char *utils_canonicalize_pstrdup(struct apr_pool_t *pool, char *path);

/* Reads a single line from a file, allocating it in pool */
extern char *utils_file_readln(struct apr_pool_t *pool, struct apr_file_t *file);

/* Recursively removes the contents of a directory and the directory */
/* itself it 'rmdir' is non-zero */
extern void utils_rrmdir(struct apr_pool_t *pool, const char *path, char rmdir);


#endif
