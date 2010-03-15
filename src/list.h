/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008-2010 Jonas Gehring
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
 *      file: list.h
 *      desc: A simple dynamic list
 */


#ifndef LIST_H
#define LIST_H


#include <stdlib.h>


/* List structure */
typedef struct {
	unsigned int	size, max;
	unsigned int	elsize;
	void		*elements;
} list_t;


/* Creates a new list with space for a single element */
extern list_t list_create(unsigned int elsize);

/* Destroys a list and all its elements */
extern void list_free(list_t *l);

/* Appends an alement to the list */
extern list_t *list_append(list_t *l, void *element);

/* Removes the element at pos from the list */
extern void list_remove(list_t *l, unsigned int pos);

/* Sorts the list using a specified sorting function */
extern void list_qsort(list_t *l, int (* comparator)(const void *, const void *));


#endif
