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
 *      file: dump_delta.h
 *      desc: Support for dumping of deltas 
 */


#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <svn_delta.h>
#include <svn_pools.h>
#include <svn_props.h>
#include <svn_repos.h>

#include "main.h"
#include "dump_delta.h"
#include "list.h"
#include "node.h"
#include "property.h"
#include "wsvn.h"


/*---------------------------------------------------------------------------*/
/* Static variables                                                          */
/*---------------------------------------------------------------------------*/

static logentry_t prev;
static char prev_created;

typedef struct {
	dump_options_t	*opts;
} baton_t;

typedef struct {
	baton_t		*baton;
	char		*path;
	char		*filename;
	list_t		props;
	nodeaction_t	action;
	nodekind_t	kind;
	char 		*copy_from_path;
	svn_revnum_t	copy_from_rev;
	char 		use_copy;
	char		dumped;
} node_baton_t;


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/

/* Creates a new node baton */
static node_baton_t *dump_delta_node_create(baton_t *baton, apr_pool_t *pool)
{
	node_baton_t *node = apr_palloc(pool, sizeof(node_baton_t));
	node->baton = baton;
	node->props = list_create(sizeof(property_t));
	node->dumped = 0;
	return node;
}

/* Frees the memory of the node baton that was not alloced using a pool */
static void dump_delta_node_free(node_baton_t *node)
{
	int i;
	for (i = 0; i < node->props.size; i++) {
		property_free((property_t *)node->props.elements + i);
	}
	list_free(&node->props);
}


/* Dumps a node */
static char dump_delta_node(node_baton_t *node)
{
	int i;
	unsigned long prop_len, content_len;
	dump_options_t *opts = ((baton_t *)node->baton)->opts;
	char *path = node->path;

	/* If the node is a directory and no properties have been changed,
	   we don't need to dump it */
	if (node->action != NA_ADD && node->kind == NK_DIRECTORY && node->props.size == 0) {
		node->dumped = 1;
		return 0;
	}

	if (opts->prefix_is_file) {
		path = strrchr(opts->repo_eurl, '/')+1;
	}
	if (opts->user_prefix != NULL) {
		fprintf(opts->output, "%s: %s%s\n", SVN_REPOS_DUMPFILE_NODE_PATH, opts->user_prefix, path);
	} else {
		fprintf(opts->output, "%s: %s\n", SVN_REPOS_DUMPFILE_NODE_PATH, path);
	}

	if (node->action != NA_DELETE) {
		fprintf(opts->output, "%s: %s\n", SVN_REPOS_DUMPFILE_NODE_KIND, node->kind == NK_FILE ? "file" : "dir");
	}

	fprintf(opts->output, "%s: ", SVN_REPOS_DUMPFILE_NODE_ACTION ); 
	switch (node->action) {
		case NA_CHANGE:
			fprintf(opts->output, "change\n"); 
			break;
		case NA_ADD:
			fprintf(opts->output, "add\n"); 
			break;
		case NA_DELETE:
			fprintf(opts->output, "delete\n"); 
			break;
		case NA_REPLACE:
			fprintf(opts->output, "replace\n"); 
			break;
	}

	/* Write properties & content if neccessary */
	if (node->action == NA_DELETE) {
		fprintf(opts->output, "\n\n");
		node->dumped = 1;
		return 0;
	}

	prop_len = PROPS_END_LEN;
	content_len = 0;

	/* Write property size */
	for (i = 0; i < node->props.size; i++) {
		property_t *p = (property_t *)node->props.elements + i; 
		prop_len += property_strlen(p);
	}
	if (node->props.size != 0 || node->action == NA_ADD) {
		fprintf(opts->output, "%s: %lu\n", SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH, prop_len);
	} else {
		prop_len = 0;
	}

	/* Write content size */
	if (node->kind == NK_FILE) {
		struct stat st;
		if (stat(node->filename, &st)) {
			DEBUG_MSG("dump_delta_node: FATAL: cannot stat %s\n", node->filename);
			return 1;
		}
		content_len = st.st_size;

		fprintf(opts->output, "%s: true\n", SVN_REPOS_DUMPFILE_TEXT_DELTA);
		fprintf(opts->output, "%s: %lu\n", SVN_REPOS_DUMPFILE_TEXT_CONTENT_LENGTH, content_len);
	}

	fprintf(opts->output, "%s: %lu\n", SVN_REPOS_DUMPFILE_CONTENT_LENGTH, (unsigned long)prop_len+content_len);
	fprintf(opts->output, "\n");

	/* Write properties */
	if (node->props.size != 0 || node->action == NA_ADD) {
		for (i = 0; i < node->props.size; i++) {
			property_t *p = (property_t *)node->props.elements + i; 
			property_dump(p, opts->output);
			free((char *)p->key);
		}
		fprintf(opts->output, PROPS_END);
	}
	
	/* Write content */
	if (node->kind != NK_DIRECTORY) {
		FILE *f;
		char *buffer = malloc(2049);
		size_t s;

		f = fopen(node->filename, "rb");
		if (f == NULL) {
			fprintf(stderr, _("ERROR: Failed to open %s.\n"), node->filename);
			free(buffer);
			return 1;
		}
		while ((s = fread(buffer, 1, 2048, f))) {
			fwrite(buffer, 1, s, opts->output);
		}
		free(buffer);
		
		/* Close and remove temporary file */
		fclose(f);
		unlink(node->filename);
	}

	fprintf(opts->output, "\n\n");
	node->dumped = 1;
	return 0;
}

/*
 * Subversion delta editor callbacks
 */

static svn_error_t *set_target_revision(void *edit_baton, svn_revnum_t target_revision, apr_pool_t *pool)
{
	/* The revision header has already been dumped, so there's nothing to
	   do here */
	return SVN_NO_ERROR;
}

static svn_error_t *open_root(void *edit_baton, svn_revnum_t base_revision, apr_pool_t *dir_pool, void **root_baton)
{
	node_baton_t *node = dump_delta_node_create(edit_baton, dir_pool);
	/* The revision header has already been dumped, so there's nothing to
	   do for the root node */
	node->dumped = 1;

	*root_baton = node;
	return SVN_NO_ERROR;
}

static svn_error_t *delete_entry(const char *path, svn_revnum_t revision, void *parent_baton, apr_pool_t *pool)
{
	node_baton_t *parent;
	dump_options_t *opts = ((node_baton_t *)parent_baton)->baton->opts;

	/* Check if the parent dump needs to be dumped */
	parent = (node_baton_t *)parent_baton;
	if (!parent->dumped) {
		dump_delta_node(parent);
	}

	/* A deletion can be dumped without additional notification */
	if (opts->user_prefix != NULL) {
		fprintf(opts->output, "%s: %s%s\n", SVN_REPOS_DUMPFILE_NODE_PATH, opts->user_prefix, path);
	} else {
		fprintf(opts->output, "%s: %s\n", SVN_REPOS_DUMPFILE_NODE_PATH, path);
	}

	fprintf(opts->output, "%s: delete\n\n\n", SVN_REPOS_DUMPFILE_NODE_ACTION); 

	return SVN_NO_ERROR;
}

static svn_error_t *add_directory(const char *path, void *parent_baton, const char *copyfrom_path, svn_revnum_t copyfrom_revision, apr_pool_t *dir_pool, void **child_baton)
{
	DEBUG_MSG("add_directory(%s)\n", path);
	node_baton_t *parent, *node;
	
	/* Check if the parent node needs to be dumped */
	parent = (node_baton_t *)parent_baton;
	if (!parent->dumped) {
		dump_delta_node(parent);
	}

	node = dump_delta_node_create(parent->baton, dir_pool);
	node->kind = NK_DIRECTORY;

	node->path = apr_pstrdup(dir_pool, path);
	node->copy_from_path = apr_pstrdup(dir_pool, copyfrom_path);
	node->copy_from_rev = copyfrom_revision;
	node->action = NA_ADD;

	*child_baton = node;
	return SVN_NO_ERROR;
}


static svn_error_t *open_directory(const char *path, void *parent_baton, svn_revnum_t base_revision, apr_pool_t *dir_pool, void **child_baton)
{
	DEBUG_MSG("open_directory(%s)\n", path);
	node_baton_t *parent, *node;

	/* Check if the parent node needs to be dumped */
	parent = (node_baton_t *)parent_baton;
	if (!parent->dumped) {
		dump_delta_node(parent);
	}

	node = dump_delta_node_create(parent->baton, dir_pool);
	node->kind = NK_DIRECTORY;

	node->path = apr_pstrdup(dir_pool, path);
	node->action = NA_CHANGE;

	*child_baton = node;
	return SVN_NO_ERROR;
}


static svn_error_t *change_dir_prop(void *dir_baton, const char *name, const svn_string_t *value, apr_pool_t *pool)
{
	DEBUG_MSG("change_dir_prop(%s)\n", name);

	if (svn_property_kind(NULL, name) != svn_prop_regular_kind) {
		return SVN_NO_ERROR;
	}

	property_t p = property_create();
	p.key = strdup(name);
	if (value != NULL) {
		p.value = strdup(value->data);
	} else {
		p.value = NULL;
	}

	list_append(&((node_baton_t *)dir_baton)->props, &p);

	return SVN_NO_ERROR;
}

static svn_error_t *close_directory(void *dir_baton, apr_pool_t *pool)
{
	DEBUG_MSG("close_direcotry()\n");
	node_baton_t *node;

	/* Check if the this node needs to be dumped */
	node = (node_baton_t *)dir_baton;
	if (!node->dumped) {
		dump_delta_node(node);
	}

	dump_delta_node_free(node);
	return SVN_NO_ERROR;
}

static svn_error_t *absent_directory(const char *path, void *parent_baton, apr_pool_t *pool)
{
	fprintf(stderr, "absent_directory(%s)\n", path);
	return SVN_NO_ERROR;
}

static svn_error_t *add_file(const char *path, void *parent_baton, const char *copyfrom_path, svn_revnum_t copyfrom_revision, apr_pool_t *file_pool, void **file_baton)
{
	DEBUG_MSG("add_directory(%s)\n", path);
	node_baton_t *parent, *node;

	/* Check if the parent node needs to be dumped */
	parent = (node_baton_t *)parent_baton;
	if (!parent->dumped) {
		dump_delta_node(parent);
	}

	node = dump_delta_node_create(parent->baton, file_pool);
	node->kind = NK_FILE;

	node->path = apr_pstrdup(file_pool, path);
	node->copy_from_path = apr_pstrdup(file_pool, copyfrom_path);
	node->copy_from_rev = copyfrom_revision;
	node->action = NA_ADD;

	*file_baton = node;
	return SVN_NO_ERROR;
}

static svn_error_t *open_file(const char *path, void *parent_baton, svn_revnum_t base_revision, apr_pool_t *file_pool, void **file_baton)
{
	DEBUG_MSG("open_file(%s)\n", path);

	node_baton_t *parent, *node;
	/* Check if the parent node needs to be dumped */
	parent = (node_baton_t *)parent_baton;
	if (!parent->dumped) {
		dump_delta_node(parent);
	}

	node = dump_delta_node_create(parent->baton, file_pool);
	node->kind = NK_FILE;

	node->path = apr_pstrdup(file_pool, path);
	node->action = NA_CHANGE;

	*file_baton = node;
	return SVN_NO_ERROR;
}

static svn_error_t *apply_textdelta(void *file_baton, const char *base_checksum, apr_pool_t *pool, svn_txdelta_window_handler_t *handler, void **handler_baton)
{
	DEBUG_MSG("apply_textdelta(%s)\n", base_checksum);
	apr_file_t *file;
	svn_stream_t *stream;
	node_baton_t *node = file_baton;

	/* Open a temporary file */
	node->filename = apr_palloc(pool, strlen(node->baton->opts->repo_dir)+8);
	sprintf(node->filename, "%s/XXXXXX", node->baton->opts->repo_dir);
	apr_file_mktemp(&file, node->filename, APR_CREATE | APR_READ | APR_WRITE | APR_EXCL, pool);
	stream = svn_stream_from_aprfile2(file, FALSE, pool);

	/* Write the textdelta to a temporary file */
	svn_txdelta_to_svndiff(stream, pool, handler, handler_baton);
	return SVN_NO_ERROR;
}

static svn_error_t *change_file_prop(void *file_baton, const char *name, const svn_string_t *value, apr_pool_t *pool)
{
	DEBUG_MSG("change_file_prop(%s)\n", name);

	if (svn_property_kind(NULL, name) != svn_prop_regular_kind) {
		return SVN_NO_ERROR;
	}

	property_t p = property_create();
	p.key = strdup(name);
	if (value != NULL) {
		p.value = strdup(value->data);
	} else {
		p.value = NULL;
	}

	list_append(&((node_baton_t *)file_baton)->props, &p);

	return SVN_NO_ERROR;
}

static svn_error_t *close_file(void *file_baton, const char *text_checksum, apr_pool_t *pool)
{
	DEBUG_MSG("close_file()\n");
	node_baton_t *node;

	/* Check if the this node needs to be dumped */
	node = (node_baton_t *)file_baton;
	if (!node->dumped) {
		dump_delta_node(node);
	}

	dump_delta_node_free(node);
	return SVN_NO_ERROR;
}

static svn_error_t *absent_file(const char *path, void *parent_baton, apr_pool_t *pool)
{
	DEBUG_MSG("absent_file(%s)\n", path);
	return SVN_NO_ERROR;
}

static svn_error_t *close_edit(void *edit_baton, apr_pool_t *pool)
{
	DEBUG_MSG("close_edit\n");
	return SVN_NO_ERROR;
}

static svn_error_t *abort_edit(void *edit_baton, apr_pool_t *pool)
{
	DEBUG_MSG("abort_edit\n");
	return SVN_NO_ERROR;
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/

/* Dumps the specified revision using the given dump options */
char dump_delta_revision(dump_options_t *opts, logentry_t *entry, svn_revnum_t local_revnum)
{
	char ret;
	int props_length;
	svn_delta_editor_t *editor;
	baton_t *editor_baton;
	apr_pool_t *pool = svn_pool_create(NULL);

	/* Write revision header */
	props_length = 0;
	props_length += property_strlen(&entry->author);
	props_length += property_strlen(&entry->date);
	props_length += property_strlen(&entry->msg);
	if (props_length > 0) {
		props_length += PROPS_END_LEN;
	}

	if (opts->keep_revnums) {
		fprintf(opts->output, "%s: %ld\n", SVN_REPOS_DUMPFILE_REVISION_NUMBER, entry->revision);	
	} else {
		fprintf(opts->output, "%s: %ld\n", SVN_REPOS_DUMPFILE_REVISION_NUMBER, local_revnum);	
	}
	fprintf(opts->output, "%s: %d\n", SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH, props_length);
	fprintf(opts->output, "%s: %d\n\n", SVN_REPOS_DUMPFILE_CONTENT_LENGTH, props_length);

	if (props_length > 0) {
		property_dump(&entry->msg, opts->output);
		property_dump(&entry->author, opts->output);
		property_dump(&entry->date, opts->output);

		fprintf(opts->output, PROPS_END);
		fprintf(opts->output, "\n");
	}

	/*  Setup the delta editor */
	editor = svn_delta_default_editor(pool);
	editor->set_target_revision = set_target_revision;
	editor->open_root = open_root;
	editor->delete_entry = delete_entry;
	editor->add_directory = add_directory;
	editor->open_directory = open_directory;
	editor->add_file = add_file;
	editor->open_file = open_file;
	editor->apply_textdelta = apply_textdelta;
	editor->close_file = close_file;
	editor->close_directory = close_directory;
	editor->change_file_prop = change_file_prop;
	editor->change_dir_prop = change_dir_prop;
	editor->close_edit = close_edit;
	editor->absent_directory = absent_directory;
	editor->absent_file = absent_file;
	editor->abort_edit = abort_edit;

	editor_baton = malloc(sizeof(baton_t));
	editor_baton->opts = opts;

	if (prev_created == 0) {
		prev.revision = 0;
		prev_created = 1;
	}

	ret = wsvn_do_diff(opts, &prev, entry, editor, editor_baton);

	prev.revision = entry->revision;

	if (opts->verbosity >= 0) {
		if (opts->verbosity > 0) {
			fprintf(stderr, "\033[1A\033[K");
		}

		if (opts->keep_revnums || !strlen(opts->repo_prefix)) {
			fprintf(stderr, _("* Dumped revision %ld.\n"), entry->revision);
		} else {
			fprintf(stderr, _("* Dumped revision %ld (local %ld).\n"), entry->revision, local_revnum);
		}
	}

	free(editor_baton);
	svn_pool_clear(pool);
	svn_pool_destroy(pool);
	return ret;
}
