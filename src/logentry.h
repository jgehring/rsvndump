/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008 Jonas Gehring
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * 	
 * 	file: logentry.h
 * 	desc: Simple struct for a log entry 
 */


#ifndef LOGENTRY_H 
#define LOGENTRY_H


#include <svn_types.h>

#include "property.h"


/* Log entry */
typedef struct {
	svn_revnum_t	revision;
	property_t	author;
	property_t	date;
	property_t	msg;
} logentry_t;


/* Creates a new logentry with default values */
extern logentry_t logentry_create();

/* Destroys a node, freeing all non-NULL value strings  */
extern void logentry_free(logentry_t *logentry);


#endif
