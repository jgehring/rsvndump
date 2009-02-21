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
 *      file: wsvn.h
 *      desc: Wrapper functions for Subversion API 
 */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <svn_auth.h>
#include <svn_client.h>
#include <svn_cmdline.h>
#include <svn_fs.h>
#include <svn_path.h>
#include <svn_pools.h>
#include <svn_ra.h>
#include <svn_sorts.h>
#include <svn_wc.h>

#include <apr_lib.h>

#include "wsvn.h"
#include "logentry.h"
#include "main.h"


/*---------------------------------------------------------------------------*/
/* Local data structures                                                     */
/*---------------------------------------------------------------------------*/

typedef struct {
	char **root;
	char **uuid;
	svn_revnum_t *rev;
} info_t;


/*---------------------------------------------------------------------------*/
/* Static variables                                                          */
/*---------------------------------------------------------------------------*/

static apr_pool_t *mainpool = NULL;
static svn_ra_session_t *session = NULL;
static svn_client_ctx_t *ctx = NULL;
static dump_options_t *dopts = NULL;
static char wc_setup = 0;


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/

/* Reads a string from the command line */
static svn_error_t *wsvn_read_line(const char *prompt, char *buffer, size_t max, char pass)
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
static svn_error_t *wsvn_auth_arguments(svn_auth_cred_simple_t **cred, void *baton, const char *realm, const char *username, svn_boolean_t may_save, apr_pool_t *pool)
{
#if DEBUG
	fprintf(stderr, "wsvn_auth_arguments() called with realm=%s username=%s\n", realm, username);
#endif
	svn_auth_cred_simple_t *ret = apr_pcalloc(pool, sizeof(*ret));

	if (dopts->username) {
		ret->username = apr_pstrdup(pool, dopts->username);
	} else if (username) {
		ret->username = apr_pstrdup(pool, username);
	} else {
		ret->username = apr_pstrdup(pool, "\0");
	}
	if (dopts->password) {
		ret->password = apr_pstrdup(pool, dopts->password);
	} else {
		ret->password = apr_pstrdup(pool, "\0");
	}

	*cred = ret;
	return SVN_NO_ERROR;
}


/* Try to authenticate via a prompt  */
static svn_error_t *wsvn_auth_prompt(svn_auth_cred_simple_t **cred, void *baton, const char *realm, const char *username, svn_boolean_t may_save, apr_pool_t *pool)
{
#if DEBUG
	fprintf(stderr, "wsvn_auth_prompt() called with realm=%s username=%s\n", realm, username);
#endif
	svn_auth_cred_simple_t *ret = apr_pcalloc(pool, sizeof(*ret));
	char answerbuf[128];
	char *prompt;

	if (dopts->username) {
		ret->username = apr_pstrdup(pool, dopts->username);
	} else if (username) {
		ret->username = apr_pstrdup(pool, username);
	} else {
		SVN_ERR(wsvn_read_line("Username: ", answerbuf, sizeof(answerbuf), 0));
		ret->username = apr_pstrdup(pool, answerbuf);
	}

	if (ret->username && strnlen(ret->username, 128) < 128) {
		char *format = "Password for '%s':";
		prompt = apr_pcalloc(pool, strlen(ret->username) + strlen(format) + 1);
		sprintf(prompt, format, ret->username);
	} else {
		prompt = apr_pstrdup(pool, "Password: ");
	}

	SVN_ERR(wsvn_read_line(prompt, answerbuf, sizeof(answerbuf), 1));
	ret->password = apr_pstrdup(pool, answerbuf);
	*cred = ret;
	return SVN_NO_ERROR;
}

/* SSL certificate authentication */
static svn_error_t *wsvn_auth_ssl_trust(svn_auth_cred_ssl_server_trust_t **cred, void *baton, const char *realm, apr_uint32_t failures, const svn_auth_ssl_server_cert_info_t *cert_info, svn_boolean_t may_save, apr_pool_t *pool)
{
	char choice[3];

	if (failures & SVN_AUTH_SSL_UNKNOWNCA) {
		fprintf(stderr, "Error validating server certificate for '%s':\n", realm);
		fprintf(stderr, " - The certificate is not issued by a trusted authority. Use the\n");
		fprintf(stderr, "   fingerprint to validate the certificate manually!\n");
	}
	if (failures & SVN_AUTH_SSL_CNMISMATCH) {
		fprintf(stderr, "Error validating server certificate for '%s':\n", realm);
		fprintf(stderr, " - The certificate hostname does not match.\n");
	}
	if (failures & SVN_AUTH_SSL_NOTYETVALID) {
		fprintf(stderr, "Error validating server certificate for '%s':\n", realm);
		fprintf(stderr, " - The certificate is not yet valid.\n");
	}
	if (failures & SVN_AUTH_SSL_EXPIRED) {
		fprintf(stderr, "Error validating server certificate for '%s':\n", realm);
		fprintf(stderr, " - The certificate has expired.\n");
	}
	if (failures & SVN_AUTH_SSL_OTHER) {
		fprintf(stderr, "Error validating server certificate for '%s':\n", realm);
		fprintf(stderr, " - The certificate has an unknown error.\n");
	}

	fprintf(stderr, "Certificate information:\n"
			" - Hostname: %s\n"
			" - Valid: from %s until %s\n"
			" - Issuer: %s\n"
			" - Fingerprint: %s\n",
			cert_info->hostname,
			cert_info->valid_from,
			cert_info->valid_until,
			cert_info->issuer_dname,
			cert_info->fingerprint);

	fprintf(stderr, ("(R)eject or accept (t)emporarily? "));
	SVN_ERR(wsvn_read_line(" ", choice, sizeof(choice), 0));

	if (choice[0] == 't' || choice[0] == 'T') {
		*cred = apr_pcalloc(pool, sizeof (**cred));
		(*cred)->may_save = FALSE;
		(*cred)->accepted_failures = failures;
	}
	else {
		*cred = NULL;
	}

	return SVN_NO_ERROR;
}


/* Callback for wsvn_repo_info() */
static svn_error_t *wsvn_repo_info_handler(void *baton, const char *path, const svn_info_t *info, apr_pool_t *pool)
{
	/* TODO: The uuid may also be read here */
	info_t *i= (info_t *)baton;
	*i->root = apr_pstrdup(pool, info->repos_root_URL);
	*i->uuid = apr_pstrdup(pool, info->repos_UUID);
	if (i->rev) {
		*i->rev = info->rev;
	}
	return SVN_NO_ERROR;
}


/* Callback for wsvn_log() */
static svn_error_t *wsvn_log_handler(void *baton, apr_hash_t *changed_paths, svn_revnum_t revision, const char *author, const char *date, const char *message, apr_pool_t *ool)
{
	logentry_t *entry = (logentry_t *)baton; 

	entry->revision = revision;
	if (author) {
		entry->author.value = strdup(author);
	}
	if (date) {
		entry->date.value = strdup(date);
	}
	if (message) {
		entry->msg.value = strdup(message);
	}

	return SVN_NO_ERROR;
}


/* Checks if path s1 is prefix of path s2 */
static char wsvn_check_prefix(const char *s1, const char *s2)
{
	if (!strncmp(s2, s1, strlen(s1))) {
		/* Assume s1 is a parent folder of s2 if it consist of more
		   characters */
		if ((strlen(s2) <= strlen(s1)) || (*(s2 + strlen(s1)) == '/')) {
			return 0;
		}
	}
	return 1;
}


/* Callback for wsvn_get_changeset */
static svn_error_t *wsvn_get_changeset_handler(void *baton, apr_hash_t *changed_paths, svn_revnum_t revision, const char *author, const char *date, const char *message, apr_pool_t *pool)
{
	list_t *list = (list_t *)baton;
	apr_hash_index_t *idx;

	if (changed_paths == NULL) {
		return SVN_NO_ERROR;
	}

	for (idx = apr_hash_first(pool, changed_paths); idx; idx = apr_hash_next(idx)) {
		const char *entryname;
		svn_log_changed_path_t *val;
		node_t node = node_create();

		apr_hash_this(idx, (void *) &entryname, NULL, (void *)&val);

/*		fprintf(stderr, "%c %s %s cfp %s\n", val->action, entryname, dopts->repo_prefix, val->copyfrom_path); */

		if (wsvn_check_prefix(dopts->repo_prefix, entryname)) {
			continue;
		}

		if (strlen(entryname) <= strlen(dopts->repo_prefix)) {
/*			if (dopts->prefix_is_file) { */
				node.path = strdup(".");
/*			} else { */
/*				continue; */
/*			} */
		} else {
			node.path = strdup(entryname+strlen(dopts->repo_prefix)+1);
		}

		switch (val->action) {
			case 'A': node.action = NA_ADD; break;
			case 'D': node.action = NA_DELETE; break;
			case 'R': node.action = NA_REPLACE; break;
			case 'M': node.action = NA_CHANGE; break;
		}

#if 0
		/* This is not really clean, but it is easy to resolve */
		/* copy problems here. At least for files! */
		if (val->copyfrom_path && !strncmp(val->copyfrom_path, dopts->repo_prefix, strlen(dopts->repo_prefix))) {
			node.copy_from_path = strdup(val->copyfrom_path+strlen(dopts->repo_prefix)+1);
			node.copy_from_rev = val->copyfrom_rev;
		} else if (val->copyfrom_path && node.action == NA_REPLACE) {
			/* This occurs if directories are copied */
			node.action = NA_ADD;
		} 
#endif

		if (val->copyfrom_path) {
			node.copy_from_path = strdup(val->copyfrom_path);
			node.copy_from_rev = val->copyfrom_rev;
		}

		list_append(list, &node);
	}

	return SVN_NO_ERROR;
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/

/* Initializes the Subversion API */
char wsvn_init(dump_options_t *opts)
{
	svn_error_t *err;
	svn_auth_provider_object_t *provider;
	apr_array_header_t *providers;

	dopts = opts;
	wc_setup = 0;

	mainpool = svn_pool_create(NULL);
	if (mainpool == NULL) {
		return 1;
	}

	err = svn_fs_initialize(mainpool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		return 1;
	}

	err = svn_config_ensure(NULL, mainpool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		return 1;
	}

	/* Init the context object */
	if ((err = svn_client_create_context(&ctx, mainpool))) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		return 1;
	}
	if ((err = svn_config_get_config(&(ctx->config), NULL, mainpool))) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		return 1;
	}

	/* Init authentication functions */
	providers = apr_array_make(mainpool, 4, sizeof (svn_auth_provider_object_t *));

	svn_auth_get_username_provider(&provider, mainpool);
	APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

	svn_auth_get_simple_prompt_provider(&provider, wsvn_auth_arguments, NULL, 2, mainpool);
	APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

	svn_auth_get_simple_prompt_provider(&provider, wsvn_auth_prompt, NULL, 2, mainpool);
	APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

	svn_auth_get_ssl_server_trust_prompt_provider(&provider, wsvn_auth_ssl_trust, NULL, mainpool);
	APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

	svn_auth_open(&ctx->auth_baton, providers, mainpool);

	/* Init ra session */
	err = svn_ra_initialize(mainpool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		return 1;
	}

	err = svn_client_open_ra_session(&session, dopts->repo_eurl, ctx, mainpool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		return 1;
	}

	return 0;
}


/* Frees all recources of the Subversion API */
void wsvn_free()
{
	svn_pool_clear(mainpool);
	svn_pool_destroy(mainpool);
}


/* Encodes an uri */
char *wsvn_uri_encode(const char *path)
{
	apr_pool_t *pool = svn_pool_create(NULL);
	const char *e = svn_path_uri_encode(svn_path_canonicalize(path, pool), pool);
	char *ed = strdup(e);
	svn_pool_clear(pool);
	svn_pool_destroy(pool);
	return ed;
}


/* Retrieves a brief changelog of the revision after the given one
   If current is NULL, the first log entry (since dopts->startrev)
   will be fetched */
char wsvn_next_log(logentry_t *current, logentry_t *next)
{
	apr_pool_t *pool;
	apr_array_header_t *paths;
	svn_error_t *err;

	pool = svn_pool_create(mainpool);
	paths = apr_array_make (pool, 1, sizeof (const char *));
	APR_ARRAY_PUSH(paths, const char *) = svn_path_canonicalize(".", pool);

	err = svn_ra_get_log(session, paths, (current ? current->revision+1 : dopts->startrev), dopts->endrev, 1, FALSE, TRUE, wsvn_log_handler, next, pool);
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "wsvn_next_log(%ld,%ld)\n\n", current->revision, next->revision);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_clear(pool);
		svn_pool_destroy(pool);
		return 1;
	}

	svn_pool_clear(pool);
	svn_pool_destroy(pool);
	return 0;
}


/* Stats a repository node */
char wsvn_stat(node_t *node, svn_revnum_t rev)
{
	apr_pool_t *pool = svn_pool_create(mainpool);
	svn_dirent_t *dirent;

	svn_error_t *err = svn_ra_stat(session, svn_path_canonicalize(node->path, pool), rev, &dirent, pool);
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "wsvn_stat(%p,%ld)\n\n", (void *)node, rev);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_clear(pool);
		svn_pool_destroy(pool);
		return 1;
	}

	if (dirent != NULL) {
		node->kind = (dirent->kind == svn_node_file ? NK_FILE : NK_DIRECTORY);
		node->size = dirent->size;
		node->has_props = dirent->has_props;
	} else {
		fprintf(stderr, "stat %s@%ld: file not existent in that revision.\n", node->path, rev);
		svn_pool_clear(pool);
		svn_pool_destroy(pool);
		return 1;
	}

	svn_pool_clear(pool);
	svn_pool_destroy(pool);
	return 0;
}


/* Writes the contents of a repository node to a given stream */
char wsvn_cat(node_t *node, svn_revnum_t rev, FILE *output)
{
	apr_pool_t *pool = svn_pool_create(mainpool);
	svn_stringbuf_t *buf = svn_stringbuf_create("", pool);
	svn_stream_t *stream = svn_stream_from_stringbuf(buf, pool);

	svn_error_t *err = svn_ra_get_file(session, svn_path_canonicalize(node->path, pool), rev, stream, NULL, NULL, pool);
	if (err) {
#if DEBUG
		fprintf(stderr, "wsvn_cat(%p,%ld)\n\n", (void *)node, rev);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_clear(pool);
		svn_pool_destroy(pool);
		return 1;
	}

	fwrite(buf->data, 1, buf->len, output);

	svn_stream_close(stream);
	svn_pool_clear(pool);
	svn_pool_destroy(pool);
	return 0;
}


/* Fetches all properties of a node */
char wsvn_get_props(node_t *node, list_t *list, svn_revnum_t rev)
{
	apr_pool_t *pool;
	apr_hash_t *hash;
	apr_hash_index_t *idx;
	svn_error_t *err;

	if (list == NULL || list->elsize != sizeof(property_t)) {
		return 1;
	}
	pool = svn_pool_create(mainpool);

	if (node->kind == NK_FILE) {
		err = svn_ra_get_file(session, node->path, rev, NULL, NULL, &hash, pool);
	} else {
		/*err = svn_ra_get_dir(session, (!strcmp(node->path, ".") ? "" : node->path), rev, NULL, NULL, &hash, pool);*/
		err = svn_ra_get_dir2(session, NULL, NULL, &hash, (!strcmp(node->path, ".") ? "": node->path), rev, 0, pool);
	}
	if (err) {
#if DEBUG
		fprintf(stderr, "wsvn_get_props(%p,%p,%ld)\n\n", (void *)node, (void *)list, rev);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_clear(pool);
		svn_pool_destroy(pool);
		return 1;
	}

	for (idx = apr_hash_first(pool, hash); idx; idx = apr_hash_next(idx)) {
		const char *entryname;
		svn_string_t *val;
		property_t prop = property_create();

		apr_hash_this(idx, (void *) &entryname, NULL, (void *)&val);

		if (svn_property_kind(NULL, entryname) != svn_prop_regular_kind) {
			continue;
		}

		prop.key = strdup(entryname);
		prop.value = strdup(val->data);
		list_append(list, &prop);
	}

	svn_pool_clear(pool);
	svn_pool_destroy(pool);
	return 0;
}


/* Fetches all properties of a working copy node */
/* TODO: This could be faster with another global function
   to open and close the wc_adm anchor */
char wsvn_get_wc_props(node_t *node, list_t *list)
{
	svn_wc_adm_access_t *adm_access, *dir_access;
	const char *wc_anchor, *wc_target;
	apr_pool_t *pool;
	apr_hash_t *hash;
	apr_hash_index_t *idx;
	svn_error_t *err;
	char *path;

	if (list == NULL || list->elsize != sizeof(property_t)) {
		return 1;
	}
	pool = svn_pool_create(mainpool);
	path = malloc(strlen(dopts->repo_dir)+strlen(node->path)+2);
	sprintf(path, "%s/%s", dopts->repo_dir, node->path);

	/* Get anchor (without locks) */
	err = svn_wc_adm_open_anchor(&adm_access, &dir_access, &wc_target, dopts->repo_dir, FALSE, -1, ctx->cancel_func, ctx->cancel_baton, pool);
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "wsvn_get_wc_props(%p,%p)\n\n", (void *)node, (void *)list);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_clear(pool);
		svn_pool_destroy(pool);
		free(path);
		return 1;
	}
	wc_anchor = svn_wc_adm_access_path(adm_access);

	err = svn_wc_prop_list(&hash, path, adm_access, pool);
	free(path);
	if (err) {
#if DEBUG
		fprintf(stderr, "wsvn_get_wc_props(%p,%p)\n\n", (void *)node, (void *)list);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_clear(pool);
		svn_pool_destroy(pool);
		svn_wc_adm_close(adm_access);
		return 1;
	}

	for (idx = apr_hash_first(pool, hash); idx; idx = apr_hash_next(idx)) {
		const char *entryname;
		svn_string_t *val;
		property_t prop = property_create();

		apr_hash_this(idx, (void *) &entryname, NULL, (void *)&val);

		if (svn_property_kind(NULL, entryname) != svn_prop_regular_kind) {
			continue;
		}

		prop.key = strdup(entryname);
		prop.value = strdup(val->data);
		list_append(list, &prop);
	}

	svn_pool_clear(pool);
	svn_pool_destroy(pool);
	svn_wc_adm_close(adm_access);
	return 0;
}


/* Recursively lists the contents of a directory */
char wsvn_find(node_t *node, list_t *list, svn_revnum_t rev)
{
	apr_pool_t *pool;
	apr_hash_t *hash;
	apr_hash_index_t *idx;
	svn_error_t *err;

	if (list == NULL || list->elsize != sizeof(node_t)) {
		return 1;
	}

	pool = svn_pool_create(mainpool);

	/*err = svn_ra_get_dir(session, (!strcmp(node->path, ".") ? "" : node->path), rev, &hash, NULL, NULL, pool);*/
	err = svn_ra_get_dir2(session, &hash, NULL, NULL, (!strcmp(node->path, ".") ? "" : node->path), rev, SVN_DIRENT_KIND | SVN_DIRENT_SIZE | SVN_DIRENT_HAS_PROPS, pool);
	if (err) {
#if DEBUG
		fprintf(stderr, "wsvn_find(%p,%p,%ld)\n\n", (void *)node, (void *)list, rev);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_clear(pool);
		svn_pool_destroy(pool);
		return 1;
	}

	for (idx = apr_hash_first(pool, hash); idx; idx = apr_hash_next(idx)) {
		const char *entryname;
		svn_dirent_t *val;
		node_t enode = node_create();

		apr_hash_this(idx, (void *) &entryname, NULL, (void *)&val);

		if (strcmp(node->path, ".")) {
			enode.path = malloc(strlen(entryname)+strlen(node->path)+2);
			sprintf(enode.path, "%s/%s", node->path, entryname);
		} else {
			enode.path = strdup(entryname);
		}
		enode.kind = (val->kind == svn_node_file ? NK_FILE : NK_DIRECTORY);
		enode.size = val->size;
		enode.has_props = val->has_props;
		
		list_append(list, &enode);
	}

	svn_pool_clear(pool);
	svn_pool_destroy(pool);
	return 0;
}


/* Returns a changeset as a dynamic list of node_ts */
char wsvn_get_changeset(logentry_t *entry, list_t *list)
{
	apr_pool_t *pool;
	apr_array_header_t *paths;
	svn_error_t *err;

	if (list == NULL || list->elsize != sizeof(node_t)) {
		return 1;
	}

	pool = svn_pool_create(mainpool);
	paths = apr_array_make (pool, 1, sizeof (const char *));
	APR_ARRAY_PUSH(paths, const char *) = svn_path_canonicalize(".", pool);

	err = svn_ra_get_log(session, paths, entry->revision, entry->revision, 1, TRUE, TRUE, wsvn_get_changeset_handler, list, pool);
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "wsvn_get_changeset(%p)\n\n", (void *)entry);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_clear(pool);
		svn_pool_destroy(pool);
		return 1;
	}

	svn_pool_clear(pool);
	svn_pool_destroy(pool);
	return 0;
}


/* Fetches additional information about a repository */
char wsvn_repo_info(char *path, char **url, char **prefix, char **uuid, svn_revnum_t *headrev)
{
	apr_pool_t *pool = svn_pool_create(mainpool);
	svn_opt_revision_t revision;
	svn_error_t *err;
	char *root, *id;
	const char *mpref;
	info_t info;

	info.root = &root;
	info.uuid = &id;
	info.rev = headrev;
	revision.kind = svn_opt_revision_head;

	/* TODO: Subversion 1.5 offers svn_client_root_url_from_path() */
	err = svn_client_info(dopts->repo_eurl, &revision, &revision, wsvn_repo_info_handler, &info, FALSE, ctx, pool);
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "wsvn_repo_info(%s,%p,%p)\n\n", path, (void *)url, (void *)prefix);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_clear(pool);
		svn_pool_destroy(pool);
		return 1;
	}

	*url = strdup(root);
	*uuid = strdup(id);
	mpref = path+strlen(root);
	if (strlen(root) < strlen(path) && mpref && strlen(mpref)) {
		*prefix = strdup(mpref);
	} else {
		*prefix = strdup("\0");
	}

	svn_pool_clear(pool);
	svn_pool_destroy(pool);
	return 0;
}


/* Updates the repository to the specified revision */
char wsvn_update(svn_revnum_t rev)
{
	svn_wc_adm_access_t *adm_access, *dir_access;
	const char *wc_anchor, *wc_target;
	const svn_delta_editor_t *editor;
	const svn_ra_reporter2_t *reporter;
	void *editor_baton, *reporter_baton;
	apr_pool_t *pool = svn_pool_create(mainpool);
	svn_wc_traversal_info_t *traversal_info = svn_wc_init_traversal_info(pool);
	svn_error_t *err;

	/* Setup working copy if needed */
	if (wc_setup == 0) {
		err = svn_wc_ensure_adm2(dopts->repo_dir, dopts->repo_uuid, dopts->repo_eurl, dopts->repo_base, rev, pool);
		if (err) {
#ifdef DEBUG
			fprintf(stderr, "wsvn_update(%ld): -1\n\n", rev);
#endif
			svn_handle_error2(err, stderr, FALSE, APPNAME": ");
			svn_error_clear(err);
			svn_pool_clear(pool);
			svn_pool_destroy(pool);
			return 1;
		}
		wc_setup = 1;
	}

	/* Get anchor and locks */
	err = svn_wc_adm_open_anchor(&adm_access, &dir_access, &wc_target, dopts->repo_dir, TRUE, -1, ctx->cancel_func, ctx->cancel_baton, pool);
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "wsvn_update(%ld): 0\n\n", rev);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_clear(pool);
		svn_pool_destroy(pool);
		return 1;
	}
	wc_anchor = svn_wc_adm_access_path(adm_access);

	/* We don't need conflict resolving stuff, because (hopefully) we are */
	/* the only one accessing the working copy. And we're just updating. */
	
	/* Fetch the update editor */
	err = svn_wc_get_update_editor2(&rev, adm_access, wc_target, FALSE,TRUE, NULL, NULL, ctx->cancel_func, ctx->cancel_baton, NULL, &editor, &editor_baton, NULL, pool);
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "wsvn_update(%ld): 1\n\n", rev);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_clear(pool);
		svn_pool_destroy(pool);
		return 1;
	}

	/* Perform the update */
	err = svn_ra_do_update(session, &reporter, &reporter_baton, rev, wc_target, TRUE, editor, editor_baton, pool); 
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "wsvn_update(%ld): 2\n\n", rev);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_clear(pool);
		svn_pool_destroy(pool);
		return 1;
	}

	err = svn_wc_crawl_revisions2(dopts->repo_dir, dir_access, reporter, reporter_baton, TRUE, TRUE, TRUE, NULL, NULL, traversal_info, pool);
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "wsvn_update(%ld): 3\n\n", rev);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_clear(pool);
		svn_pool_destroy(pool);
		return 1;
	}

	err = svn_wc_adm_close(adm_access);
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "wsvn_update(%ld): 4\n\n", rev);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		svn_error_clear(err);
		svn_pool_clear(pool);
		svn_pool_destroy(pool);
		return 1;
	}

	svn_pool_clear(pool);
	svn_pool_destroy(pool);
	svn_wc_adm_close(adm_access);
	return 0;
}
