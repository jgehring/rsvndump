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
 *      file: utils.c
 *      desc: Miscellaneous utility functions
 */


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <apr_file_info.h>
#include <apr_file_io.h>
#include <apr_strings.h>

#include <svn_path.h>
#include <svn_pools.h>

#include "utils.h"

#ifdef USE_TIMING

 #ifdef TIME_WITH_SYS_TIME
  #include <sys/time.h>
 #endif
 #include <time.h>
 #ifdef WIN32
  #include <windows.h>
 #else
  #include <sys/time.h>
 #endif


/*---------------------------------------------------------------------------*/
/* Local definitions                                                         */
/*---------------------------------------------------------------------------*/


#define MSECS_PER_HOUR 3600000
#define MSECS_PER_MIN 60000


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/


/* Starts the watch */
stopwatch_t stopwatch_create()
{
	/* NOTE: This is from the Qt 4.4 sources (QTime) */
	stopwatch_t watch;
#ifdef WIN32
	SYSTEMTIME st;
	memset(&st, 0, sizeof(SYSTEMTIME));
	GetLocalTime(&st);
	watch.mds = MSECS_PER_HOUR * st.wHour + MSECS_PER_MIN * st.wMinute + 1000 * st.wSecond + st.wMilliseconds;
#else
	struct timeval tv;
	time_t ltime;
	struct tm *t;
	gettimeofday(&tv, NULL);
	ltime = tv.tv_sec;
	t = localtime(&ltime);
	watch.mds = MSECS_PER_HOUR * t->tm_hour + MSECS_PER_MIN * t->tm_min + 1000 * t->tm_sec + tv.tv_usec / 1000;
#endif
	return watch;
}


/* Returns the number of milliseconds passed */
float stopwatch_elapsed(stopwatch_t *watch)
{
	stopwatch_t tw = stopwatch_create();
	int d = tw.mds - watch->mds;
	if (d < 0) {	/* passed midnight */
		d += 86400 * 1000;
	}
	return ((float)d)/1000;
}

#endif /* USE_TIMING */

/* Calls vfprintf on stderr using the given arguments */
int utils_debug(const char *format, ...)
{
#ifndef DEBUG
	(void)format;
	return 0;
#else
	int res;
	va_list argptr;
	va_start(argptr, format);
	res = vfprintf(stderr, format, argptr);
	va_end(argptr);
	return res;
#endif
}


/* Returns a canonicalized path that has been allocated in the given pool */
char *utils_canonicalize_pstrdup(struct apr_pool_t *pool, char *path)
{
	apr_pool_t *subpool = svn_pool_create((apr_pool_t *)pool);
	const char *e = svn_path_canonicalize(path, subpool);
	char *ed = apr_pstrdup(pool, e);
	svn_pool_destroy(subpool);
	return ed;
}


/* Recursively removes the contents of a directory and the directory */
/* itself if 'remove_dir' is non-zero */
void utils_rrmdir(struct apr_pool_t *pool, const char *path, char remove_dir)
{
	apr_pool_t *subpool = svn_pool_create((apr_pool_t *)pool);
	apr_dir_t *dir = NULL;

	if (apr_dir_open(&dir, path, subpool) == APR_SUCCESS) {
		apr_finfo_t *info = apr_palloc(subpool, sizeof(apr_finfo_t));
		while (apr_dir_read(info, APR_FINFO_NAME | APR_FINFO_TYPE, dir) == APR_SUCCESS) {
			if (strcmp(info->name, ".") && strcmp(info->name, "..")) {
				char *fpath = apr_psprintf(subpool, "%s/%s", path, info->name);
				if ((info->filetype == APR_DIR) && (info->filetype != APR_LNK)) {
					utils_rrmdir(subpool, fpath, 1);
				} else {
					apr_file_remove(fpath, subpool);
				}
			}
		}

		apr_dir_close(dir);
		if (remove_dir) {
			apr_dir_remove(path, subpool);
		}
	}

	svn_pool_destroy(subpool);
}
