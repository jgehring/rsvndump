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
 *      file: main.c
 *      desc: Program initialization and option parsing
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <svn_cmdline.h>
#include <svn_path.h>

#include "main.h"
#include "dump.h"
#include "wsvn.h"
#include "utils.h"


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/

/* Prints the program version */
static void print_version()
{
	printf(APPNAME" "APPVERSION"\n");
	printf("Copyright (C) 2008-2009 by "APPAUTHOR"\n");
}


/* Prints usage information */
static void print_usage()
{
	print_version();
	printf("\n");
	printf(_("USAGE:"));
	printf(" "APPNAME" ");
	printf(_("[options] <url>\n\n"));
	printf(_("Valid options:\n"));
	printf(_("    -h [--help]               print a nice help screen\n"));
	printf(_("    --version                 print the program name and version\n"));
	printf(_("    -q [--quiet]              be quiet\n"));
	printf(_("    -v [--verbose]            print extra progress\n"));
	printf(_("    -u [--username] arg       username\n"));
	printf(_("    -p [--password] arg       password\n"));
	printf(_("    -o [--outfile] arg        write data to file arg\n" \
	         "                              if not specified, print to stdout\n"));
	printf(_("    -d [--download-dir] arg   directory for working copy\n" \
	         "                              if not specified, create a temporary directory\n"));
	printf(_("    --online                  don't use a working copy for dumping\n"));
	printf(_("    --no-check-certificate    don't validate SSL certificates\n"));
	printf(_("    --stop arg                stop after dumping revision arg\n"));
	printf(_("    --prefix arg              prepend arg to the path that is being dumped\n"));
	printf(_("    --keep-revnums            keep the dumped revision numbers in sync with\n" \
	         "                              the repository by using empty revisions for\n" \
	         "                              padding\n"));
	printf(_("    --dump-uuid               include the repository uuid in the dump\n"));
	printf(_("    --deltas                  use deltas in dump output\n"));
	printf("\n");
	printf("Report bugs to <"PACKAGE_BUGREPORT">\n");
}


/* Parses a revision number */
static int rev_atoi(char *str)
{
	if (strncmp("HEAD", str, 4) == 0) {
		return HEAD_REVISION;
	}
	return atoi(str);
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/

/* Program entry point */
int main(int argc, char **argv)
{
	char ret, dir_created = 0;
	int i;
	dump_options_t opts = dump_options_create();

#if ENABLE_NLS
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif /* ENABLE_NLS */

	/* Init subversion (sets apr locale etc.) */
	if (svn_cmdline_init(APPNAME, stderr) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	/* Parse arguments */
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			print_usage();
			dump_options_free(&opts);
			return EXIT_SUCCESS;
		} else if (!strcmp(argv[i], "--version")) {
			print_version();
			dump_options_free(&opts);
			return EXIT_SUCCESS;
		} else if (!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet")) {
			opts.verbosity = -1;
		} else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
			opts.verbosity = 1;
		} else if (!strcmp(argv[i], "--online")) {
			opts.online = 1;
		} else if (!strcmp(argv[i], "--no-check-certificate")) {
			opts.no_check_certificate = 1;
		} else if (!strcmp(argv[i], "--keep-revnums")) {
			opts.keep_revnums = 1;
		} else if (!strcmp(argv[i], "--dump-uuid")) {
			opts.dump_uuid = 1;
		} else if (!strcmp(argv[i], "--deltas")) {
			opts.deltas = 1;
		} else if (i+1 < argc && !strcmp(argv[i], "--stop")) {
			opts.endrev = rev_atoi(argv[++i]);
		} else if (i+1 < argc && (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--username"))) {
			if (opts.username != NULL) {
				free(opts.username);
			}
			opts.username = strdup(argv[++i]);
			/* No one needs to know the username */
			memset(argv[i], ' ', strlen(argv[i]));
		} else if (i+1 < argc && (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--password"))) {
			if (opts.password != NULL) {
				free(opts.password);
			}
			opts.password = strdup(argv[++i]);
			/* No one needs to know the password */
			memset(argv[i], ' ', strlen(argv[i]));
		} else if (i+1 < argc && !strcmp(argv[i], "--prefix")) {
			if (opts.user_prefix != NULL) {
				free(opts.user_prefix);
			}
			opts.user_prefix = strdup(argv[++i]);
		} else if (i+1 < argc && (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--outfile"))) {
			if (opts.output != NULL && opts.output != stdout) {
				fclose(opts.output);
			}
#ifdef HAVE_FOPEN64
			opts.output = fopen64(argv[++i], "wb");
#else
			opts.output = fopen(argv[++i], "wb");
#endif
			if (opts.output == NULL) {
				fprintf(stderr, _("ERROR: Unable to open output file.\n"));
				dump_options_free(&opts);
				return EXIT_FAILURE;
			}
		} else if (i+1 < argc && (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--download-dir"))) {
			struct stat st;
			stat(argv[++i], &st);
			if (!(st.st_mode & S_IFDIR)) {
				fprintf(stderr, _("ERROR: '%s' is either not a directory or does not exist.\n"), argv[i]);
				dump_options_free(&opts);
				return EXIT_FAILURE;
			}
			if (opts.repo_dir != NULL) {
				free(opts.repo_dir);
			}
			opts.repo_dir = utils_canonicalize_strdup(argv[i]);
		} else if (svn_path_is_url(argv[i])) {
			if (opts.repo_url != NULL) {
				fprintf(stderr, _("ERROR: multiple URLs detected.\n"));
				dump_options_free(&opts);
				return EXIT_FAILURE;
			}
			opts.repo_url = strdup(argv[i]);
		} else {
			fprintf(stderr, _("ERROR: Unkown argument or malformed url '%s'.\n"), argv[i]);
			fprintf(stderr, _("Type %s --help for usage information.\n"), argv[0]);
			dump_options_free(&opts);
			return EXIT_FAILURE;
		}
	}

	if (opts.repo_url == NULL) {
		dump_options_free(&opts);
		print_usage();
		return EXIT_FAILURE;
	}

	/* Generate temporary directory if neccessary */
	if (opts.repo_dir == NULL && (opts.online == 0 || opts.deltas)) {
		const char *tdir = getenv("TMPDIR");
		if (tdir != NULL) {
			char *tmp = malloc(strlen(tdir)+strlen(APPNAME)+8);
			sprintf(tmp, "%s/%sXXXXXX", tdir, APPNAME);
			opts.repo_dir = utils_canonicalize_strdup(tmp);
			free(tmp);
		} else {
			opts.repo_dir = utils_canonicalize_strdup("/tmp/"APPNAME"XXXXXX");
		}
		opts.repo_dir = mkdtemp(opts.repo_dir);
		if (opts.repo_dir == NULL) {
			fprintf(stderr, _("ERROR: Unable to create download directory.\n"));
			dump_options_free(&opts);
			return EXIT_FAILURE;
		}
		dir_created = 1;
	}

	/* Get a properly encoded url */
	opts.repo_eurl = wsvn_uri_encode(opts.repo_url);

	/* Dump */
	ret = dump(&opts);

	/* Clean up working copy */
	if (opts.online == 0 || opts.deltas) {
		utils_rrmdir(opts.repo_dir, dir_created);
	}

	dump_options_free(&opts);
	if (ret == 0) {
		return EXIT_SUCCESS;
	} else {
		return EXIT_FAILURE;
	}
}
