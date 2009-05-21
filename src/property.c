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
 *      file: property.c
 *      desc: Convenience functions for dumping properties
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <svn_pools.h>

#include <apr_strings.h>

#include "property.h"


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/


/* Returns the length of a property */
size_t property_strlen(struct apr_pool_t *pool, const char *key, const char *value)
{
	size_t len;
	apr_pool_t *subpool = svn_pool_create((apr_pool_t *)pool);

	if (key == NULL) {
		return 0;
	}

	if (value == NULL) {
		len = strlen(apr_psprintf(subpool, "K %d\n%s\nV 0\n\n", strlen(key), key));
	} else {
		len = strlen(apr_psprintf(subpool, "K %d\n%s\nV %d\n%s\n", strlen(key), key, strlen(value), value));
	}

	apr_pool_destroy(subpool);
	return len;
}


/* Dumps a property to stdout */
void property_dump(const char *key, const char *value)
{
	if (key == NULL) {
		return;
	}
	printf("K %lu\n", (unsigned long)strlen(key));
	printf("%s\n", key);
	if (value != NULL) {
		printf("V %lu\n", (unsigned long)strlen(value));
		printf("%s\n", value);
	} else {
		printf("V 0\n\n");
	}
}
