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
 *      file: node.c
 *      desc: Represents a node that has been changed in a revision
 */


#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <svn_repos.h>

#include "main.h"
#include "node.h"
#include "logentry.h"
#include "whash.h"
#include "wsvn.h"


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/

/* Dumps the contents of a node which is the destination of a copy action */
static char node_prepare_copy(node_t *node, dump_options_t *opts, list_t *logs, int local_revnum)
{
	/* First, check if the source is reachable, i.e. can be found under
	   the current session root */
	if (!strncmp(node->copy_from_path, opts->repo_prefix, strlen(opts->repo_prefix))) {
		/* If we sync the revision numbers, the copy-from revision is correct */
		if (opts->keep_revnums) {
			node->use_copy = 1;
			return 0;
		}

		/* This is good news: we already dumped the source. Let's figure
		   out at which revision */
		svn_revnum_t r, rr = -1;
		svn_revnum_t mind = LONG_MAX;

		/* Find the best matching revision.
		   This will work, because if we have not dumped the requested
		   revision itself, the source of the copy has not changed between
		   the best matching and the requested revision. */
		for (r = local_revnum-1; r > 0; r--) {
			/* Yes, the +1 is needed */
			svn_revnum_t d = (node->copy_from_rev - (((logentry_t *)logs->elements)[r].revision))+1;
#if DEBUG
			fprintf(stderr, "node_prepare_copy: req: %ld cur: %ld, local: %ld\n", node->copy_from_rev, (((logentry_t *)logs->elements)[r].revision), r);
#endif
			/* TODO: This can be optimized: Once we notice that the distance to the
			   requested revision gets bigger, it should be safe to break out of this
			   loop. */
			if (d >= 0 && d < mind) {
				mind = d;
				rr = r;
				if (d <= 1) {
					break;
				}
			}
		}

		node->copy_from_rev = rr;
		node->use_copy = 1;
#if DEBUG
		fprintf(stderr, "node_prepare_copy: using local %ld\n", rr);
#endif
	} else {
		/* Hm, this is bad. we have to ignore the copy operation and
		   simulate it by simple dumping it the node as being added.
		   This will work fine for single files, but directories
		   must be dumped recursively. */
		node->action = NA_ADD;
		node->use_copy = 0;
	}

	return 0;
}


/* Recursively dumps a directory that is the destination of a copy action */
static char node_dump_copydir(node_t *node, dump_options_t *opts, list_t *logs, svn_revnum_t revnum, svn_revnum_t local_revnum)
{
	unsigned int i;
	list_t cnodes = list_create(sizeof(node_t));
	if (wsvn_find(node, &cnodes, revnum)) {
		list_free(&cnodes);
		return 1;
	}

	for (i = 0; i < cnodes.size; i++) {
		node_t *n = (node_t *)cnodes.elements + i;
		if (whash_contains(n->path)) {
			node_free(n);
			list_remove(&cnodes, i);
			--i;
		}
		n->action = NA_ADD;
	}
	list_qsort(&cnodes, nodecmp);
	for (i = 0; i < cnodes.size; i++) {
		node_t *n = (node_t *)cnodes.elements + i;
		node_dump(n, opts, logs, revnum, local_revnum);
		if (n->kind == NK_DIRECTORY) {
			node_dump_copydir(n, opts, logs, revnum, local_revnum);
		}
		node_free(n);
	}
	
	list_free(&cnodes);

	return 0;
}


/* Write node copy-from information */
static void node_write_copyinfo(node_t *node, dump_options_t *opts)
{
	if (node->copy_from_path && node->use_copy == 1) {
		int offset = strlen(opts->repo_prefix);
		while (*(node->copy_from_path+offset) == '/') {
			++offset;
		}
		fprintf(opts->output, "%s: %ld\n", SVN_REPOS_DUMPFILE_NODE_COPYFROM_REV, node->copy_from_rev);
		if (opts->user_prefix != NULL) {
			fprintf(opts->output, "%s: %s%s\n", SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH, opts->user_prefix, node->copy_from_path+offset);
		} else {
			fprintf(opts->output, "%s: %s\n", SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH, node->copy_from_path+offset);
		}
	}
}


/* Dumps a node in online mode */
static char node_dump_online(node_t *node, dump_options_t *opts, list_t *logs, svn_revnum_t revnum, svn_revnum_t local_revnum)
{
	/* Write node header */
	if (node->kind == NK_FILE || strcmp(node->path, ".")) {
		char *path = node->path;
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

		node_write_copyinfo(node, opts);

		/* Write node contents */
		if (node->action != NA_DELETE && !(node->copy_from_path != NULL && node->use_copy)) {
			unsigned int i;
			unsigned int prop_len = 0;
			list_t props;
			if (node->has_props) {
				props = list_create(sizeof(property_t));
				prop_len = PROPS_END_LEN;
				if (wsvn_get_props(node, &props, revnum)) {
					list_free(&props);
					return 1;
				}

				for (i = 0; i < props.size; i++) {
					property_t *p = (property_t *)props.elements + i; 
					prop_len += property_strlen(p);
				}
				fprintf(opts->output, "%s: %d\n", SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH, prop_len);
			} else if (node->action == NA_ADD) {
				props = list_create(sizeof(property_t));
				prop_len = PROPS_END_LEN;
				fprintf(opts->output, "%s: %d\n", SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH, prop_len);
			}

			if (node->kind == NK_FILE && node->action != NA_DELETE) {
				fprintf(opts->output, "%s: %d\n", SVN_REPOS_DUMPFILE_TEXT_CONTENT_LENGTH, node->size);
			}

			fprintf(opts->output, "%s: %d\n", SVN_REPOS_DUMPFILE_CONTENT_LENGTH, prop_len+node->size);
			fprintf(opts->output, "\n");

			if (node->has_props || node->action == NA_ADD) {
				for (i = 0; i < props.size; i++) {
					property_t *p = (property_t *)props.elements + i; 
					property_dump(p, opts->output);
					free((char *)p->key);
					property_free(p);
				}
				fprintf(opts->output, PROPS_END);
				list_free(&props);
			}

			if (node->kind != NK_DIRECTORY) {
				wsvn_cat(node, revnum, opts->output);
			}
		}

		fprintf(opts->output, "\n\n");
	}

	return 0;
}


/* Dumps a node in offline mode */
static char node_dump_offline(node_t *node, dump_options_t *opts, list_t *logs, svn_revnum_t revnum, svn_revnum_t local_revnum)
{
	/* Write node header */
	if (node->kind == NK_FILE || strcmp(node->path, ".")) {
		char *path = node->path;
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

		node_write_copyinfo(node, opts);

		/* Write node contents */
		if (node->action != NA_DELETE && !(node->copy_from_path != NULL && node->use_copy)) {
			unsigned int i;
			unsigned int prop_len = 0;
			list_t props;
			/* TODO: There is no cheap property checking in
			   offline mode by now. */
			props = list_create(sizeof(property_t));
			prop_len = PROPS_END_LEN;
			if (wsvn_get_wc_props(node, &props)) {
				list_free(&props);
				return 1;
			}

			for (i = 0; i < props.size; i++) {
				property_t *p = (property_t *)props.elements + i; 
				prop_len += property_strlen(p);
			}
			if (props.size != 0 || node->action == NA_ADD) {
				fprintf(opts->output, "%s: %d\n", SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH, prop_len);
			} else {
				prop_len = 0;
			}

			if (node->kind == NK_FILE && node->action != NA_DELETE) {
				fprintf(opts->output, "%s: %d\n", SVN_REPOS_DUMPFILE_TEXT_CONTENT_LENGTH, node->size);
			}

			fprintf(opts->output, "%s: %d\n", SVN_REPOS_DUMPFILE_CONTENT_LENGTH, prop_len+node->size);
			fprintf(opts->output, "\n");

			if (props.size != 0 || node->action == NA_ADD) {
				for (i = 0; i < props.size; i++) {
					property_t *p = (property_t *)props.elements + i; 
					property_dump(p, opts->output);
					free((char *)p->key);
					property_free(p);
				}
				fprintf(opts->output, PROPS_END);
			}
			list_free(&props);

			if (node->kind != NK_DIRECTORY) {
				/* We're in offline mode, so let's simply cat the file */
				FILE *f;
				char *buffer = malloc(2049);
				size_t s;
				char *path = malloc(strlen(opts->repo_dir)+strlen(node->path)+2);
				sprintf(path, "%s/%s", opts->repo_dir, node->path);
				f = fopen(path, "rb");
				if (f == NULL) {
					fprintf(stderr, "Failed to open %s\n", path);
					free(path);
					free(buffer);
					return 1;
				}
				while ((s = fread(buffer, 1, 2048, f))) {
					fwrite(buffer, 1, s, opts->output);
				}
				fclose(f);
				free(path);
				free(buffer);
			}
		}

		fprintf(opts->output, "\n\n");
	}

	return 0;
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/

/* Creates a new node with default values */
node_t node_create()
{
	node_t node;
	node.path = NULL;
	node.kind = NK_NONE;
	node.action = -1;
	node.copy_from_path = NULL;
	node.copy_from_rev = -2;
	node.use_copy = -1;
	node.has_props = 0;
	node.size = 0;
	return node;
}


/* Destroys a node, freeing char-pointers if they are not NULL */
void node_free(node_t *node)
{
	if (node->path) {
		free(node->path);
	}
	if (node->copy_from_path) {
		free(node->copy_from_path);
	}
}


/* Compares two nodes */
int nodecmp(const void *a, const void *b)
{
	char *ap = ((node_t *)a)->path;
	char *bp = ((node_t *)b)->path;
	nodeaction_t aa = ((node_t *)a)->action;
	nodeaction_t ba = ((node_t *)b)->action;
	if (!strncmp(ap, bp, strlen(ap)) && strlen(ap) < strlen(bp)) {
		/* a is prefix of b and must be commited first */
		return -1; 
	} else if (!strncmp(ap, bp, strlen(bp)) && strlen(bp) < strlen(ap)) {
		return 1;
	} else if (aa != ba) {
		return (aa < ba ? -1 : 1);
	}
	return strcmp(((node_t *)a)->path, ((node_t *)b)->path);
}


/* Dumps the contents of a node, using online mode */
char node_dump(node_t *node, dump_options_t *opts, list_t *logs, svn_revnum_t revnum, svn_revnum_t local_revnum)
{
	char ret;

	/* Check for copy */
	if (node->copy_from_path) {
		node_prepare_copy(node, opts, logs, local_revnum);
	}

	if (opts->online) {
		ret = node_dump_online(node, opts, logs, revnum, local_revnum);
	} else {
		ret = node_dump_offline(node, opts, logs, revnum, local_revnum);
	}

	if (ret) {
		return 1;
	}

	if (node->kind == NK_DIRECTORY && node->copy_from_path && node->use_copy == 0) {
		return node_dump_copydir(node, opts, logs, revnum, local_revnum);
	}

	return 0;
}
