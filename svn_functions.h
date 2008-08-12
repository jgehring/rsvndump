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


#ifndef _SVN_FUNCTIONS_H
#define _SVN_FUNCTIONS_H


#include "main.h"

#include <stdio.h>

#include <svn_io.h>


extern char svn_init();
extern void svn_free();
extern char svn_alloc_rev_pool();
extern void svn_free_rev_pool();
svn_stream_t *svn_open(char *path, int rev, char **buffer, int *len);
void svn_close(svn_stream_t *stream);
extern char svn_log(const char *path, int rev, char **author, char **logmsg, char **date);
extern list_t svn_list_changes(const char *path, int rev);
extern list_t svn_list_props(const char *path, int rev);
extern nodekind_t svn_get_kind(const char *path, int rev);
extern char svn_repo_info(const char *path, char **url, char **prefix);
extern char svn_checkout(const char *repo, const char *path, int rev);
extern char svn_update_path(const char *path, int rev);


#endif
