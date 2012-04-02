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
 *      file: property.h
 *      desc: Convenience functions for dumping properties
 */


#ifndef PROPERTY_H_
#define PROPERTY_H_


/* Returns the length of a property */
extern size_t property_strlen(struct apr_pool_t *pool, const char *key, const char *value);

/* Returns the length of a property deletion */
extern size_t property_del_strlen(struct apr_pool_t *pool, const char *key);

/* Dumps a property to stdout */
extern void property_dump(const char *key, const char *value);

/* Dumps a property deletion to stdout */
extern void property_del_dump(const char *key);

/* Initializes the property storage, binding it to the given pool */
extern int property_store_init(const char *tmpdir, struct apr_pool_t *pool);

/* Saves the properties of the given path */
extern int property_store(const char *path, struct apr_hash_t *props, struct apr_pool_t *pool);

/* Loads the properties of the given path and removes them from the storage */
extern int property_load(const char *path, struct apr_hash_t *props, struct apr_pool_t *pool);

/* Removes the properties of the given path from the storage */
extern int property_delete(const char *path, struct apr_pool_t *pool);


#endif
