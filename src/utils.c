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

#include <svn_error.h>
#include <svn_path.h>
#include <svn_pools.h>
#include <svn_utf.h>

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


/* Error handling proxy function */
void utils_handle_error(svn_error_t *error, FILE *stream, svn_boolean_t fatal, const char *prefix)
{
#ifndef WIN32
	svn_handle_error2(error, stream, fatal, prefix);
#else /* !WIN32 */
	/*
	 * When running under Windows, a custom error handler (very similar to the one
	 * from Subversion) is used, because passing stderr to Subversion functions
	 * is a little problematic. Note that it is possible, but it's too much hassle
	 * for now.
	 */
	apr_pool_t *subpool;
	svn_error_t *tmp_err;
	apr_array_header_t *empties;

	apr_pool_create(&subpool, error->pool);
	empties = apr_array_make(subpool, 0, sizeof(apr_status_t));

	tmp_err = error;
	while (tmp_err) {
		int i;
		svn_boolean_t printed_already = FALSE;

		if (!tmp_err->message) {
			for (i = 0; i < empties->nelts; i++) {
				if (tmp_err->apr_err == APR_ARRAY_IDX(empties, i, apr_status_t)) {
					printed_already = TRUE;
					break;
				}
			}
		}

		if (!printed_already) {
			char errbuf[256];
			const char *err_string;
			svn_error_t *tmp_err2 = NULL;

			if (tmp_err->message) {
				fprintf(stderr,"%s%s\n", prefix, tmp_err->message);
				svn_error_clear(tmp_err);
			} else {
				if ((tmp_err->apr_err > APR_OS_START_USEERR) && (tmp_err->apr_err <= APR_OS_START_CANONERR)) {
					err_string = svn_strerror(tmp_err->apr_err, errbuf, sizeof(errbuf));
				} else if ((tmp_err2 = svn_utf_cstring_from_utf8(&err_string, apr_strerror(tmp_err->apr_err, errbuf, sizeof(errbuf)), tmp_err->pool))) {
					svn_error_clear(tmp_err2);
					err_string = "Can't recode error string from APR";
				}
				fprintf(stderr,"%s%s\n", prefix, tmp_err->message);
				svn_error_clear(tmp_err);

				APR_ARRAY_PUSH(empties, apr_status_t) = tmp_err->apr_err;
			}
		}

		tmp_err = tmp_err->child;
	}

	apr_pool_destroy(subpool);
	fflush(stream);

	if (fatal) {
		svn_error_clear(error);
		exit(EXIT_FAILURE);
	}
#endif /* !WIN32 */
}


/* Reads a single line from a file, allocating it in pool */
char *utils_file_readln(struct apr_pool_t *pool, struct apr_file_t *file)
{
	char buffer[512];
	char *dest = NULL;

	while (1) {
		char *bptr = buffer;
		while ((bptr - buffer) < 511 && (bptr == buffer || *(bptr-1) != '\n')) {
			if (apr_file_getc(bptr++, file)) {
				if (bptr == buffer+1) {
					return NULL;
				} else {
					*(bptr-1) = '\n';
				}
				break;
			}
		}
		*bptr = '\0';

		if (dest == NULL) {
			dest = apr_pstrdup(pool, buffer);
		} else {
			dest = apr_pstrcat(pool, dest, buffer);
		}

		if (*(bptr-1) == '\n') {
			break;
		}
	}

	if (dest && strlen(dest) > 0) {
		dest[strlen(dest)-1] = '\0'; /* Remove newline character */
	}
	return dest;
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
