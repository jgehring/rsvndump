/*
 *	rsvndump - remote svn repository dump
 *	Copyright (C) 2008 Jonas Gehring
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
 *      file: whash.h
 *      desc: A simple wrapper for apr_hash_t supporting strings
 */


#ifndef WHASH_H 
#define WHASH_H


/* Creates the hash table */
extern char whash_create();

/* Frees the hash table */
extern void whash_free();

/* Inserts a string into the hash */
extern void whash_insert(const char *key);

/* Provides string lookup */
extern char whash_contains(const char *key);


#endif
