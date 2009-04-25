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

#include "property.h"


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/


/* Returns the length of a property */
unsigned int property_strlen(const char *key, const char *value)
{
	unsigned int len, keylen, vallen;
	char *buffer;
	if ((key == NULL) || (value == NULL)) {
		return 0;
	}
	len = 0;
	keylen = strlen(key);
	vallen = strlen(value);
	buffer = malloc(16 + (keylen < vallen ? vallen : keylen));
	sprintf(buffer, "K %d\n", keylen);
	len += strlen(buffer);
	sprintf(buffer, "%s\n", key);
	len += strlen(buffer);
	sprintf(buffer, "V %d\n", vallen);
	len += strlen(buffer);
	sprintf(buffer, "%s\n", value);
	len += strlen(buffer);
	free(buffer);
	return len;
}


/* Dumps a property to stdout */
void property_dump(const char *key, const char *value)
{
	if ((key == NULL) || (value == NULL)) {
		return;
	}
	printf("K %d\n", strlen(key));
	printf("%s\n", key);
	printf("V %d\n", strlen(value));
	printf("%s\n", value);
}
