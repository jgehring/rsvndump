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
 *      file: session.h
 *      desc: Subversion session abstraction
 */


#ifndef SESSION_H_
#define SESSION_H_


#include "main.h"

#include <svn_types.h>


/* Flags for the session data */
enum session_flags {
	SF_NO_CHECK_CERTIFICATE = 0x01,
	SF_NON_INTERACTIVE = 0x02,
	SF_NO_AUTH_CACHE = 0x04,
	SF_OBFUSCATE = 0x08
};

/* Session data */
typedef struct {
	struct svn_ra_session_t *ra;
	struct apr_pool_t *pool;
	char *url;
	const char *encoded_url;
	const char *root;
	const char *prefix;
#ifdef USE_SINGLEFILE_DUMP
	const char *file; /* Only set if the target is a file */
#endif
	char *username;
	char *password;
	char *config_dir;
	int flags;

	struct apr_hash_t *obf_hash;
	struct apr_hash_t *obf_taken;
} session_t;


/* Creates and initializes a new session_t object */
extern session_t session_create();

/* Frees a session_t object */
extern void session_free(session_t *session);

/* Opens a session */
extern char session_open(session_t *session);

/* Closes a session */
extern char session_close(session_t *session);

/* Reparents the session if the current root is a file */
extern char session_check_reparent(session_t *session, svn_revnum_t rev);

/* Returns the obfuscated name for a path if necessary */
extern const char *session_obfuscate(session_t *session, struct apr_pool_t *pool, const char *path);

/* Returns a one-time obfuscated string if necesssary */
extern const char *session_obfuscate_once(session_t *session, struct apr_pool_t *pool, const char *str);


#endif
