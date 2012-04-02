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
 *      file: property.c
 *      desc: Convenience functions for dumping properties and a persistant
 *            property storage.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <svn_pools.h>
#include <svn_string.h>
#include <svn_md5.h>

#include <apr_file_io.h>
#include <apr_hash.h>
#include <apr_md5.h>
#include <apr_strings.h>

#include <gdbm.h>

#include "main.h"

#include "property.h"


/*---------------------------------------------------------------------------*/
/* Local data structures                                                     */
/*---------------------------------------------------------------------------*/


/* Property reference */
typedef struct {
	unsigned char id[APR_MD5_DIGESTSIZE];
	int count;  /* Reference counter */
} prop_ref_t;


/* Referenced property reference */
typedef struct {
	char *path;
	prop_ref_t *ref;
} prop_entry_t;


/* Property storage */
struct property_storage_t {
	apr_pool_t *pool;
	apr_hash_t *refs;      /* Property IDs to reference */
	apr_hash_t *entries;   /* Path to property ID pointer */
	GDBM_FILE db;          /* DBM: ID to property data */
};


/*---------------------------------------------------------------------------*/
/* Local functions                                                           */
/*---------------------------------------------------------------------------*/


/* Cleanup handler for the global pool */
static apr_status_t prop_cleanup(void *param)
{
	property_storage_t *store = param;
	apr_hash_index_t *hi;
	const void *key;
	apr_ssize_t klen;
	void *value;

	/* Manually delete hash data */
	for (hi = apr_hash_first(store->pool, store->refs); hi; hi = apr_hash_next(hi)) {
		apr_hash_this(hi, &key, &klen, &value);
		free(value);
	}
	for (hi = apr_hash_first(store->pool, store->entries); hi; hi = apr_hash_next(hi)) {
		apr_hash_this(hi, &key, &klen, &value);
		free(((prop_entry_t *)value)->path);
		free(value);
	}

	gdbm_close(store->db);
	return APR_SUCCESS;
}


/* Serializes a hash to a simple string format */
static int prop_hash_serialize(char **data, size_t *len, apr_hash_t *props, apr_pool_t *pool)
{
	apr_hash_index_t *hi;
	char *bptr;

	/* Determine length of data first */
	*len = 0;
	for (hi = apr_hash_first(pool, props); hi; hi = apr_hash_next(hi)) {
		const char *key;
		svn_string_t *value;
		apr_hash_this(hi, (const void **)(void *)&key, NULL, (void **)(void *)&value);

		*len += sizeof(int) * 2;
		*len += strlen(key) + value->len;
	}
	*len += sizeof(int); /* Final zero integer */

	if ((*data = apr_palloc(pool, *len)) == NULL) {
		return -1;
	}

	/* Encode */
	bptr = *data;
	for (hi = apr_hash_first(pool, props); hi; hi = apr_hash_next(hi)) {
		const char *key;
		svn_string_t *value;
		apr_hash_this(hi, (const void **)(void *)&key, NULL, (void **)(void *)&value);

		*(int *)bptr = strlen(key);
		strcpy(bptr + sizeof(int), key);
		bptr += sizeof(int) + *(int *)bptr;

		*(int *)bptr = value->len;
		memcpy(bptr + sizeof(int), value->data, value->len);
		bptr += sizeof(int) + *(int *)bptr;
	}

	*(int *)bptr = 0; /* End of data */
	return 0;
}


/* Reconstructs a property hash from its serialized form */
static int prop_hash_reconstruct(apr_hash_t *props, const char *data, apr_pool_t *pool)
{
	const char *bptr = data;
	while (*(int *)bptr) {
		int len;
		char *key;
		svn_string_t *value;

		len = *(int *)bptr;
		key = apr_pstrndup(pool, bptr + sizeof(int), len);
		bptr += sizeof(int) + len;

		len = *(int *)bptr;
		value = svn_string_ncreate(bptr + sizeof(int), len, pool);
		bptr += sizeof(int) + len;

		apr_hash_set(props, key, APR_HASH_KEY_STRING, value);
	}
	return 0;
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/


/* Returns the length of a property */
size_t property_strlen(struct apr_pool_t *pool, const char *key, const char *value)
{
	size_t len = 0;

	if (key == NULL) {
		return 0;
	}
	if (value == NULL) {
		len = strlen(apr_psprintf(pool, "K %lu\n%s\nV 0\n\n", (unsigned long)strlen(key), key));
	} else {
		len = strlen(apr_psprintf(pool, "K %lu\n%s\nV %lu\n%s\n", (unsigned long)strlen(key), key, (unsigned long)strlen(value), value));
	}

	return len;
}


/* Returns the length of a property deletion */
size_t property_del_strlen(struct apr_pool_t *pool, const char *key)
{
	size_t len = 0;

	if (key == NULL) {
		return 0;
	}
	len = strlen(apr_psprintf(pool, "D %lu\n%s\n", (unsigned long)strlen(key), key));

	return len;
}


/* Dumps a property to stdout */
void property_dump(const char *key, const char *value)
{
	/* NOTE: This is duplicated in property_hash_write */
	if (key == NULL) {
		return;
	}
	printf("K %lu\n", (unsigned long)strlen(key));
	printf("%s\n", key);
	if (value != NULL) {
		printf("V %lu\n", (unsigned long)strlen(value));
		printf("%s\n", value);
	} else {
		printf("V 0\n\n");
	}
}


/* Dumps a property deletion to stdout */
void property_del_dump(const char *key)
{
	if (key == NULL) {
		return;
	}
	printf("D %lu\n", (unsigned long)strlen(key));
	printf("%s\n", key);
}


/* Initializes the property storage, binding it to the given pool */
property_storage_t *property_storage_create(const char *tmpdir, apr_pool_t *pool)
{
	char *db_path;
	property_storage_t *store = apr_pcalloc(pool, sizeof(property_storage_t));

	if ((store->pool = svn_pool_create(pool)) == NULL) {
		fprintf(stderr, "Error creating property storage: out of memory\n");
		return NULL;
	}

	store->refs = apr_hash_make(store->pool);
	store->entries = apr_hash_make(store->pool);

	/* Open database */
	db_path = apr_psprintf(store->pool, "%s/props.db", tmpdir);
	store->db = gdbm_open(db_path, 0, GDBM_NEWDB, 0600, NULL);
	if (store->db == NULL) {
		fprintf(stderr, "Error creating path database (%s)\n", gdbm_strerror(gdbm_errno));
		return NULL;
	}

	apr_pool_cleanup_register(store->pool, store, prop_cleanup, NULL);
	return store;
}


/* Saves the properties of the given path */
int property_store(property_storage_t *store, const char *path, apr_hash_t *props, apr_pool_t *pool)
{
	size_t len;
	char *data;
	unsigned char id[APR_MD5_DIGESTSIZE];
	prop_ref_t *ref;
	prop_entry_t *entry;

	entry = apr_hash_get(store->entries, path, APR_HASH_KEY_STRING);

	/* No work for empty property hashes */
	if (apr_hash_count(props) == 0) {
		apr_hash_set(store->entries, path, APR_HASH_KEY_STRING, NULL);
		if (entry) {
			free(entry);
		}
		return 0;
	}

	/* Serialize and generate id */
	if (prop_hash_serialize(&data, &len, props, pool) != 0) {
		return -1;
	}
	if (apr_md5(id, data, len) != APR_SUCCESS){
		return -1;
	}

	/* Check if this ID is already present */
	if ((ref = apr_hash_get(store->refs, id, sizeof(id))) == NULL) {
		datum key, value;

		ref = malloc(sizeof(prop_ref_t));
		if (ref == NULL) {
			return -1;
		}
		memcpy(ref->id, id, sizeof(id));
		ref->count = 0;
		apr_hash_set(store->refs, ref->id, sizeof(id), ref);

		/* Add new ID -> data mapping to database */
		key.dptr = (char *)id;
		key.dsize = sizeof(id);
		value.dptr = data;
		value.dsize = len+1;
		if (gdbm_store(store->db, key, value, GDBM_INSERT) != 0) {
			return -1;
		}
	}

	/* Add entry */
	if (entry == NULL) {
		entry = malloc(sizeof(prop_entry_t));
		if (entry == NULL) {
			return -1;
		}
		if ((entry->path = strdup(path)) == NULL) {
			return -1;
		}
	}
	entry->ref = ref;
	ref->count++;
	apr_hash_set(store->entries, entry->path, APR_HASH_KEY_STRING, entry);
	return 0;
}


/* Loads the properties of the given path (and removes the corresponding entry) */
int property_load(property_storage_t *store, const char *path, apr_hash_t *props, apr_pool_t *pool)
{
	datum key, value;
	prop_entry_t *entry;
	
	/* Check if path has properties attached */
	if ((entry = apr_hash_get(store->entries, path, APR_HASH_KEY_STRING)) == NULL) {
		return 0;
	}

	/* Retrieve item from database */
	key.dptr = (char *)entry->ref->id;
	key.dsize = APR_MD5_DIGESTSIZE;
	value = gdbm_fetch(store->db, key);
	if (value.dptr == NULL) {
		return -1;
	}

	/* Reconstruct hash */
	if (prop_hash_reconstruct(props, value.dptr, pool) != 0) {
		return -1;
	}

	/* TODO: Don't remove entry */
	/* Remove entry */
	apr_hash_set(store->entries, path, APR_HASH_KEY_STRING, NULL);
	entry->ref->count--;

	/* Remove item from database if reference count is zero */
	if (entry->ref->count <= 0) {
		apr_hash_set(store->refs, entry->ref->id, APR_MD5_DIGESTSIZE, NULL);
		if (gdbm_delete(store->db, key) != 0) {
			return -1;
		}
		free(entry->ref);
	}
	free(entry->path);
	free(entry);
	free(value.dptr);
	return 0;
}


/* Removes the properties of the given path from the storage */
int property_delete(property_storage_t *store, const char *path, apr_pool_t *pool)
{
	datum key;
	prop_entry_t *entry;

	/* Check if path has properties attached */
	if ((entry = apr_hash_get(store->entries, path, APR_HASH_KEY_STRING)) == NULL) {
		return 0;
	}

	/* Remove entry */
	apr_hash_set(store->entries, path, APR_HASH_KEY_STRING, NULL);
	entry->ref->count--;

	/* Remove item from database if reference count is zero */
	if (entry->ref->count <= 0) {
		apr_hash_set(store->refs, entry->ref->id, APR_MD5_DIGESTSIZE, NULL);
		if (gdbm_delete(store->db, key) != 0) {
			return -1;
		}
		free(entry->ref);
	}
	free(entry->path);
	free(entry);
	return 0;
}
