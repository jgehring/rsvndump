/*
 *	rsvndump - remote svn repository dump
 *	Copyright (C) 2008 Jonas Gehring
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
 *      file: dump.h
 *      desc: The main work is done here 
 */


#ifndef DUMP_H
#define DUMP_H


#include <stdio.h>

#ifdef HAVE_CONFIG_H
 #include "config.h"
#endif


/* Bundles all information neccessary to start a dump */
typedef struct {
#if __CHAR_UNSIGNED__
	signed char verbosity;
#else
	char verbosity;
#endif
	char online;
	char *repo_url;
	char *repo_eurl;
	char *repo_base;
	char *repo_uuid;
	char *repo_dir;
	char *repo_prefix;
	char *username;
	char *password;
	char *user_prefix;
	char prefix_is_file;
	FILE *output;
	int startrev;
	int endrev;
#ifdef USE_DELTAS
	char deltas;
#endif /* USE_DELTAS */
} dump_options_t;


/* Creats default dumping options */
extern dump_options_t dump_options_create();

/* Destroys dumping options, freeing char-pointers if they are not NULL */
extern void dump_options_free(dump_options_t *opts);

/* Dumps a repository, returning 0 on success */
extern char dump(dump_options_t *opts);


#endif
