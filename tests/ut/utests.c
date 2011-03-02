/*
 *      Unit tests for rsvndump
 *      Copyright (C) 2009-2011 by Jonas Gehring
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
 */


#include <stdlib.h>
#include <stdio.h>

#include <svn_error.h>
#include <svn_types.h>

#include <main.h>
#include <list.h>


/* Check parse_revnum in main.c */
extern char ut_parse_revnum(char *str, svn_revnum_t *start, svn_revnum_t *end);
static char test_parse_revnum()
{
	typedef struct {
		const char *str;
		char result;
		svn_revnum_t start;
		svn_revnum_t end;
	} testcase_t;

	int i;
	char ret = 0;
	testcase_t tests[] = {
		{"0:1:0", 1, 0, 0},
		{"-3:-4", 1, 0, 0},
		{"a:b", 1, 0, 0},
		{"-:", 1, 0, 0},
		{"", 1, 0, 0},
		{"2392miocsc_", 1, 0, 0},
		{"0:1", 0, 0, 1},
		{"1:1", 0, 1, 1},
		{"1:", 1, 0, 0},
		{":1", 1, 0, 0},
		{"00:1201", 0, 0, 1201},
		{"90:3", 1, 0, 0},
		{"-1:4", 1, 0, 0},
		{"HEAD", 0, -1, -1},
		{"HEAD:", 1, 0, 0},
		{"HEAD:HEAD", 0, -1, -1},
		{"HEAD:4", 1, 0, 0},
		{"10:HEAD", 0, 10, -1},
		{"10:HEAD:0", 1, 0, 0}
	};

	printf("Testing parse_revnum(): ");
	for (i = 0; i < sizeof(tests)/sizeof(testcase_t); i++) {
		char res;
		svn_revnum_t start, end;

		res = ut_parse_revnum((char *)tests[i].str, &start, &end);
		if (res != tests[i].result) {
			printf("\n\t%d: FAIL: wrong result: %d instead of %d\n", i, (int)res, (int)tests[i].result);
			ret = 1;
			continue;
		}
		if (res == 0 && (start != tests[i].start || end != tests[i].end)) {
			printf("\n\t%d: FAIL: wrong range: %ld:%ld instead of %ld:%ld\n\t", i, start, end, tests[i].start, tests[i].end);
			ret = 1;
			continue;
		}

		printf("%d ", i);
		fflush(stdout);
	}

	printf("\n");
	return ret;
}

static int intcmp(const void *a, const void *b) {
	return *(long *)a - *(long *)b;
}

static char test_list()
{
	long i, j;
	list_t l;

	printf("Testing list implementation: ");

	l = list_create(sizeof(long));
	for (i = 0; i < 10; i++) {
		size_t t;
		long s = (long)4 << i;
		for (j = 0; j < s; j++) {
			list_append(&l, &j);
		}
		t = l.size;
		if (l.size != s) {
			printf("\n\t%ld: FAIL: size corrupted after inserting: %ld instead of %ld\n", i, (long)t, s);
			list_free(&l);
			return 1;
		}

		list_qsort(&l, intcmp);

		if (l.size != t) {
			printf("\n\t%ld: FAIL: size corrupted after sorting: %ld instead of %ld\n", i, (long)l.size, (long)t);
			list_free(&l);
			return 1;
		}
		for (j = 0; j < t-1; j++) {
			if (((long *)l.elements)[j] > ((long *)l.elements)[j+1]) {
				printf("\n\t%ld: FAIL: sorting failed: [%ld] > [%ld]\n", i, j, j+1);
				list_free(&l);
				return 1;
			}
		}

		for (j = 0; j < t; j++) {
			list_remove(&l, rand() % l.size);
		}
		if (l.size != 0) {
			printf("\n\t%ld: FAIL: List not empty after removing %ld elements\n", i, (long)t);
			list_free(&l);
			return 1;
		}
		printf("%ld ", i);
		fflush(stdout);
	}

	printf("\n");
	list_free(&l);
	return 0;
}


/* Program entry point */
int main(int argc, char **argv)
{
	if (test_parse_revnum()) {
		return EXIT_FAILURE;
	}
	if (test_list()) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
