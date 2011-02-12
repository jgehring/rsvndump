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
 *      file: list.c
 *      desc: A simple dynamic list
 */


#include <string.h>

#include "list.h"


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/

/* Creates a new list with space for a single element */
list_t list_create(unsigned int elsize)
{
	list_t l;
	l.max = 1;
	l.elsize = elsize;
	l.size = 0;
	l.elements = malloc(l.elsize);
	return l;
}


/* Destroys a list and all its elements */
void list_free(list_t *l)
{
	if (l->elements != NULL) {
		free(l->elements);
		l->elements = NULL;
	}
}


/* Appends an alement to the list */
list_t *list_append(list_t *l, void *element)
{
	if (l->size >= l->max) {
		/* Resize, i.e. make the list twice as large */
		void *t = malloc(l->elsize * l->max);
		if (t == NULL) {
			return NULL;
		}
		memcpy(t, l->elements, l->elsize * l->max);
		free(l->elements);
		l->max *= 2;
		l->elements = malloc(l->elsize * l->max);
		if (l->elements == NULL) {
			return NULL;
		}
		memcpy(l->elements, t, l->elsize * (l->max >> 1));
		free(t);
	}
	memcpy((char *)l->elements + (l->elsize * l->size), element, l->elsize);
	++l->size;
	return l;
}


/* Removes the element at pos from the list */
void list_remove(list_t *l, unsigned int pos)
{
	/* Resizing on removal is not implemented. The lists in this program
	   have a very short lifetime and are mostly only used for adding elements */
	if (pos < l->size) {
		memcpy((char *)l->elements + pos*l->elsize, (char *)l->elements + (pos+1)*l->elsize, (l->size-pos-1)*l->elsize);
		--l->size;
	}
}


/* Sorts the list using a specified sorting function */
void list_qsort(list_t *l, int (* comparator)(const void *, const void *))
{
	qsort(l->elements, (size_t)l->size, (size_t)l->elsize, comparator);
}
