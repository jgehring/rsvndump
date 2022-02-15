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
 *      file: session.c
 *      desc: Subversion session abstraction
 */


#include <svn_auth.h>
#include <svn_client.h>
#include <svn_cmdline.h>
#include <svn_config.h>
#include <svn_fs.h>
#include <svn_path.h>
#include <svn_pools.h>
#include <svn_ra.h>
#include <svn_utf.h>

#include <time.h>

#include "main.h"
#include "utils.h"

#include "session.h"


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/


/* Creates a new obfuscated path name */
char *session_make_obfuscated(apr_pool_t *pool, apr_hash_t *taken)
{
	static const char rchars[] = {"abcdefghijklmnopqrstuvwxyz"};
	static const int probs[] = {
		81, 96, 124, 156, 283, 305, 325, 386, 456, 458, 466, 506, 530,
		597, 672, 691, 692, 752, 815, 906, 934, 944, 968, 970, 999, 1000
	};
	int i, j, len, lfac = 8;
	char *obf;

	do {
		len = 3 + (rand() % lfac++);
		obf = apr_pcalloc(pool, len+1);
		for (i = 0; i < len; i++) {
			int n = rand() % 1000;
			for (j = 0; j < (int)sizeof(rchars)-1; j++) {
				if (probs[j] >= n) break;
			}
			obf[i] = rchars[j];
		}
	} while (taken && apr_hash_get(taken, obf, APR_HASH_KEY_STRING) != NULL);

	if (taken) {
		apr_hash_set(taken, obf, APR_HASH_KEY_STRING, obf);
	}
	return obf;
}


/* Recursively obfuscate path components */
const char *session_obfuscate_rec(apr_pool_t *pool, apr_hash_t *hash, apr_hash_t *taken, const char *path)
{
	const char *obf = NULL, *dir, *base;

	utils_path_split(pool, path, &dir, &base);
	if (strcmp(dir, "/") && strlen(dir)) {
		dir = session_obfuscate_rec(pool, hash, taken, dir);
	}
	if (strlen(base) == 0) {
		return apr_pstrdup(pool, "");;
	}

	if ((obf = apr_hash_get(hash, base, APR_HASH_KEY_STRING)) == NULL) {
		obf = session_make_obfuscated(apr_hash_pool_get(hash), taken);
		apr_hash_set(hash, apr_pstrdup(apr_hash_pool_get(hash), base), APR_HASH_KEY_STRING, obf);
	}
	return (strlen(dir) ? utils_path_join(pool, dir, obf) : obf);
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/


/* Creates and initializes a new session_t object */
session_t session_create()
{
	session_t session;

	session.ra = NULL;
	session.url = NULL;
	session.encoded_url = NULL;
	session.root = NULL;
	session.prefix = NULL;
#ifdef USE_SINGLEFILE_DUMP
	session.file = 0;
#endif
	session.username = NULL;
	session.password = NULL;
	session.config_dir = NULL;
	session.flags = 0x00;

	session.pool = svn_pool_create(NULL);

	session.obf_hash = apr_hash_make(session.pool);
	session.obf_taken = apr_hash_make(session.pool);
	srand(time(NULL));

	return session;
}


/* Frees a session_t object */
void session_free(session_t *session)
{
	if (session->pool != NULL) {
		svn_pool_destroy(session->pool);
	}
}


/* Opens a session */
char session_open(session_t *session)
{
	svn_error_t *err;
	svn_client_ctx_t *ctx;
	svn_config_t *config;
	svn_auth_baton_t *auth_baton;
	const char *root;
	const char *config_dir = NULL;

	/* Make sure the URL is properly encoded */
	session->encoded_url = svn_path_uri_encode(svn_path_canonicalize(session->url, session->pool), session->pool);

	/* Do neccessary SVN library initialization */
	if ((err = svn_fs_initialize(session->pool))) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		return 1;
	}
	if ((err = svn_ra_initialize(session->pool))) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		return 1;
	}

	if ((err = svn_config_ensure(NULL, session->pool))) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		return 1;
	}

	/* Setup the client context */
	if ((err = svn_client_create_context(&ctx, session->pool))) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		return 1;
	}

	if ((err = svn_config_get_config(&(ctx->config), NULL, session->pool))) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		return 1;
	}

	if (session->config_dir != NULL) {
		const char *path;
		if ((err = svn_utf_cstring_to_utf8(&path, session->config_dir, session->pool))) {
			utils_handle_error(err, stderr, FALSE, "ERROR: ");
			svn_error_clear(err);
			return 1;
		}
		config_dir = svn_path_canonicalize(path, session->pool);
	}

	/* Setup auth baton */
	config = apr_hash_get(ctx->config, SVN_CONFIG_CATEGORY_CONFIG, APR_HASH_KEY_STRING);
	if ((err = svn_cmdline_setup_auth_baton(&auth_baton, (session->flags & SF_NON_INTERACTIVE), session->username, session->password, config_dir, (session->flags & SF_NO_AUTH_CACHE), config, NULL, NULL, session->pool))) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		return 1;
	}
	ctx->auth_baton = auth_baton;

	/* Setup the RA session */
	if ((err = svn_client_open_ra_session(&(session->ra), session->encoded_url, ctx, session->pool))) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		return 1;
	}

	/* Determine the root (and the prefix) of the URL */
	if ((err = svn_ra_get_repos_root(session->ra, &root, session->pool))) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		return 1;
	}
	session->root = root;
	if (!strcmp(session->encoded_url, root)) {
		session->prefix = apr_pstrdup(session->pool, "");
	} else {
		session->prefix = apr_pstrdup(session->pool, session->encoded_url + strlen(root) + 1);
	}
	session->prefix = session_obfuscate(session, session->pool, session->prefix);

	return 0;
}


/* Closes a session */
char session_close(session_t *session)
{
	svn_pool_clear(session->pool);
	return 0;
}


/* Reparents the session if the current root is a file */
char session_check_reparent(session_t *session, svn_revnum_t rev)
{
	svn_error_t *err;
	svn_node_kind_t kind;
	apr_pool_t *pool = svn_pool_create(session->pool);

	/* Check if the current root is a file */
	err = svn_ra_check_path(session->ra, "", rev, &kind, pool);
	if (err) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		svn_pool_destroy(pool);
		return 1;
	}

	if (kind == svn_node_file) {
#ifdef USE_SINGLEFILE_DUMP
		/* Determine the parent directory */
		const char *new_parent;
		svn_path_split(session->encoded_url, &new_parent, &session->file, session->pool);
		/* Reparent the session to the parent repository */
		err = svn_ra_reparent(session->ra, new_parent, pool);
		if (err) {
			utils_handle_error(err, stderr, FALSE, "ERROR: ");
			svn_error_clear(err);
			svn_pool_destroy(pool);
			return 1;
		}
#else
		/* Sorry, we're unable to dump single files for now */
		fprintf(stderr, _("ERROR: '%s' refers to a file.\n"), session->encoded_url);
		svn_pool_destroy(pool);
		return 1;
#endif
	}

	svn_pool_destroy(pool);
	return 0;
}


/* Returns the obfuscated name for a path if necessary */
const char *session_obfuscate(session_t *session, struct apr_pool_t *pool, const char *path)
{
	if (path == NULL || !(session->flags & SF_OBFUSCATE)) {
		return path;
	}
	return session_obfuscate_rec(pool, session->obf_hash, session->obf_taken, path);
}


/* Returns a one-time obfuscated string if necesssary */
const char *session_obfuscate_once(session_t *session, struct apr_pool_t *pool, const char *str)
{
	if (str== NULL || !(session->flags & SF_OBFUSCATE)) {
		return str;
	}
	return session_make_obfuscated(pool, NULL);
}
