/*
 *      Unit tests for rsvndump
 *      Copyright (C) 2009 by Jonas Gehring
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


/* Check parse_revnum in main.c */
extern char ut_parse_revnum(char *str, svn_revnum_t *start, svn_revnum_t *end);
static void test_parse_revnum()
{
	typedef struct {
		const char *str;
		char result;
		svn_revnum_t start;
		svn_revnum_t end;
	} testcase_t;

	int i;
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

	printf("Testing parse_revnum() [targetted]:\n");
	for (i = 0; i < sizeof(tests)/sizeof(testcase_t); i++) {
		char res;
		svn_revnum_t start, end;

		res = ut_parse_revnum(tests[i].str, &start, &end);
		if (res != tests[i].result) {
			printf("\t%d: FAIL: wrong result: %d instead of %d\n", i, (int)res, (int)tests[i].result);
			continue;
		}
		if (res == 0 && (start != tests[i].start || end != tests[i].end)) {
			printf("\t%d: FAIL: wrong range: %ld:%ld instead of %ld:%ld\n", i, start, end, tests[i].start, tests[i].end);
			continue;
		}

		printf("\t%d: pass\n", i);
	}
}


/* Program entry point */
int main(int argc, char **argv)
{
	test_parse_revnum();

	return EXIT_SUCCESS;
}
