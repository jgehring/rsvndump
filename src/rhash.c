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
 *
 *      The implementation uses an extra hash to store the pointers
 *      for the allocated key data.
 */


#include <stdlib.h>

#include <svn_error.h>

#include <apr_strings.h>

#include "main.h"
#include "logger.h"
#include "rhash.h"


/*---------------------------------------------------------------------------*/
/* Local data structures                                                     */
/*---------------------------------------------------------------------------*/


struct rhash_t {
	apr_hash_t    *apr_hash;
	apr_pool_t    *pool;
	apr_hash_t    *key_pointers;
};


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/


/* Creates a new rhash */
rhash_t *rhash_make(apr_pool_t *pool)
{
	rhash_t *hash = apr_palloc(pool, sizeof(rhash_t));
	hash->apr_hash = apr_hash_make(pool);
	hash->pool = pool;
	hash->key_pointers = apr_hash_make(pool);

	return hash;
}


/* Clears and frees an rhash */
void rhash_clear(rhash_t *ht)
{
	apr_hash_index_t *hi;
	apr_hash_t *temp = apr_hash_make(ht->pool);

	/* Save all entries into a temporary hash */
	for (hi = apr_hash_first(ht->pool, ht->apr_hash); hi; hi = apr_hash_next(hi)) {
		const void *key;
		apr_ssize_t klen;
		void *value;
		apr_hash_this(hi, &key, &klen, &value);
		apr_hash_set(temp, key, klen, value);
	}

	/* Remove all entris */
	for (hi = apr_hash_first(ht->pool, temp); hi; hi = apr_hash_next(hi)) {
		const void *key;
		apr_ssize_t klen;
		void *value;
		apr_hash_this(hi, &key, &klen, &value);
		rhash_set(ht, key, klen, NULL, 0);
	}
}


/* Wrapper for apr_hash_set */
void rhash_set(rhash_t *ht, const void *key, apr_ssize_t klen, const void *val, apr_ssize_t vlen)
{
	if (val == NULL) {
		/* The mapping should be removed, so free both key and value memory */
		void *key_ptr = apr_hash_get(ht->key_pointers, key, klen);
		void *val_ptr = apr_hash_get(ht->apr_hash, key, klen);

		if (val_ptr) {
			apr_hash_set(ht->apr_hash, key, klen, NULL);
			free(val_ptr);
		}
		if (key_ptr) {
			apr_hash_set(ht->key_pointers, key, klen, NULL);
			free(key_ptr);
		}
		return;
	}

	if (apr_hash_get(ht->apr_hash, key, klen) != NULL) {
		/* Replacement: Free the old value */
		void *val_ptr = apr_hash_get(ht->apr_hash, key, klen);
		void *val_dup;

		if (vlen == -1) {
			DEBUG_MSG("rhash_set(): replacing %s: %s -> %s (0x%X -> 0x%X)\n", (const char *)key, (const char *)val_ptr, (const char *)val, val_ptr, val);
		}

		if (vlen == RHASH_VAL_STRING) {
			val_dup = strdup(val);
		} else {
			val_dup = malloc(vlen);
			memcpy(val_dup, val, vlen);
		}

		apr_hash_set(ht->apr_hash, key, klen, val_dup);
		if (val_ptr && val != val_ptr) {
			DEBUG_MSG("freeing %X\n", (void *)val_ptr);
			free(val_ptr);
		}
	} else {
		/* Normal insert: Duplicate both key and value */
		void *key_dup, *val_dup;

		if (klen == APR_HASH_KEY_STRING) {
			key_dup = strdup(key);
		} else {
			key_dup = malloc(klen);
			memcpy(key_dup, key, klen);
		}
		if (vlen == RHASH_VAL_STRING) {
			val_dup = strdup(val);
		} else {
			val_dup = malloc(vlen);
			memcpy(val_dup, val, vlen);
		}

		if (vlen == -1) {
			DEBUG_MSG("rhash_set(): setting %s: %s (0x%X)\n", (const char *)key, (const char *)val_dup, val_dup);
		}
		
		apr_hash_set(ht->key_pointers, key_dup, klen, key_dup);
		apr_hash_set(ht->apr_hash, key_dup, klen, val_dup);
	}
}


/* Wrapper for apr_hash_get */
void *rhash_get(rhash_t *ht, const void *key, apr_ssize_t klen)
{
	return apr_hash_get(ht->apr_hash, key, klen);
}


/* Wrapper for apr_hash_first */
apr_hash_index_t *rhash_first(apr_pool_t *p, rhash_t *ht)
{
	return apr_hash_first(p, ht->apr_hash);
}


/* Wrapper for apr_hash_next */
apr_hash_index_t *rhash_next(apr_hash_index_t *hi)
{
	return apr_hash_next(hi);
}


/* Wrapper for apr_hash_this */
void rhash_this(apr_hash_index_t *hi, const void **key, apr_ssize_t *klen, void **val)
{
	apr_hash_this(hi, key, klen, val);
}


/* Wrapper for apr_hash_count */
unsigned int rhash_count(rhash_t *ht)
{
	return apr_hash_count(ht->apr_hash);
}
