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
 *      file: dump.h
 *      desc: Main working place
 */


#ifndef DUMP_H_
#define DUMP_H_


#include <svn_types.h>

#include "session.h"


#define HEAD_REIVISION (-1)


/* Flags than can be set in dump_options_t */
enum dump_flags {
	DF_USE_DELTAS = 0x01,
	DF_KEEP_REVNUMS = 0x02,
	DF_INCREMENTAL = 0x04,
	DF_INITIAL_DRY_RUN = 0x08,
	DF_NO_INCREMENTAL_HEADER = 0x10,
	DF_DRY_RUN = 0x20
};

/* Data structure to bundle information related to the dumping process */
typedef struct {
	char          *temp_dir;
	char          *prefix;
	svn_revnum_t  start;
	svn_revnum_t  end;
	int           flags;
	int           dump_format;
} dump_options_t;


/* Creates and intializes a new dump_options_t object */
extern dump_options_t dump_options_create();

/* Frees a dump_options_t object */
extern void dump_options_free(dump_options_t *opts);

/* Start the dumping process, using the given session and options */
extern char dump(session_t *session, dump_options_t *opts);


#endif
