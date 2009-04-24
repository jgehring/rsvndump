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
 *      file: utils.h
 *      desc: Miscellaneous utility functions
 */


#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <svn_path.h>
#include <svn_pools.h>

#include "utils.h"


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/


/* Calls vfprintf on stderr using the given arguments */
int utils_debug(const char *format, ...)
{
	int res;
	va_list argptr;
	va_start(argptr, format);
	res = vfprintf(stderr, format, argptr);
	va_end(argptr);
	return res;
}


/* Returns a canonicalized path that has been allocated using strdup() */
char *utils_canonicalize_strdup(char *path)
{
	apr_pool_t *pool = svn_pool_create(NULL);
	const char *e = svn_path_canonicalize(path, pool);
	char *ed = strdup(e);
	svn_pool_clear(pool);
	svn_pool_destroy(pool);
	return ed;
}


/* Recursively removes the contents of a directory and the directory */
/* itself if 'remove_dir' is non-zero */
void utils_rrmdir(const char *path, char remove_dir)
{
	DIR *dir;
	struct dirent *entry;

	if ((dir = opendir(path)) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
				struct stat st;
				char *filename = malloc(strlen(path)+strlen(entry->d_name)+2);
				sprintf(filename, "%s/%s", path, entry->d_name);
				stat(filename, &st);
				/* Descend into other directories if they aren't symlinks */
				if ((st.st_mode & S_IFDIR) && !(st.st_mode & S_IFLNK)) {
					utils_rrmdir(filename, 1);
				} else {
					unlink(filename);
				}
				free(filename);
			}
		}

		closedir(dir);
		if (remove_dir) {
			rmdir(path);
		}
	}
}
