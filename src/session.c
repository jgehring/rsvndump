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
 *      file: session.c
 *      desc: Subversion session abstraction
 */


#include <svn_auth.h>
#include <svn_client.h>
#include <svn_config.h>
#include <svn_fs.h>
#include <svn_path.h>
#include <svn_pools.h>
#include <svn_ra.h>

#include <apr_lib.h>

#include "main.h"

#include "session.h"


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/


/* Reads a string from the command line */
static svn_error_t *session_read_line(const char *prompt, char *buffer, size_t max, char pass)
{
	if (!pass) {
		int len;
		fprintf(stderr, "%s", prompt);
		if (fgets(buffer, max, stdin) == NULL) {
			return svn_error_create(0, NULL, "error reading from stdin");
		}
		len = strlen(buffer);
		if (len > 0 && buffer[len-1] == '\n') {
			buffer[len-1] = '\0';
		}
	} else {
		if (apr_password_get(prompt, buffer, &max)) {
			return svn_error_create(0, NULL, "error reading from stdin");
		}
	}

	return SVN_NO_ERROR;
}


/* Try to authenticate with given arguments */
static svn_error_t *session_auth_arguments(svn_auth_cred_simple_t **cred, void *baton, const char *realm, const char *username, svn_boolean_t may_save, apr_pool_t *pool)
{
	svn_auth_cred_simple_t *ret = apr_pcalloc(pool, sizeof(*ret));
	session_t *session = (session_t *)baton;

	DEBUG_MSG("session_auth_arguments() called with realm=%s username=%s\n", realm, username);

	if (session->username) {
		ret->username = apr_pstrdup(pool, session->username);
	} else if (username) {
		ret->username = apr_pstrdup(pool, username);
	} else {
		ret->username = apr_pstrdup(pool, "\0");
	}
	if (session->password) {
		ret->password = apr_pstrdup(pool, session->password);
	} else {
		ret->password = apr_pstrdup(pool, "\0");
	}

	*cred = ret;
	return SVN_NO_ERROR;
}


/* Tries to authenticate via a prompt  */
static svn_error_t *session_auth_prompt(svn_auth_cred_simple_t **cred, void *baton, const char *realm, const char *username, svn_boolean_t may_save, apr_pool_t *pool)
{
	svn_auth_cred_simple_t *ret = apr_pcalloc(pool, sizeof(*ret));
	session_t *session = (session_t *)baton;
	char answerbuf[128];
	char *prompt;

	DEBUG_MSG("session_auth_prompt() called with realm=%s username=%s\n", realm, username);

	if (session->username) {
		ret->username = apr_pstrdup(pool, session->username);
	} else if (username) {
		ret->username = apr_pstrdup(pool, username);
	} else {
		SVN_ERR(session_read_line(_("Username: "), answerbuf, sizeof(answerbuf), 0));
		ret->username = apr_pstrdup(pool, answerbuf);
	}

	if (ret->username && strnlen(ret->username, 128) < 128) {
		char *format = _("Password for '%s': ");
		prompt = apr_pcalloc(pool, strlen(ret->username) + strlen(format) + 1);
		sprintf(prompt, format, ret->username);
	} else {
		prompt = apr_pstrdup(pool, _("Password: "));
	}

	SVN_ERR(session_read_line(prompt, answerbuf, sizeof(answerbuf), 1));
	ret->password = apr_pstrdup(pool, answerbuf);
	*cred = ret;
	return SVN_NO_ERROR;
}

/* SSL certificate authentication */
static svn_error_t *session_auth_ssl_trust(svn_auth_cred_ssl_server_trust_t **cred, void *baton, const char *realm, apr_uint32_t failures, const svn_auth_ssl_server_cert_info_t *cert_info, svn_boolean_t may_save, apr_pool_t *pool)
{
	session_t *session = (session_t *)baton;
	char choice[3];
	/* TRANSLATORS: This is the key used for accepting a certificate temporarily.
	 * Every other key is used to reject the certificate. */
	const char *accept = _("t");

	if (failures & SVN_AUTH_SSL_UNKNOWNCA) {
		if (session->flags & SF_NO_CHECK_CERTIFICATE) {
			fprintf(stderr, _("WARNING: Validating server certificate for '%s' failed:\n"), realm);
		} else {
			fprintf(stderr, _("Error validating server certificate for '%s':\n"), realm);
		}
		fprintf(stderr, _(" - The certificate is not issued by a trusted authority. Use the\n" \
		                  "   fingerprint to validate the certificate manually!\n"));
	}
	if (failures & SVN_AUTH_SSL_CNMISMATCH) {
		if (session->flags & SF_NO_CHECK_CERTIFICATE) {
			fprintf(stderr, _("WARNING: Validating server certificate for '%s' failed:\n"), realm);
		} else {
			fprintf(stderr, _("Error validating server certificate for '%s':\n"), realm);
		}
		fprintf(stderr, _(" - The certificate hostname does not match.\n"));
	}
	if (failures & SVN_AUTH_SSL_NOTYETVALID) {
		if (session->flags & SF_NO_CHECK_CERTIFICATE) {
			fprintf(stderr, _("WARNING: Validating server certificate for '%s' failed:\n"), realm);
		} else {
			fprintf(stderr, _("Error validating server certificate for '%s':\n"), realm);
		}
		fprintf(stderr, _(" - The certificate is not yet valid.\n"));
	}
	if (failures & SVN_AUTH_SSL_EXPIRED) {
		if (session->flags & SF_NO_CHECK_CERTIFICATE) {
			fprintf(stderr, _("WARNING: Validating server certificate for '%s' failed:\n"), realm);
		} else {
			fprintf(stderr, _("Error validating server certificate for '%s':\n"), realm);
		}
		fprintf(stderr, _(" - The certificate has expired.\n"));
	}
	if (failures & SVN_AUTH_SSL_OTHER) {
		if (session->flags & SF_NO_CHECK_CERTIFICATE) {
			fprintf(stderr, _("WARNING: Validating server certificate for '%s' failed:\n"), realm);
		} else {
			fprintf(stderr, _("Error validating server certificate for '%s':\n"), realm);
		}
		fprintf(stderr, _(" - The certificate has an unknown error.\n"));
	}

	fprintf(stderr, _("Certificate information:\n"
			" - Hostname: %s\n"
			" - Valid: from %s until %s\n"
			" - Issuer: %s\n"
			" - Fingerprint: %s\n"),
			cert_info->hostname,
			cert_info->valid_from,
			cert_info->valid_until,
			cert_info->issuer_dname,
			cert_info->fingerprint);

	if (session->flags & SF_NO_CHECK_CERTIFICATE) {
		*cred = apr_pcalloc(pool, sizeof (**cred));
		(*cred)->may_save = FALSE;
		(*cred)->accepted_failures = failures;
		return SVN_NO_ERROR;
	}

	fprintf(stderr, _("(R)eject or accept (t)emporarily? "));
	SVN_ERR(session_read_line(" ", choice, sizeof(choice), 0));

	if (tolower(choice[0]) == *accept) {
		*cred = apr_pcalloc(pool, sizeof (**cred));
		(*cred)->may_save = FALSE;
		(*cred)->accepted_failures = failures;
	}
	else {
		*cred = NULL;
	}

	return SVN_NO_ERROR;
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
	session.flags = 0x00;

	session.pool = svn_pool_create(NULL);

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
	svn_auth_provider_object_t *provider;
	apr_array_header_t *providers;
	const char *root;

	/* Make sure the URL is properly encoded */
	session->encoded_url = svn_path_uri_encode(svn_path_canonicalize(session->url, session->pool), session->pool);

	/* Do neccessary SVN library initialization */
	if ((err = svn_fs_initialize(session->pool))) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		return 1;
	}
	if ((err = svn_ra_initialize(session->pool))) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		return 1;
	}

	if ((err = svn_config_ensure(NULL, session->pool))) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		return 1;
	}

	/* Setup the client context */
	if ((err = svn_client_create_context(&ctx, session->pool))) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		return 1;
	}

	if ((err = svn_config_get_config(&(ctx->config), NULL, session->pool))) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		return 1;
	}

	/* Setup the authentication providers */
	providers = apr_array_make(session->pool, 4, sizeof (svn_auth_provider_object_t *));
	svn_auth_get_username_provider(&provider, session->pool);
	APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

	svn_auth_get_simple_prompt_provider(&provider, session_auth_arguments, session, 2, session->pool);
	APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

	svn_auth_get_simple_prompt_provider(&provider, session_auth_prompt, session, 2, session->pool);
	APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

	svn_auth_get_ssl_server_trust_prompt_provider(&provider, session_auth_ssl_trust, session, session->pool);
	APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

	svn_auth_open(&ctx->auth_baton, providers, session->pool);

	/* Setup the RA session */
	if ((err = svn_client_open_ra_session(&(session->ra), session->encoded_url, ctx, session->pool))) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		return 1;
	}

	/* Determine the root (and the prefix) of the URL */
	if ((err = svn_ra_get_repos_root(session->ra, &root, session->pool))) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		return 1;
	}
	session->root = root;
	if (!strcmp(session->encoded_url, root)) {
		session->prefix = apr_pstrdup(session->pool, "");
	} else {
		session->prefix = apr_pstrdup(session->pool, session->encoded_url + strlen(root) + 1);
	}

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
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
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
			svn_handle_error2(err, stderr, FALSE, APPNAME": ");
			svn_error_clear(err);
			svn_pool_destroy(pool);
			return 1;
		}
#else
		/* Sorry, we're unable to dump single files for now */
		fprintf(stderr, _("ERROR: '%s' refers to a file\n"), session->encoded_url);
		svn_pool_destroy(pool);
		return 1;
#endif
	}

	svn_pool_destroy(pool);
	return 0;
}
