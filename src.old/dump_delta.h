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
 *      file: dump_delta.h
 *      desc: Support for dumping of deltas 
 */


#ifndef DUMP_DELTA_H
#define DUMP_DELTA_H


#include <svn_types.h>

#include "dump.h"
#include "list.h"
#include "logentry.h"


/* Dumps the specified revision using the given dump options */
extern char dump_delta_revision(dump_options_t *opts, list_t *logs, logentry_t *entry, svn_revnum_t local_revnum);


#endif
