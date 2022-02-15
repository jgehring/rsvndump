/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008-present Jonas Gehring
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
 *      file: mukv.h
 *      desc: Small and simple key-value storage
 */


#ifndef MUKV_H_
#define MUKV_H_


#include <apr_pools.h>


typedef struct mukv_t mukv_t;

typedef struct {
	char *dptr;
	size_t dsize;
} mdatum_t;


/* Opens a file to be used for random-accesible storage */
extern mukv_t *mukv_open(const char *path, apr_pool_t *pool);

/* Closes the storage and sends it into oblivion */
extern int mukv_close(mukv_t *kv);

/* Stores a record */
extern int mukv_store(mukv_t *kv, mdatum_t key, mdatum_t val);

/* Retrieves a record */
extern mdatum_t mukv_fetch(mukv_t *kv, mdatum_t key, apr_pool_t *pool);

/* Deletes a record (from the index, not from the disk) */
extern int mukv_delete(mukv_t *kv, mdatum_t key);

/* Checks whether a record exists */
extern int mukv_exists(mukv_t *kv, mdatum_t key);


#endif /* MUKV_H_ */
