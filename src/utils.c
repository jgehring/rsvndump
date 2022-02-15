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
 *      file: utils.c
 *      desc: Miscellaneous utility functions
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <apr_file_info.h>
#include <apr_portable.h>
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
	/*
	 * On Windows, passing stderr to Subversion functions (e.g., svn_handle_error2())
	 * is not that easy. Note that it is possible, but it's too much hassle for now.
	 * Therefore, a custom error handler is used. The implementation is close to svn_handle_error2().
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
				fprintf(stderr, "%s%s\n", prefix, tmp_err->message);
			} else {
				if ((tmp_err->apr_err > APR_OS_START_USEERR) && (tmp_err->apr_err <= APR_OS_START_CANONERR)) {
					err_string = svn_strerror(tmp_err->apr_err, errbuf, sizeof(errbuf));
				} else if ((tmp_err2 = svn_utf_cstring_from_utf8(&err_string, apr_strerror(tmp_err->apr_err, errbuf, sizeof(errbuf)), tmp_err->pool))) {
					svn_error_clear(tmp_err2);
					err_string = "Can't recode error string from APR";
				}
				fprintf(stderr, "%s%s\n", prefix, err_string);

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
}


/* Creates a temporary file with a multi-directory template */
int utils_mkstemp(apr_file_t **file, char *name, apr_pool_t *pool)
{
	apr_status_t status;
	char *dir;

	/* Create intermediate directories if needed */
	dir = svn_path_dirname(name, pool);
	status = apr_dir_make_recursive(dir, APR_UREAD | APR_UWRITE | APR_UEXECUTE, pool);
	if (status != APR_SUCCESS) {
		return status;
	}

	return apr_file_mktemp(file, name, APR_CREATE | APR_READ | APR_WRITE | APR_EXCL | APR_BINARY, pool);
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


/* Path splitting, without canonicalization */
void utils_path_split(struct apr_pool_t *pool, const char *path, const char **dir, const char **base)
{
	char *sep;

	if ((sep = strrchr(path, '/')) == NULL) {
		*dir = apr_pstrdup(pool, "");
		if (path[0] == '\0') {
			*base = apr_pstrdup(pool, "");
		} else {
			*base = apr_pstrdup(pool, path);
		}
	} else {
		if (!strcmp(path, "/")) {
			*dir = apr_pstrdup(pool, "/");
			*base = apr_pstrdup(pool, "/");
		} else {
			*dir = apr_pstrndup(pool, path, (sep == path ? 1 : sep - path));
			*base = apr_pstrdup(pool, sep + 1);
		}
	}
}


/* Path joining, without canonicalization */
const char *utils_path_join(struct apr_pool_t *pool, const char *dir, const char *base)
{
	if (dir[strlen(dir)-1] == '/' || *base == '/') {
		return apr_psprintf(pool, "%s%s", dir, base);
	}
	return apr_psprintf(pool, "%s/%s", dir, base);
}


static int compstrp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char * const *)b); }

/* qsort() wrapper for an array of strings */
void utils_sort(apr_array_header_t *a)
{
	qsort(a->elts, a->nelts, a->elt_size, compstrp);
}


/* bsearch() wrapper for an array of strings */
char *utils_search(const char *s, apr_array_header_t *a)
{
	return bsearch(&s, a->elts, a->nelts, a->elt_size, compstrp);
}
