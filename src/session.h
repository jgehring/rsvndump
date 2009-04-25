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
 *      file: session.h
 *      desc: Subversion session abstraction
 */


#ifndef SESSION_H_
#define SESSION_H_


#include <svn_types.h>


/* Flags for the session data */
enum session_flags {
	SF_NO_CHECK_CERTIFICATE = 0x01
};

/* Session data */
typedef struct {
	struct svn_ra_session_t	*ra;
	struct apr_pool_t	*pool;
	char			*url;
	const char		*encoded_url;
	const char		*root;
	char			*prefix;
	const char		*file; /* Only set if the target is a file */
	char			*username;
	char			*password;
	int			flags;
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


#endif
