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
 * 	file: logentry.c
 * 	desc: Simple struct for a log entry 
 */


#include <svn_repos.h>

#include "logentry.h"


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/

/* Creates a new logentry with default values */
logentry_t logentry_create()
{
	logentry_t entry;
	entry.revision = -2;
	entry.author.key = SVN_PROP_REVISION_AUTHOR;
	entry.author.value = NULL;
	entry.date.key = SVN_PROP_REVISION_DATE;
	entry.date.value = NULL;
	entry.msg.key = SVN_PROP_REVISION_LOG;
	entry.msg.value = NULL;
	return entry;
}


/* Destroys a node, freeing all non-NULL value strings  */
void logentry_free(logentry_t *logentry)
{
	property_free(&logentry->author);
	property_free(&logentry->date);
	property_free(&logentry->msg);
}
