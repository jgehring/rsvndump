/*
 *	rsvndump - remote svn repository dump
 *	Copyright (C) 2008 Jonas Gehring
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along
 *	with this program; if not, write to the Free Software Foundation, Inc.,
 *	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef _MAIN_H
#define _MAIN_H


#include "list.h"

#include <stdio.h>


// Application-specific constants
#ifndef HAVE_CONFIG_H
 #define APPNAME "rsvndump"
 #define APPVERSION "0.3.2"
 #define APPAUTHOR "Jonas Gehring <jonas.gehring@boolsoft.org>"
#else
 #include "config.h"
 #define APPNAME PACKAGE
 #define APPVERSION PACKAGE_VERSION
 #define APPAUTHOR "Jonas Gehring <"PACKAGE_BUGREPORT">"
#endif

// Other constants
#define DUMPFORMAT_VERSION 2


// Enumerations
typedef enum {
	NK_NONE = -1, NK_DIRECTORY, NK_FILE
} nodekind_t;

typedef enum {
	// Ordering is important, see compare_changes() in dump.c
	NK_CHANGE, NK_ADD, NK_DELETE, NK_REPLACE
} nodeaction_t;


// Data structures
typedef struct {
	char *key, *value;
} prop_t;

typedef struct {
	char *path;
	nodekind_t kind;
	nodeaction_t action;
	char *copy_from_path;
	int copy_from_rev;
	char *localpath;
	char use_copy;	// -1: no copy, 0: explicit dump copy, 1: use copy_from_path and copy_from_rev 
} change_entry_t;


// Globals
extern char *repo_url;
extern char *repo_base;
extern char *repo_prefix;
extern char *repo_dir;
extern char *repo_uuid;
extern char *repo_username;
extern char *repo_password;
extern char *user_prefix;
extern char verbosity, online;
extern FILE *input, *output;


#endif
