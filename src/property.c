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
 *      file: property.c
 *      desc: Convenience functions for dumping properties
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <svn_pools.h>
#include <svn_string.h>

#include <apr_file_io.h>
#include <apr_hash.h>
#include <apr_strings.h>

#include "property.h"


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
		len = strlen(apr_psprintf(pool, "K %d\n%s\nV 0\n\n", strlen(key), key));
	} else {
		len = strlen(apr_psprintf(pool, "K %d\n%s\nV %d\n%s\n", strlen(key), key, strlen(value), value));
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
	len = strlen(apr_psprintf(pool, "D %d\n%s\n", strlen(key), key));

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


/* Writes a hash of properties to a given file */
void property_hash_write(struct apr_hash_t *hash, struct apr_file_t *file, struct apr_pool_t *pool)
{
	apr_hash_index_t *hi;
	for (hi = apr_hash_first(pool, hash); hi; hi = apr_hash_next(hi)) {
		const char *key;
		svn_string_t *value;
		apr_hash_this(hi, (const void **)(void *)&key, NULL, (void **)(void *)&value);

		/* NOTE: This is the same as property_dump() */
		if (key == NULL) {
			return;
		}
		apr_file_printf(file, "K %lu\n", (unsigned long)strlen(key));
		apr_file_printf(file, "%s\n", key);
		if (value != NULL) {
			apr_file_printf(file, "V %lu\n", (unsigned long)strlen(value->data));
			apr_file_printf(file, "%s\n", value->data);
		} else {
			apr_file_printf(file, "V 0\n\n");
		}
	}
}


/* Loads a hash of properties from a given file, storing the properties using the given pool */
char property_hash_load(struct apr_hash_t *hash, struct apr_file_t *file, struct apr_pool_t *pool)
{
	apr_pool_t *subpool = svn_pool_create(pool);
	apr_off_t skip = 1;
	const int maxlen = 512;
	char *buffer = apr_palloc(subpool, maxlen+1);

	while (!APR_STATUS_IS_EOF(apr_file_eof(file))) {
		unsigned long dlen;
		char end; /* Used for EOL detection */
		char *keybuffer, *valbuffer;

		/* Read key */
		if (apr_file_gets(buffer, maxlen, file) != APR_SUCCESS) {
			break;
		}
		if ((sscanf(buffer, "K %lu%c", &dlen, &end) != 2) || (end != '\n')) {
			break;
		}
		keybuffer = apr_pcalloc(subpool, dlen+1);
		if (apr_file_read(file, keybuffer, (apr_size_t *)&dlen) != APR_SUCCESS) {
			break;
		}

		/* Skip newline */
		apr_file_seek(file, APR_CUR, &skip);
		skip = 1;

		/* Read value */
		if (apr_file_gets(buffer, maxlen, file) != APR_SUCCESS) {
			break;
		}
		if ((sscanf(buffer, "V %lu%c", &dlen, &end) != 2) || (end != '\n')) {
			break;
		}
		if (dlen == 0) {
			apr_hash_set(hash, apr_pstrdup(pool, keybuffer), APR_HASH_KEY_STRING, svn_string_ncreate("", 0, pool));
		} else {
			valbuffer = apr_pcalloc(subpool, dlen+1);
			if (apr_file_read(file, valbuffer, (apr_size_t *)&dlen) != APR_SUCCESS) {
				break;
			}
			apr_hash_set(hash, apr_pstrdup(pool, keybuffer), APR_HASH_KEY_STRING, svn_string_create(valbuffer, pool));
		}

		/* Skip newline */
		apr_file_seek(file, APR_CUR, &skip);
		skip = 1;
	}

	svn_pool_destroy(subpool);
	return !APR_STATUS_IS_EOF(apr_file_eof(file));
}
