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


#ifndef _LIST_H
#define _LIST_H


#include <stdlib.h>


// Data structures
typedef struct {
	int	size, max;
	size_t	elsize;
	void	*elements;
} list_t;

extern void list_init(list_t *l, size_t elsize);
extern void list_free(list_t *l);
extern void list_add(list_t *l, void *element);
extern void list_remove(list_t *l, int pos);


#endif
