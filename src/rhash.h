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
 *      file: rhash.h
 *      desc: Interface for apr_hash_t with special memory handling
 */


#ifndef RHASH_H_
#define RHASH_H_


#include <apr_hash.h>
#include <apr_pools.h>


#define RHASH_VAL_STRING APR_HASH_KEY_STRING


typedef struct rhash_t rhash_t;


/* Creates a new rhash */
extern rhash_t *rhash_make(apr_pool_t *pool);

/* Clears and frees an rhash */
extern void rhash_clear(rhash_t *ht);

/* Wrapper for apr_hash_set */
extern void rhash_set(rhash_t *ht, const void *key, apr_ssize_t klen, const void *val, apr_ssize_t vlen);

/* Wrapper for apr_hash_get */
extern void *rhash_get(rhash_t *ht, const void *key, apr_ssize_t klen);

/* Wrapper for apr_hash_first */
extern apr_hash_index_t *rhash_first(apr_pool_t *p, rhash_t *ht);

/* Wrapper for apr_hash_next */
extern apr_hash_index_t *rhash_next(apr_hash_index_t *hi);

/* Wrapper for apr_hash_this */
extern void rhash_this(apr_hash_index_t *hi, const void **key, apr_ssize_t *klen, void **val);

/* Wrapper for apr_hash_count */
unsigned int rhash_count(rhash_t *ht);


#endif
