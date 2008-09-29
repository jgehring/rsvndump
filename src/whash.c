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
 *
 *
 * 	file: whash.c
 * 	desc: A simple wrapper for apr_hash_t supporting strings
 */


#include <apr_hash.h>

#include "whash.h"


/*---------------------------------------------------------------------------*/
/* Static variables                                                          */
/*---------------------------------------------------------------------------*/

static apr_hash_t *table = NULL;
static apr_pool_t *pool = NULL;
static int dummy_val = 0;


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/

/* Creates the hash table */
char whash_create()
{
	if (apr_pool_create(&pool, NULL)) {
		return 1;
	}
	table = apr_hash_make(pool);
	return 0;
}


/* Frees the hash table */
void whash_free()
{
	apr_pool_clear(pool);
	apr_pool_destroy(pool);
	pool = NULL;
	table = NULL;
}


/* Inserts a string into the hash */
void whash_insert(const char *key)
{
	apr_hash_set(table, key, APR_HASH_KEY_STRING, &dummy_val);
}


/* Provides string lookup */
char whash_contains(const char *key)
{
	return (apr_hash_get(table, key, APR_HASH_KEY_STRING) != NULL);
}
