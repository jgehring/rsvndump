/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008-2011 Jonas Gehring
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
 *      file: main.h
 *      desc: Common definitions and types
 */


#ifndef MAIN_H_
#define MAIN_H_


/* Application-specific constants */
#ifndef HAVE_CONFIG_H
	#define PACKAGE "rsvndump"
	#define PACKAGE_VERSION "0.5.5"
	#define PACKAGE_BUGREPORT "jonas.gehring@boolsoft.org"

	/* Internationalization */
	#define _(String) (String)
	#define N_(String) String
	#define textdomain(Domain)
	#define bindtextdomain(Package, Directory)
#else /* !HAVE_CONFIG_H */
	#include "config.h"

	/* Compability functions */
	#ifndef HAVE_MEMSET
		extern void *memset(void *str, int c, size_t len);
	#endif
	#ifndef HAVE_MEMCMP
		extern int *memcmp(const void *b1, const void *b2, size_t len);
	#endif
	#ifndef HAVE_STRDUP
		extern char *strdup(const char *s);
	#endif

	/* Internationalization */
	#if ENABLE_NLS
		#include "../lib/gettext.h"
		#include <locale.h>
		#define _(String) gettext (String)
		#define gettext_noop(String) String
		#define N_(String) gettext_noop (String)
	#else /* ENABLE_NLS */
		#define _(String) (String)
		#define N_(String) String
		#define textdomain(Domain)
		#define bindtextdomain(Package, Directory)
	#endif /* ENABLE_NLS */
#endif /* !HAVE_CONFIG_H */

#define PACKAGE_AUTHOR "Jonas Gehring <"PACKAGE_BUGREPORT">"


/* Helper definitions */
#define PROPS_END "PROPS-END\n"
#define PROPS_END_LEN (sizeof(PROPS_END)-1)

/* Other features, some used for debugging */
#if defined(USE_TIMING) && !defined(HAVE_GETTIMEOFDAY)
	#undef USE_TIMING
#endif

extern void utils_handle_error(svn_error_t *error, FILE *stream, svn_boolean_t fatal, const char *prefix);
#ifndef DEBUG
	#undef DUMP_DEBUG
#endif

/* This enables dumping of single files (not working yet) */
/*#define USE_SINGLEFILE_DUMP*/

#endif
