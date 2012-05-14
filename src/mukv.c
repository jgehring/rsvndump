/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008-2012 Jonas Gehring
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
 *      file: mukv.c
 *      desc: Small and simple key-value storage
 */


#include <errno.h>
#include <stdio.h>
#ifndef WIN32
	#include <unistd.h>
#endif

#include <apr_strings.h>

#include "main.h"
#include "rhash.h"

#include "mukv.h"


struct mukv_t {
	rhash_t *index;
	char *path;
	FILE *file;
};

typedef struct {
	long off;
	size_t size;
} entry_t;


/* Opens a file to be used for random-accesible storage */
mukv_t *mukv_open(const char *path, apr_pool_t *pool)
{
	mukv_t *kv = apr_palloc(pool, sizeof(mukv_t));
	kv->index = rhash_make(pool);
	kv->path = apr_pstrdup(pool, path);
	if ((kv->file = fopen(path, "w+")) == NULL) {
		return NULL;
	}
	return kv;
}

/* Closes the storage and sends it into oblivion */
int mukv_close(mukv_t *kv)
{
	rhash_clear(kv->index);

	/* Goodbye, data */
	if (fclose(kv->file) != 0 || unlink(kv->path) != 0) {
		return errno;
	}
	return 0;
}

/* Stores a record */
int mukv_store(mukv_t *kv, mdatum_t key, mdatum_t val)
{
	entry_t entry;
	if (fseek(kv->file, 0, SEEK_END) != 0) {
		return errno;
	}
	entry.off = ftell(kv->file);
	entry.size = val.dsize;

	if (fwrite(val.dptr, 1, val.dsize, kv->file) != val.dsize) {
		return errno;
	}

	rhash_set(kv->index, key.dptr, key.dsize, &entry, sizeof(entry_t));
	return 0;
}

/* Retrieves a record */
mdatum_t mukv_fetch(mukv_t *kv, mdatum_t key, apr_pool_t *pool)
{
	entry_t *entry;
	mdatum_t val;

	val.dptr = NULL;
	val.dsize = 0;
	entry = rhash_get(kv->index, key.dptr, key.dsize);
	if (entry == NULL) {
		return val;
	}

	if (ftell(kv->file) != entry->off) {
		if (fseek(kv->file, entry->off, SEEK_SET) != 0) {
			return val;
		}
	}
	val.dptr = apr_palloc(pool, entry->size);
	if (fread(val.dptr, 1, entry->size, kv->file) != entry->size) {
		val.dptr = NULL;
		return val;
	}
	val.dsize = entry->size;
	return val;
}

/* Deletes a record (from the index, not from the disk) */
int mukv_delete(mukv_t *kv, mdatum_t key)
{
	entry_t *entry = rhash_get(kv->index, key.dptr, key.dsize);
	if (entry) {
		rhash_set(kv->index, key.dptr, key.dsize, NULL, 0);
	}
	return 0;
}

/* Checks whether a record exists */
int mukv_exists(mukv_t *kv, mdatum_t key)
{
	return (rhash_get(kv->index, key.dptr, key.dsize) != NULL);
}
