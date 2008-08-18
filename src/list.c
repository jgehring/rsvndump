/*
 *	rsvndump - remote svn repository dump
 *	Copyright (C) 2008 Jonas Gehring
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along
 *	with this program; if not, write to the Free Software Foundation, Inc.,
 *	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include "list.h"

#include <string.h>


void list_init(list_t *l, size_t elsize)
{
	l->max = 1;
	l->elsize = elsize;
	l->size = 0;
	l->elements = malloc(l->elsize);
}

void list_free(list_t *l)
{
	free(l->elements);
}

void list_add(list_t *l, void *element)
{
	if (l->size >= l->max) {
		void *t = malloc(l->elsize * l->max);
		memcpy(t, l->elements, l->elsize * l->max);
		free(l->elements);
		l->max *= 2;
		l->elements = malloc(l->elsize * l->max);
		memcpy(l->elements, t, l->elsize * (l->max >> 1)); 
		free(t);
	}
	memcpy(l->elements + (l->elsize * l->size), element, l->elsize);
	++l->size;
}

void list_remove(list_t *l, int pos)
{
	if (pos < l->size) {
		memcpy(l->elements + pos*l->elsize, l->elements + (pos+1)*l->elsize, (l->size-pos-1)*l->elsize);
		--l->size;
	}
}
