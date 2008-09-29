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
 * 	file: property.c
 * 	desc: Simple convenience type for properties 
 */


#include <stdlib.h>
#include <string.h>

#include "property.h"


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/

/* Creates a new property */
property_t property_create()
{
	property_t prop;
	prop.key = prop.value = NULL;
	return prop;
}


/* Destroys a property, freeing char-pointers if they are not NULL */
void property_free(property_t *prop)
{
	if (prop->value != NULL) {
		free(prop->value);
		prop->value = NULL;
	}
}


/* Gets the string length of a property */
unsigned int property_strlen(property_t *prop)
{
	int len, keylen, vallen;
	char *buffer;
	if (!prop->key || !prop->value) {
		return 0;
	}
	len = 0;
	keylen = strlen(prop->key);
	vallen = strlen(prop->value);
	buffer = malloc(16 + (keylen < vallen ? vallen : keylen));
	sprintf(buffer, "K %d\n", keylen);
	len += strlen(buffer);
	sprintf(buffer, "%s\n", prop->key);
	len += strlen(buffer);
	sprintf(buffer, "V %d\n", vallen);
	len += strlen(buffer);
	sprintf(buffer, "%s\n", prop->value);
	len += strlen(buffer);
	free(buffer);
	return len;
}


/* Dumps the contents of a property */
void property_dump(property_t *prop, FILE *output)
{
	if (!prop->key || !prop->value) {
		return;
	}
	fprintf(output, "K %d\n", strlen(prop->key));
	fprintf(output, "%s\n", prop->key);
	fprintf(output, "V %d\n", strlen(prop->value));
	fprintf(output, "%s\n", prop->value);
}
