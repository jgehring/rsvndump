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
 *      file: rhash.c
 *      desc: Interface for apr_hash_t with special memory handling
 *
 *      The idea behind this data structure is that there are hashes in delta.c
 *      that store data for which the pool allocation model is not suitable.
 *      Thus, this hash implements its own memory handling for both keys and
 *      values.
 *      However, this raises the need for manually clearing the hash contents
 *      (or calling rhash_clear()).
 */


#include <assert.h>
#include <stdlib.h>

#include <svn_error.h>

#include <apr_strings.h>
#include <apr_tables.h>

#include "main.h"
#include "logger.h"
#include "rhash.h"


/*---------------------------------------------------------------------------*/
/* Local data structures                                                     */
/*---------------------------------------------------------------------------*/


typedef struct {
	void *key;
	void *val;
} entry_t;


/*---------------------------------------------------------------------------*/
/* Local functions                                                           */
/*---------------------------------------------------------------------------*/


/* Frees the memory of an entry */
static void entry_free(entry_t *e)
{
	free(e->key);
	free(e->val);
	free(e);
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/


/* Creates a new rhash */
rhash_t *rhash_make(apr_pool_t *pool)
{
	return apr_hash_make(pool);
}


/* Clears and frees an rhash */
void rhash_clear(rhash_t *ht)
{
	apr_hash_index_t *hi;
	apr_pool_t *pool = apr_hash_pool_get(ht);
	apr_array_header_t *arr = apr_array_make(pool, apr_hash_count(ht), sizeof(void *));
	entry_t **eptr;

	/* Insert all values into an array, and free it afterwards */
	for (hi = apr_hash_first(pool, ht); hi; hi = apr_hash_next(hi)) {
		const void *key;
		apr_ssize_t klen;
		void *val;
		apr_hash_this(hi, &key, &klen, &val);
		*(void **)apr_array_push(arr) = val;
	}
	apr_hash_clear(ht);

	while ((eptr = apr_array_pop(arr)) != NULL) {
		entry_free(*eptr);
	}
}


/* Wrapper for apr_hash_set */
void rhash_set(rhash_t *ht, const void *key, apr_ssize_t klen, const void *val, apr_ssize_t vlen)
{
	entry_t *entry = apr_hash_get(ht, key, klen);

	if (val == NULL) {
		/* Deletion */
		if (entry) {
			apr_hash_set(ht, key, klen, NULL);
			entry_free(entry);
		}
		return;
	}

	if (entry) {
		/* Replacement: Free the old value */
		if (vlen == RHASH_VAL_STRING) {
			DEBUG_MSG("rhash_set(): replacing %s: %s -> %s (0x%X -> 0x%X)\n", (const char *)key, (const char *)entry->val, (const char *)val, entry->val, val);
		}

		assert(val != entry->val);
		free(entry->val);
		if (vlen == RHASH_VAL_STRING) {
			entry->val = strdup(val);
		} else {
			entry->val = malloc(vlen);
			memcpy(entry->val, val, vlen);
		}
	} else {
		/* Normal insert: Duplicate both key and value */
		entry = malloc(sizeof(entry_t));

		if (klen == APR_HASH_KEY_STRING) {
			entry->key = strdup(key);
		} else {
			entry->key = malloc(klen);
			memcpy(entry->key, key, klen);
		}
		if (vlen == RHASH_VAL_STRING) {
			entry->val = strdup(val);
		} else {
			entry->val = malloc(vlen);
			memcpy(entry->val, val, vlen);
		}

		if (vlen == RHASH_VAL_STRING) {
			DEBUG_MSG("rhash_set(): setting %s: %s (0x%X)\n", (const char *)key, (const char *)entry->val, entry->val);
		}
		apr_hash_set(ht, entry->key, klen, entry);
	}
}


/* Wrapper for apr_hash_get */
void *rhash_get(rhash_t *ht, const void *key, apr_ssize_t klen)
{
	entry_t *e = apr_hash_get(ht, key, klen);
	if (e) {
		return e->val;
	}
	return NULL;
}


/* Wrapper for apr_hash_first */
apr_hash_index_t *rhash_first(apr_pool_t *p, rhash_t *ht)
{
	return apr_hash_first(p, ht);
}


/* Wrapper for apr_hash_next */
apr_hash_index_t *rhash_next(apr_hash_index_t *hi)
{
	return apr_hash_next(hi);
}


/* Wrapper for apr_hash_this */
void rhash_this(apr_hash_index_t *hi, const void **key, apr_ssize_t *klen, void **val)
{
	entry_t *e;
	apr_hash_this(hi, key, klen, (void **)&e);
	val = &e->val;
}


/* Wrapper for apr_hash_count */
unsigned int rhash_count(rhash_t *ht)
{
	return apr_hash_count(ht);
}
