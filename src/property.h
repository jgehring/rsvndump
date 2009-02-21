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
 *      file: property.h
 *      desc: Simple convenience type for properties 
 */


#ifndef PROPERTRY_H
#define PROPERTRY_H


#include <stdio.h>


/* Valid node kinds */
typedef struct {
	const char	*key;
	char		*value;
} property_t;


/* Creates a new property */
extern property_t property_create();

/* Destroys a property, freeing the value-pointer if it is not NULL */
extern void property_free(property_t *prop); 

/* Gets the string length of a property */
extern unsigned int property_strlen(property_t *prop);

/* Dumps the contents of a property */
extern void property_dump(property_t *prop, FILE *output);


#endif
