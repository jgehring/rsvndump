/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008-2012 Jonas Gehring
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
 *      file: main.c
 *      desc: Program initialization and option parsing
 */


#ifdef WIN32
 #include <io.h>
 #include <fcntl.h>
#else
 #include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <svn_cmdline.h>
#include <svn_path.h>

#include "main.h"
#include "dump.h"
#include "logger.h"
#include "utils.h"


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/

/* Prints the program version */
static void print_version()
{
	printf(PACKAGE" "PACKAGE_VERSION"\n");
	printf("Copyright (C) 2008-2011 "PACKAGE_AUTHOR"\n");
	printf(_("Released under the GNU General Public License.\n"));
}


/* Prints usage information */
static void print_usage()
{
	print_version();
	printf("\n");
	printf(_("USAGE:"));
	printf(" "PACKAGE" ");
	printf(_("[options] <url>\n\n"));
	printf(_("Common options:\n"));
	printf(_("    -h [--help]               print a nice help screen\n"));
	printf(_("    --version                 print the program name and version\n"));
	printf(_("    -q [--quiet]              be quiet\n"));
	printf(_("    -v [--verbose]            print extra progress\n"));
	printf(_("    -n [--dry-run]            don't fetch text deltas\n"));
	printf("\n");
	printf(_("Dump options:\n"));
	printf(_("    -r [--revision] ARG       specify revision number (or X:Y range)\n"));
	printf(_("    --deltas                  use deltas in dump output\n"));
	printf(_("    --incremental             dump incrementally\n"));
	printf(_("    --prefix ARG              prepend ARG to the path that is being dumped\n"));
	printf(_("    --keep-revnums            keep the dumped revision numbers in sync with\n" \
	         "                              the repository by using empty revisions for\n" \
	         "                              padding\n"));
	printf(_("    --no-incremental-header   don't print the dumpfile header when dumping\n"));
	printf(_("                              with --incremental and not starting at\n"));
	printf(_("                              revision 0\n"));
	printf("\n");
	printf(_("Subversion compatibility options:\n"));
	printf(_("    -u [--username] ARG       specify a username ARG\n"));
	printf(_("    -p [--password] ARG       specify a password ARG\n"));
	printf(_("    --no-auth-cache           do not cache authentication tokens\n"));
	printf(_("    --non-interactive         do no interactive prompting\n"));
	printf(_("    --config-dir ARG          read user configuration files from directory ARG\n"));
	printf("\n");
	printf(_("Report bugs to <%s>\n"), PACKAGE_BUGREPORT);
}


/* Prints an error message about a missing argument */
static void print_missing_arg(const char *opt)
{
	fprintf(stderr, _("ERROR: Missing argument for option: %s\n"), opt);
	fprintf(stderr, _("Please run with --help for usage information.\n"));
}


/* Parses a revision number (or range) */
static char parse_revnum(char *str, svn_revnum_t *start, svn_revnum_t *end)
{
	static const char *head = "HEAD";
	char tmp1[5], tmp2[5];
	char eos;

	/*
	 * Simply try all possible schemes.
	 * The eos-character is used to force that the sscanf command
	 * matches the whole string by ignoring it in the count comparison.
	 */
	if (sscanf(str, "%4s:%4s%c", tmp1, tmp2, &eos) == 2 && !strcmp(tmp1, head) && !strcmp(tmp2, head)) {
		*start = -1;
		*end = -1;
		return 0;
	}
	if (sscanf(str, "%ld:%4s%c", start, tmp1, &eos) == 2 && !strcmp(tmp1, head)) {
		if (*start < 0) {
			return 1;
		}
		*end = -1;
		return 0;
	}
	if (sscanf(str, "%ld:%ld%c", start, end, &eos) == 2) {
		if (*start < 0 || *end < 0 || *end < *start) {
			return 1;
		}
		return 0;
	}
	if (sscanf(str, "%ld%c", end, &eos) == 1) {
		if (*end < 1) {
			return 1;
		}
		*start = *end - 1;
		return 0;
	}
	if (sscanf(str, "%4s%c", tmp1, &eos) == 1 && !strcmp(tmp1, head)) {
		*start = -1;
		*end = -1;
		return 0;
	}
	return 1;
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/

#ifndef UNIT_TESTS

/* Program entry point */
int main(int argc, char **argv)
{
	char ret = 0;
	const char *tdir = NULL;
	int i;
	session_t session;
	dump_options_t opts;

#if ENABLE_NLS
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif /* ENABLE_NLS */

	/* Init subversion (sets apr locale etc.) */
#ifndef WIN32
	if (svn_cmdline_init(PACKAGE, stderr) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}
	atexit(apr_terminate);
#else /* !WIN32 */
	if (svn_cmdline_init(PACKAGE, NULL) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	/*
	 * On windows, we need to change the mode for stdout to binary to avoid
	 * newlines being automatically translated to CRLF.
	 */
	_setmode(_fileno(stdout), _O_BINARY);
#endif /* !WIN32 */

	session = session_create();
	opts = dump_options_create();

	/* Parse arguments */
	DEBUG_MSG("parsing arguments\n");
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			print_usage();
			goto finish;
		} else if (!strcmp(argv[i], "--version")) {
			print_version();
			goto finish;
		} else if (!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet")) {
			loglevel = -1;
		} else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
			if (loglevel < 0) {
				loglevel = 0;
			}
			loglevel++;
		} else if (!strcmp(argv[i], "-n") || !strcmp(argv[i], "--dry-run")) {
			opts.flags |= DF_DRY_RUN;
		} else if (!strcmp(argv[i], "--obfuscate")) {
			session.flags |= SF_OBFUSCATE;
		} else if (!strcmp(argv[i], "--no-auth-cache")) {
			session.flags |= SF_NO_AUTH_CACHE;
		} else if (!strcmp(argv[i], "--non-interactive")) {
			session.flags |= SF_NON_INTERACTIVE;
		} else if (!strcmp(argv[i], "--keep-revnums")) {
			opts.flags |= DF_KEEP_REVNUMS;
		} else if (!strcmp(argv[i], "--no-incremental-header")) {
			opts.flags |= DF_NO_INCREMENTAL_HEADER;
		} else if (!strcmp(argv[i], "--deltas")) {
			opts.flags |= DF_USE_DELTAS;
		} else if (!strcmp(argv[i], "--incremental")) {
			opts.flags |= DF_INCREMENTAL;
		} else if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--revision")) {
			if (i+1 >= argc) {
				print_missing_arg(argv[i]);
				goto failure;
			}
			if (parse_revnum(argv[++i], &opts.start, &opts.end)) {
				fprintf(stderr, _("ERROR: invalid revision range '%s'.\n"), argv[i]);
				goto failure;
			}
		} else if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--username")) {
			if (i+1 >= argc) {
				print_missing_arg(argv[i]);
				goto failure;
			}
			session.username = apr_pstrdup(session.pool, argv[++i]);
			/* No one needs to know the username */
			memset(argv[i], ' ', strlen(argv[i]));
		} else if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--password")) {
			if (i+1 >= argc) {
				print_missing_arg(argv[i]);
				goto failure;
			}
			session.password = apr_pstrdup(session.pool, argv[++i]);
			/* No one needs to know the password */
			memset(argv[i], ' ', strlen(argv[i]));
		} else if (!strcmp(argv[i], "--config-dir")) {
			if (i+1 >= argc) {
				print_missing_arg(argv[i]);
				goto failure;
			}
			session.config_dir = apr_pstrdup(session.pool, argv[++i]);
		} else if (!strcmp(argv[i], "--prefix")) {
			if (i+1 >= argc) {
				print_missing_arg(argv[i]);
				goto failure;
			}
			opts.prefix = apr_pstrdup(session.pool, argv[++i]);

		/* Deprecated options */
		} else if (!strcmp(argv[i], "--stop")) {
			if (i+1 >= argc) {
				print_missing_arg(argv[i]);
				goto failure;
			}
			fprintf(stderr, _("WARNING: the '--stop' option is deprated. Please use '--revision'.\n" \
			                  "         The resulting dump WILL DIFFER from the one obtained with\n" \
			                  "         previous versions of the program if you are dumping a subdirectory.\n"));
			opts.start = 0;
			if (parse_revnum(argv[++i], &opts.end, &opts.end)) {
				fprintf(stderr, _("ERROR: invalid revision number '%s'.\n"), argv[i]);
				goto failure;
			}
		} else if (!strcmp(argv[i], "--online") || !strcmp(argv[i], "--dump-uuid")) {
			fprintf(stderr, _("WARNING: the '%s' option is deprecated.\n"), argv[i]);
		} else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--download-dir")) {
			fprintf(stderr, _("WARNING: the '%s' option is deprecated.\n"), argv[i]);
			++i;
		} else if (!strcmp(argv[i], "--no-check-certificate")) {
			fprintf(stderr, _("WARNING: the '%s' option is deprecated and will be IGNORED!\n"), argv[i]);
		} else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--outfile")) {
			fprintf(stderr, _("WARNING: the '%s' option is deprecated and will be IGNORED!\n"), argv[i]);
			++i;

		/* An url */
		} else if (svn_path_is_url(argv[i])) {
			if (session.url != NULL) {
				fprintf(stderr, _("ERROR: multiple URLs detected.\n"));
				fprintf(stderr, _("Please run with --help for usage information.\n"));
				goto failure;
			}
			session.url = apr_pstrdup(session.pool, argv[i]);
		} else {
			fprintf(stderr, _("ERROR: Unknown argument or malformed url '%s'.\n"), argv[i]);
			fprintf(stderr, _("Please run with --help for usage information.\n"));
			goto failure;
		}
	}

	/* URL given ? */
	if (session.url == NULL) {
		print_usage();
		goto failure;
	}

	/* Generate temporary directory */
	DEBUG_MSG("creating temporary directory\n");
#ifndef WIN32
	tdir = getenv("TMPDIR");
	if (tdir != NULL) {
		char *tmp = apr_psprintf(session.pool, "%s/"PACKAGE"XXXXXX", tdir);
		opts.temp_dir = utils_canonicalize_pstrdup(session.pool, tmp);
	} else {
		opts.temp_dir = utils_canonicalize_pstrdup(session.pool, "/tmp/"PACKAGE"XXXXXX");
	}
	opts.temp_dir = mkdtemp(opts.temp_dir);
	if (opts.temp_dir == NULL) {
		fprintf(stderr, _("ERROR: Unable to create temporary directory.\n"));
		goto failure;
	}
#else /* !WIN32 */
	tdir = getenv("TEMP");
	if (tdir == NULL) {
		fprintf(stderr, _("ERROR: Unable to find a suitable temporary directory.\n"));
		goto failure;
	}
	opts.temp_dir = utils_canonicalize_pstrdup(session.pool, apr_psprintf(session.pool, "%s/"PACKAGE"XXXXXX", tdir));
	opts.temp_dir = _mktemp(opts.temp_dir);
	if ((opts.temp_dir == NULL) || (apr_dir_make(opts.temp_dir, 0700, session.pool) != APR_SUCCESS)) {
		fprintf(stderr, _("ERROR: Unable to create temporary directory.\n"));
		session_free(&session);
		dump_options_free(&opts);
		return EXIT_FAILURE;
	}
#endif /* !WIN32 */

	/* Do the real work */
	if (session_open(&session) == 0) {
		ret = dump(&session, &opts);
		session_close(&session);

		/* Clean up temporary directory on success */
#ifndef DUMP_DEBUG
		if (ret == 0) {
			utils_rrmdir(session.pool, opts.temp_dir, 1);
		} else {
			fprintf(stderr, _("NOTE: Please remove the temporary directory %s manually\n"), opts.temp_dir);
		}
#endif
	} else {
		utils_rrmdir(session.pool, opts.temp_dir, 1);
	}

	if (ret != 0) {
		goto failure;
	}

	/* Program termination: success */
	ret = 0;
	goto finish;

failure:
	ret = 1;
finish:
	session_free(&session);
	dump_options_free(&opts);
	return (ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

#endif /* !UNIT_TESTS */



/*---------------------------------------------------------------------------*/
/* Functions exported for unit testing                                       */
/*---------------------------------------------------------------------------*/

#ifdef UNIT_TESTS

char ut_parse_revnum(char *str, svn_revnum_t *start, svn_revnum_t *end)
{
	return parse_revnum(str, start, end);
}

#endif /* !UNIT_TESTS */
