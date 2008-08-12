/*
 *	rsvndump - remote svn repository dump
 *	Copyright (C) 2008 Jonas Gehring
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along
 *	with this program; if not, write to the Free Software Foundation, Inc.,
 *	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include "main.h"
#include "svn_functions.h"
#include "list.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <svn_client.h>
#include <svn_pools.h>
#include <svn_auth.h>
#include <svn_cmdline.h>
#include <svn_fs.h>
#include <svn_path.h>
#include <svn_sorts.h>


// Globals
static apr_pool_t *pool = NULL;
static apr_pool_t *filepool = NULL;
static apr_pool_t *revpool = NULL;
static svn_client_ctx_t *ctx = NULL;
static char initialized = 0;


// Subversion callbacks
static svn_error_t *prompt_and_read_line(const char *prompt, char *buffer, size_t max, char pass)
{
	if (!pass) {
		int len;
		fprintf(stderr, "%s", prompt);
		if (fgets(buffer, max, stdin) == NULL) {
			return svn_error_create(0, NULL, "error reading stdin");
		}
		len = strlen(buffer);
		if (len > 0 && buffer[len-1] == '\n') {
			buffer[len-1] = '\0';
		}
	} else {
		strcpy(buffer, getpass(prompt));	
	}

	return SVN_NO_ERROR;
}

// Try to authenticate with given arguments
static svn_error_t *auth_arguments(svn_auth_cred_simple_t **cred, void *baton, const char *realm, const char *username, svn_boolean_t may_save, apr_pool_t *poola)
{
	if (revpool != NULL) {
		poola = revpool;
	}
	svn_auth_cred_simple_t *ret = apr_pcalloc(poola, sizeof(*ret));

	if (repo_username) {
		ret->username = apr_pstrdup(poola, repo_username);
	} else {
		ret->username = apr_pstrdup(poola, "\0");
	}
	if (repo_password) {
		ret->password = apr_pstrdup(poola, repo_password);
	} else {
		ret->password = apr_pstrdup(poola, "\0");
	}

	*cred = ret;
	return SVN_NO_ERROR;
}

// Try to authenticate via a prompt 
static svn_error_t *auth_prompt(svn_auth_cred_simple_t **cred, void *baton, const char *realm, const char *username, svn_boolean_t may_save, apr_pool_t *poola)
{
	if (revpool != NULL) {
		poola = revpool;
	}
	svn_auth_cred_simple_t *ret = apr_pcalloc(poola, sizeof(*ret));
	char answerbuf[100];

	if (username) {
		ret->username = apr_pstrdup(poola, username);
	} else {
		SVN_ERR(prompt_and_read_line("Username: ", answerbuf, sizeof(answerbuf), 0));
		ret->username = apr_pstrdup(poola, answerbuf);
	}

	SVN_ERR(prompt_and_read_line("Password: ", answerbuf, sizeof(answerbuf), 1));
	ret->password = apr_pstrdup(poola, answerbuf);

	*cred = ret;
	return SVN_NO_ERROR;
}

// SSL certificate authentication
static svn_error_t *auth_ssl_trust(svn_auth_cred_ssl_server_trust_t **cred, void *baton, const char *realm, apr_uint32_t failures, const svn_auth_ssl_server_cert_info_t *cert_info, svn_boolean_t may_save, apr_pool_t *poola)
{
	if (revpool != NULL) {
		poola = revpool;
	}
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
	char choice[3];
	SVN_ERR(prompt_and_read_line(" ", choice, sizeof(choice), 0));

	if (choice[0] == 't' || choice[0] == 'T') {
		*cred = apr_pcalloc(poola, sizeof (**cred));
		(*cred)->may_save = FALSE;
		(*cred)->accepted_failures = failures;
	}
	else {
		*cred = NULL;
	}

	return SVN_NO_ERROR;
}


// Initializes the client
char svn_init()
{
	// We only initilize the client once
	if (initialized) {
		return 0;
	}

	// Let's go
	if (svn_cmdline_init(APPNAME, stderr) != EXIT_SUCCESS) {
		return 1;		
	}

	pool = svn_pool_create(NULL);
	svn_error_t *err = svn_fs_initialize(pool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		return 1;
	}

	err = svn_config_ensure(NULL, pool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		return 1;
	}

	// Init the context object
	if ((err = svn_client_create_context(&ctx, pool))) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		return 1;
	}
	if ((err = svn_config_get_config(&(ctx->config), NULL, pool))) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		return 1;
	}
	if ((err = svn_config_get_config(&(ctx->config), NULL, pool))) {
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		return 1;
	}

	// Init authentication functions
	svn_auth_provider_object_t *provider;
	apr_array_header_t *providers
		= apr_array_make (pool, 4, sizeof (svn_auth_provider_object_t *));

	svn_auth_get_simple_prompt_provider(&provider, auth_arguments, NULL, 2, pool);
	APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

	svn_auth_get_simple_prompt_provider(&provider, auth_prompt, NULL, 2, pool);
	APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

	svn_auth_get_ssl_server_trust_prompt_provider(&provider, auth_ssl_trust, NULL, pool);
	APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

	svn_auth_open(&ctx->auth_baton, providers, pool);      

	initialized = 1;
	return 0;
}


// Free svn pool
void svn_free()
{
	svn_pool_destroy(pool);
}


// Allocate a pool used by svn_log, svn_list_* and svn_get_kind
char svn_alloc_rev_pool()
{
	revpool = svn_pool_create(NULL);
	return (revpool == NULL);
}

void svn_free_rev_pool()
{
	if (revpool) {
		svn_pool_destroy(revpool);
		revpool = NULL;
	}
}


// Output contents of a given file using to a file descriptor
svn_stream_t *svn_open(char *path, int rev, char **buffer, int *len)
{
	if (filepool == NULL) {
		filepool = svn_pool_create(NULL);
		if (filepool == NULL) {
			fprintf(stderr, "out of memory!\n");
			exit(1);
		}
	}

	svn_opt_revision_t revision;
	svn_stringbuf_t *buf = svn_stringbuf_create("", filepool);
	svn_stream_t *stream = svn_stream_from_stringbuf(buf, filepool);

	if (rev == HEAD_REVISION) {
		revision.kind = svn_opt_revision_head;
	} else {
		revision.kind = svn_opt_revision_number;
		revision.value.number = rev;
	}

	svn_error_t *err = svn_client_cat2(stream, svn_path_uri_encode(path, filepool), &revision, &revision, ctx, filepool);
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "error: svn_open(%s,%d,%p,%p)\n\n", path, rev, buffer, len);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		return NULL;
	}
	
	*len = buf->len;
	*buffer = buf->data;

	return stream;
}

void svn_close(svn_stream_t *stream)
{
	svn_stream_close(stream);

	if (filepool != NULL) {
		svn_pool_destroy(filepool);
		filepool = NULL;
	}
}


// Output log for a specified path
static char *mauthor, *mlogmsg, *mdate;

static svn_error_t *svn_log_rec_info(void *baton, apr_hash_t *changed_paths, svn_revnum_t revision, const char *author, const char *date, const char *message, apr_pool_t *pool)
{
	if (author) {
		mauthor = strdup(author);
	}
	else {
		mauthor = strdup("\0");
	}
	if (message) {
		mlogmsg = strdup(message);
	}
	else {
		mlogmsg = strdup("\0");
	}
	if (date) {
		mdate = strdup(date);
	}
	else {
		mdate = strdup("\0");
	}
	return SVN_NO_ERROR;
}
char svn_log(const char *path, int rev, char **author, char **logmsg, char **date)
{
	svn_opt_revision_t start, end;
	if (rev == HEAD_REVISION) {
		start.kind = svn_opt_revision_head;
		end.kind = svn_opt_revision_head;
	} else {
		start.kind = svn_opt_revision_number;
		start.value.number = rev;
		end.kind = svn_opt_revision_number;
		end.value.number = rev;
	}

	apr_array_header_t *paths
		= apr_array_make (revpool, 1, sizeof (const char *));
	APR_ARRAY_PUSH(paths, const char *) = svn_path_uri_encode(path, revpool);

	svn_error_t *err = svn_client_log(paths, &start, &end, FALSE, FALSE, svn_log_rec_info, NULL, ctx, revpool);
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "error: svn_log(%s,%d,%p,%p)\n\n", path, rev, logmsg, date);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		return 1;
	}

	*author = mauthor;
	*logmsg = mlogmsg;
	*date = mdate;

	return 0;
}


// List changes of path for previous revision to rev 
static list_t *mlist;
static svn_error_t *svn_log_rec(void *baton, apr_hash_t *changed_paths, svn_revnum_t revision, const char *author, const char *date, const char *message, apr_pool_t *pool)
{
	change_entry_t entry;
	apr_hash_index_t *idx;
	if (changed_paths == NULL) {
		return SVN_NO_ERROR;
	}
	for (idx = apr_hash_first(revpool, changed_paths); idx; idx = apr_hash_next(idx)) {
		const char *entryname;
		svn_log_changed_path_t *val;

		apr_hash_this(idx, (void *) &entryname, NULL, (void *)&val);

		entry.path = strdup(entryname);
		if (val->copyfrom_path) {
			entry.copy_from_path = strdup(val->copyfrom_path);
			entry.copy_from_rev = val->copyfrom_rev;
		} else {
			entry.copy_from_path = NULL;
		}
		switch (val->action) {
			case 'A': entry.action = NK_ADD; break;
			case 'D': entry.action = NK_DELETE; break;
			case 'R': entry.action = NK_REPLACE; break;
			case 'M': entry.action = NK_CHANGE; break;
		}

		list_add(mlist, &entry);
	}

	return SVN_NO_ERROR;
}

list_t svn_list_changes(const char *path, int rev)
{
	svn_opt_revision_t start, end;
	if (rev == HEAD_REVISION) {
		start.kind = svn_opt_revision_head;
		end.kind = svn_opt_revision_head;
	} else {
		start.kind = svn_opt_revision_number;
		start.value.number = rev;
		end.kind = svn_opt_revision_number;
		end.value.number = rev;
	}

	list_t list;
	list_init(&list, sizeof(change_entry_t));
	mlist = &list;

	apr_array_header_t *paths
		= apr_array_make (revpool, 1, sizeof (const char *));
	APR_ARRAY_PUSH(paths, const char *) = svn_path_uri_encode(path, revpool);

	svn_error_t *err = svn_client_log(paths, &start, &end, TRUE, TRUE, svn_log_rec, NULL, ctx, revpool);
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "error: svn_list_changes(%s,%d)\n\n\n", path, rev);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
	}

	return list;
}


// Lists all properties
list_t svn_list_props(const char *path, int rev)
{
	svn_opt_revision_t revision;
	if (rev == HEAD_REVISION) {
		revision.kind = svn_opt_revision_head;
	} else {
		revision.kind = svn_opt_revision_number;
		revision.value.number = rev;
	}

	list_t list;
	list_init(&list, sizeof(prop_t));
	mlist = &list;

	apr_array_header_t *props;
	svn_error_t *err = svn_client_proplist(&props, svn_path_uri_encode(path, revpool), &revision, FALSE, ctx, revpool);
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "error: svn_list_props(%s,%d)\n\n", path, rev);
#endif
//		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		return list;
	}

	int i;
	for (i = 0; i < props->nelts; i++) {
		svn_client_proplist_item_t *item = APR_ARRAY_IDX(props, i, svn_client_proplist_item_t *);
		apr_hash_index_t *idx;
		for (idx = apr_hash_first(revpool, item->prop_hash); idx; idx = apr_hash_next(idx)) {
			const char *entryname;
			svn_string_t *val;

			apr_hash_this(idx, (void *) &entryname, NULL, (void *)&val);

			prop_t pt;
			pt.key = strdup(entryname);
			pt.value = strdup(val->data);
			list_add(&list, &pt);
		}
	}

	return list;
}


// Lists a repository using a given function for "printing"
nodekind_t svn_get_kind(const char *path, int rev)
{
	svn_opt_revision_t revision;
	apr_hash_t *dirents;

	if (rev == HEAD_REVISION) {
		revision.kind = svn_opt_revision_head;
	} else {
		revision.kind = svn_opt_revision_number;
		revision.value.number = rev;
	}

	svn_error_t *err = svn_client_ls(&dirents, svn_path_uri_encode(path, revpool), &revision, FALSE, ctx, revpool);
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "error: svn_get_kind(%s,%d)\n\n", path, rev);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		return -1;
	}

	nodekind_t kind = NK_DIRECTORY;
	apr_hash_index_t *idx;
	char *name = strrchr(path, '/')+1;
	for (idx = apr_hash_first(revpool, dirents); idx; idx = apr_hash_next(idx)) {
		const char *entryname;
		svn_dirent_t *val;

		apr_hash_this(idx, (void *) &entryname, NULL, (void *)&val);

		if (!strcmp(name, entryname)) {
			switch (val->kind) {
				case svn_node_file:
					kind = NK_FILE; break;
				case svn_node_dir:
					kind = NK_DIRECTORY; break;
				default: break;
			}
			break;
		}
	}
	return kind;
}


// Gets repository information
char *murl;
svn_error_t *svn_info_rec(void *baton, const char *path, const svn_info_t *info, apr_pool_t *poola)
{
	// TODO: The uuid may also be read here
	murl = strdup(info->repos_root_URL);
	return SVN_NO_ERROR;
}

char svn_repo_info(const char *path, char **url, char **prefix)
{
	svn_opt_revision_t revision;
	revision.kind = svn_opt_revision_head;

	svn_error_t *err = svn_client_info(svn_path_uri_encode(path, pool), &revision, &revision, svn_info_rec, NULL, FALSE, ctx, pool);
	if (err) {
#ifdef DEBUG
		fprintf(stderr, "error: svn_repo_info(%s,%p,%p)\n\n", path, url, prefix);
#endif
		svn_handle_error2(err, stderr, FALSE, APPNAME": ");
		return 1;
	}

	*url = strdup(murl);
	const char *mpref = path+strlen(murl);
	if (mpref && strlen(mpref)) {
		*prefix = strdup(mpref);
	} else {
		*prefix = strdup("\0");
	}
	free(murl);

	return 0;
}

