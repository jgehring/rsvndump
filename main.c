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


#include "main.h"
#include "dump.h"
#include "svn_functions.h"

#include <stdlib.h>
#include <string.h>


// Globals
char *repo_url = NULL;
char *repo_uuid = NULL;
char *repo_username = NULL;
char *repo_password = NULL;
int repo_prefix_len;
char quiet = 0;
FILE *input, *output;


// Prints usage information
static void print_usage()
{
	printf(APPNAME" "APPVERSION"\n");
	printf("CopyLeft 2008 by "APPAUTHOR"\n");
	printf("\n");
	printf("USAGE: "APPNAME" [options] <url>\n\n");
	printf("Valid options:\n");
	printf("    -q [--quiet]            be quiet\n");
	printf("    -u [--username] arg     username\n");
	printf("    -p [--password] arg     password\n");
	printf("    -l [--logfile] arg      output of 'svn -q log|tac'\n");
       	printf("                            if not specified, read from stdin\n");
	printf("    -o [--outfile] arg      write data to file\n");
       	printf("                            if not specified, print to stdout\n");
	printf("\n");
}


// Frees memory used by globals
static void free_globals()
{
	if (repo_url != NULL) {
		free(repo_url);
	}
	if (repo_uuid != NULL) {
		free(repo_uuid);
	}
	if (repo_username != NULL) {
		free(repo_username);
	}
	if (repo_password != NULL) {
		free(repo_password);
	}
}


// Program entry point
int main(int argc, char **argv)
{
	int i;

	input = stdin;
	output = stdout;

	// Parse arguments
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			print_usage();
			return EXIT_SUCCESS;
		}
		else if (!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet")) {
			quiet = 1;
		}
		else if (i+1 < argc && (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--username"))) {
			repo_username = strdup(argv[++i]);
		}
		else if (i+1 < argc && (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--password"))) {
			repo_password = strdup(argv[++i]);
		}
		else if (i+1 < argc && (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--logfile"))) {
			input = fopen(argv[++i], "r");
			if (input == NULL) {
				fprintf(stderr, "Error opening logfile\n");
				free_globals();
				return EXIT_FAILURE;
			}
		}
		else if (i+1 < argc && (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--outfile"))) {
			output = fopen(argv[++i], "w");
			if (output == NULL) {
				fprintf(stderr, "Error opening outfile\n");
				free_globals();
				return EXIT_FAILURE;
			}
		}
		else {
			repo_url = strdup(argv[i]);
		}
	}

	if (repo_url == NULL) {
		print_usage();
		return EXIT_FAILURE;
	}

	svn_init();

	char *turl = NULL, *tpref = NULL;
	svn_repo_info(repo_url, &turl, &tpref);
	if (turl) {
		free(turl);
	}
	repo_prefix_len = strlen(tpref);
	if (tpref) {
		free(tpref);
	}

	dump_repository();

	svn_free();

	if (input != stdin) {
		fclose(input);
	}
	if (output != stdout) {
		fclose(output);
	}
	free_globals();
	return EXIT_SUCCESS;
}
