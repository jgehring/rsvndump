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
#include "dump.h"
#include "svn_functions.h"
#include "util.h"

#include <search.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>


#define MAX_LINE_SIZE 4096 
#define MAX_COMMENT_SIZE (MAX_LINE_SIZE*64) 
#define SEPERATOR "------------------------------------------------------------------------\n"
#define PROPS_END "PROPS-END\n"
#define PROPS_END_LEN 10 


// File globals
static int rev_number = 0; 
static int repo_rev_number = 0;
static list_t rev_map;

// Prototypes
static void dump_copy(change_entry_t *entry);


// Compares two changes
static int compare_changes(const void *a, const void *b)
{
	char *ap = ((change_entry_t *)a)->path;
	char *bp = ((change_entry_t *)b)->path;
	if (!strncmp(ap, bp, strlen(ap)) && strlen(ap) < strlen(bp)) {
		// a is prefix of b and must be commited first
		return -1; 
	} else if (!strncmp(ap, bp, strlen(bp)) && strlen(bp) < strlen(ap)) {
		return 1;
	}
	return strcmp(((change_entry_t *)a)->path, ((change_entry_t *)b)->path);
}


// Allocates and returns the real path for a node
static char *get_real_path(char *nodename)
{
	char *realpath;
	if (online) {
		realpath = malloc(strlen(repo_url)+strlen(nodename+strlen(repo_prefix))+2);
		strcpy(realpath, repo_url);
		if ((repo_url[strlen(repo_url)-1] != '/') && (nodename[strlen(repo_prefix)] != '/')) {
			strcat(realpath, "/");
		}
		strcat(realpath, nodename+strlen(repo_prefix));
	} else {
		fprintf(stderr, "get_real_path(%s): repo_dir=%s, repo_prefix=%s ", nodename, repo_dir, repo_prefix);
		realpath = malloc(strlen(repo_dir)+strlen(nodename+strlen(repo_prefix))+2);
		strcpy(realpath, repo_dir);
		if ((repo_dir[strlen(repo_dir)-1] != '/') && (nodename[strlen(repo_prefix)] != '/')) {
			strcat(realpath, "/");
		}
		strcat(realpath, nodename+strlen(repo_prefix));
		fprintf(stderr, "-> realpath=%s\n", realpath);
	}

	return realpath;
}


// Gets the string length of a property
static int strlen_property(prop_t *prop)
{
	if (!prop->key || !prop->value) {
		return 0;
	}
	int len = 0;
	char buffer[2048];
	sprintf(buffer, "K %d\n", strlen(prop->key));
	len += strlen(buffer);
	sprintf(buffer, "%s\n", prop->key);
	len += strlen(buffer);
	sprintf(buffer, "V %d\n", strlen(prop->value));
	len += strlen(buffer);
	sprintf(buffer, "%s\n", prop->value);
	return (len+strlen(buffer));
}


// Frees all node memory
static void free_property(prop_t *prop)
{
	if (prop->key) {
		free(prop->key);
	}
	if (prop->value) {
		free(prop->value);
	}
}


// Dumps a property 
static void dump_property(prop_t *prop)
{
	if (!prop->key || !prop->value) {
		return;
	}
	fprintf(output, "K %d\n", strlen(prop->key));
	fprintf(output, "%s\n", prop->key);
	fprintf(output, "V %d\n", strlen(prop->value));
	fprintf(output, "%s\n", prop->value);
}


// Frees all node memory
static void free_node(change_entry_t *entry)
{
	if (entry->path) {
		free(entry->path);
	}
	if (entry->copy_from_path) {
		free(entry->copy_from_path);
	}
}


// Dumps a node
static void dump_node(change_entry_t *entry)
{
	char *tpath;
	if (*(entry->path+strlen(repo_prefix)) == '/') {
		tpath = entry->path+strlen(repo_prefix)+1;
	} else {
		tpath = entry->path+strlen(repo_prefix);
	}

	if (entry->kind == NK_NONE || !entry->path || (entry->copy_from_path == NULL && strlen(tpath) == 0)) {
#if DEBUG
		fprintf(stderr, "\nThis should not happen.. \n\n");
#endif
		return;
	}

	char *realpath = get_real_path((char *)entry->path);

	if (entry->copy_from_path) {
		// The node is a copy, but there is always the possibility that the source revision has not been dumped
		// because it resides on a top-level path. 
		int r;
		for (r = rev_number; r > 0; r--) {
			if (((int *)rev_map.elements)[r] == entry->copy_from_rev) {
				break;
			}
		}
		if (r == 0) {
			// Here we are. This is really painful, because we need to dump the source of the copy now.
			// If the entry is a directory, it needs to be dumped recursivly (handled by dump_tree later)
			entry->action = NK_ADD;
			entry->use_copy = 0;
		} else {
			entry->copy_from_rev = r;
			entry->use_copy = 1;
		}
	}

	// Write node header
	if (strlen(tpath)) {
		fprintf(output, "Node-path: %s\n", tpath);
		if (entry->action != NK_DELETE) {
			fprintf(output, "Node-kind: %s\n", entry->kind == NK_FILE ? "file" : "dir");
		}
		fprintf(output, "Node-action: "); 
		switch (entry->action) {
			case NK_CHANGE:
				fprintf(output, "change\n"); 
				break;
			case NK_ADD:
				fprintf(output, "add\n"); 
				break;
			case NK_DELETE:
				fprintf(output, "delete\n"); 
				break;
			case NK_REPLACE:
				fprintf(output, "replace\n"); 
				break;
		}

		if (entry->copy_from_path && entry->use_copy == 1) {
			if (*(entry->copy_from_path+strlen(repo_prefix)) == '/') {
				fprintf(output, "Node-copyfrom-path: %s\n", entry->copy_from_path+strlen(repo_prefix)+1);
			} else {
				fprintf(output, "Node-copyfrom-path: %s\n", entry->copy_from_path+strlen(repo_prefix));
			}
			fprintf(output, "Node-copyfrom-rev: %d\n", entry->copy_from_rev);
		}

		if (entry->action != NK_DELETE) {
			int prop_length = PROPS_END_LEN;
			svn_stream_t *stream = NULL; 
			char *textbuffer = NULL;
			int textlen = 0;

			int i;
			list_t props;
			if (entry->use_copy) {
				props = svn_list_props(realpath, entry->copy_from_rev);
			} else {
				props = svn_list_props(realpath, repo_rev_number);
			}
			for (i = 0; i < props.size; i++) {
				prop_length += strlen_property((prop_t *)props.elements + i);
			}

			fprintf(output, "Prop-content-length: %d\n", prop_length);
			if (entry->kind == NK_FILE && entry->action != NK_DELETE) {
				if (online) {
					stream = svn_open(realpath, repo_rev_number, &textbuffer, &textlen); 
				} else {
					struct stat st;
					stat(realpath, &st);
					textlen = (int)st.st_size;
				}
				fprintf(output, "Text-content-length: %d\n", textlen);
			}
			fprintf(output, "Content-length: %d\n", prop_length+textlen);
			fprintf(output, "\n");

			for (i = 0; i < props.size; i++) {
				dump_property((prop_t *)props.elements + i);
				free_property((prop_t *)props.elements + i);
			}
			fprintf(output, PROPS_END);

			if (entry->kind == NK_FILE && entry->action != NK_DELETE) {
				if (online) {
					for (i = 0; i < textlen; i++) {
						fputc(textbuffer[i], output);
					}
					svn_close(stream);
				} else {
					FILE *f = fopen(realpath, "r");
					if (f != NULL) {
						int c;
						while ((c = fgetc(f)) != EOF) {
							fputc(c, output);
						}
						fclose(f);
					} else {
						fprintf(stderr, "\nFatal: Unable to open '%s' for reading (%d)\n", realpath, errno); 
						exit(1);
					}
				}
			}
			list_free(&props);
		}

		fprintf(output, "\n");
		fprintf(output, "\n");
	}

	if (entry->use_copy == 0 && entry->kind == NK_DIRECTORY) {
		dump_copy(entry);
	}

	free(realpath);
}


// Recursively dumps a given entry 
static void dump_copy(change_entry_t *entry)
{
	int i;

	// Copy resolving has to be done online atm, sorry
	char ton = online;
	online = 1;

	char *rcopypath = malloc(strlen(repo_base)+strlen(entry->copy_from_path)+2);
	strcpy(rcopypath, repo_base);
	if ((repo_base[strlen(repo_base)-1] != '/') && (*entry->copy_from_path != '/')) {
		strcat(rcopypath, "/");
	}
	strcat(rcopypath, entry->copy_from_path); 
	char *realpath = malloc(strlen(repo_base)+strlen(entry->path)+2);
	strcpy(realpath, repo_base);
	if ((repo_base[strlen(repo_base)-1] != '/') && (*entry->path != '/')) {
		strcat(realpath, "/");
	}
	strcat(realpath, entry->path); 

	list_t list = svn_list_path(rcopypath, entry->copy_from_rev);

	for (i = list.size-1; i >= 0; i--) {
		change_entry_t *te = ((change_entry_t *)list.elements + i);
		char *mrealpath = malloc(strlen(repo_base)+strlen(te->path)+2);
		strcpy(mrealpath, repo_base);
		if ((repo_base[strlen(repo_base)-1] != '/') && (*te->path != '/')) {
			strcat(mrealpath, "/");
		}
		strcat(mrealpath, te->path); 

		if (te->action != NK_DELETE) {
			te->kind = svn_get_kind(mrealpath, te->copy_from_rev);
		} else {
			te->kind = NK_FILE;
		}

		// We need to replace the old path (copy-from) with the new one
		char *newpath = malloc(strlen(te->path)-strlen(entry->copy_from_path)+strlen(entry->path)+3);
		strcpy(newpath, entry->path);
		strcat(newpath, te->path+strlen(entry->copy_from_path)); 

		free(te->path);
		te->path = newpath;
		free(mrealpath);

		if (hsearch((ENTRY){te->path, NULL}, FIND) != NULL) {
			// Duplicate item -> remove
			list_remove(&list, i);
		}
	}
	qsort(list.elements, list.size, list.elsize, compare_changes);
	for (i = 0; i < list.size; i++) {
		dump_node((change_entry_t *)list.elements + i);
		free_node((change_entry_t *)list.elements + i);
	}

	list_free(&list);
	free(rcopypath);
	free(realpath);

	online = ton;
}


// Dumps an entire repository
char dump_repository()
{
	int i;
	char first = 1;
	char *linebuffer = malloc(MAX_LINE_SIZE);
	list_init(&rev_map, sizeof(int)),

	fprintf(output, "SVN-fs-dump-format-version: %s\n", DUMPFORMAT_VERSION);
	fprintf(output, "\n");

	// TODO
	if (repo_uuid) {
		fprintf(output, "UUID: %s\n", repo_uuid);
		fprintf(output, "\n");
	}

	// Write initial revision header
	int props_length = PROPS_END_LEN;
	fprintf(output, "Revision-number: %d\n", rev_number);	
	fprintf(output, "Prop-content-length: %d\n", props_length);
	fprintf(output, "Content-length: %d\n\n", props_length);	

	fprintf(output, PROPS_END);
	fprintf(output, "\n");
	++rev_number;
	int temp = 0;
	list_add(&rev_map, &temp); 

	fgets(linebuffer, MAX_LINE_SIZE-1, input);
	while (linebuffer != NULL && !feof(input)) {
		if (!strcmp(linebuffer, SEPERATOR)) {
			fgets(linebuffer, MAX_LINE_SIZE-1, input);
			continue;
		}

		svn_alloc_rev_pool();

		// Parse log entry 
		repo_rev_number = atoi(linebuffer+1);

		if (verbosity > 0) {
			fprintf(stderr, "* Dumping revision %d (local: %d)...\n", repo_rev_number, rev_number);
		}
		list_add(&rev_map, &repo_rev_number);

		props_length = PROPS_END_LEN;

		if (!online) {
			if (first != 0) {
				svn_checkout(repo_url, repo_dir, repo_rev_number);
				first = 0;
			} else {
				svn_update_path(repo_dir, repo_rev_number);
			}
		}
		
		// Write revision properties
		char *author = NULL, *logmsg = NULL, *date = NULL;
		char t[10];
		svn_log(repo_url, repo_rev_number, &author, &logmsg, &date);
		if (logmsg && strlen(logmsg)) {
			sprintf(t, "%d", strlen(logmsg));
			props_length += 16+strlen(t);
			props_length += strlen(logmsg);
		}
		if (author && strlen(author)) {
			sprintf(t, "%d", strlen(author));
			props_length += 20+strlen(t);
			props_length += strlen(author);
		}
		if (date && strlen(date)) {
			sprintf(t, "%d", strlen(date));
			props_length += 17+strlen(t);
			props_length += strlen(date);
		}

		// Write initial revision header
		fprintf(output, "Revision-number: %d\n", rev_number);	
		fprintf(output, "Prop-content-length: %d\n", props_length);
		fprintf(output, "Content-length: %d\n\n", props_length);

		if (logmsg && strlen(logmsg)) {
			fprintf(output, "K 7\n");
			fprintf(output, "svn:log\n");
			fprintf(output, "V %d\n", strlen(logmsg));
			fprintf(output, "%s\n", logmsg);
		}
		if (author && strlen(author)) {
			fprintf(output, "K 10\n");
			fprintf(output, "svn:author\n");
			fprintf(output, "V %d\n", strlen(author));
			fprintf(output, "%s\n", author);
		}
		if (date && strlen(date)) {
			fprintf(output, "K 8\n");
			fprintf(output, "svn:date\n");
			fprintf(output, "V %d\n", strlen(date));
			fprintf(output, "%s\n", date);
		}

		fprintf(output, PROPS_END);
		fprintf(output, "\n");

		list_t changes;
		changes = svn_list_changes(online ? repo_url : repo_dir, repo_rev_number);

		// Get node type info and insert into hash to prevent duplicates (this can happen
		// when nodes are copied)
		hcreate((int)((float)changes.size/0.8f));
		for (i = 0; i < changes.size; i++) {
			change_entry_t *entry = ((change_entry_t *)changes.elements + i);
			char *realpath = get_real_path(entry->path);

			if (entry->action != NK_DELETE) {
				entry->kind = svn_get_kind(realpath, repo_rev_number);
			} else {
				entry->kind = NK_FILE;
			}

			if (verbosity > 0 && online) {
				fprintf(stderr, "\033[1A");
				fprintf(stderr, "* Dumping revision %d (local: %d)... %d%%\n", repo_rev_number, rev_number, (i*50)/changes.size);
			}

			hsearch((ENTRY){entry->path, NULL}, ENTER);
			free(realpath);
		}

		// Determine insertion oder
		qsort(changes.elements, changes.size, changes.elsize, compare_changes);

		// Write nodes
		for (i = 0; i < changes.size; i++) {
			dump_node((change_entry_t *)changes.elements + i);
			free_node((change_entry_t *)changes.elements + i);
			if (verbosity > 0) {
				fprintf(stderr, "\033[1A");
				if (online) {
					fprintf(stderr, "* Dumping revision %d (local: %d)... %d%%\n", repo_rev_number, rev_number, 50+(i*50)/changes.size);
				} else {
					fprintf(stderr, "* Dumping revision %d (local: %d)... %d%%\n", repo_rev_number, rev_number, (i*100)/changes.size);
				}
			}
		}

		hdestroy();
		list_free(&changes);

		if (author) {
			free(author);
		}
		if (logmsg) {
			free(logmsg);
		}
		if (date) {
			free(date);
		}
		fgets(linebuffer, MAX_LINE_SIZE-1, input);

		svn_free_rev_pool();

		if (verbosity > 0) { 
			fprintf(stderr, "\033[1A");
			fprintf(stderr, "* Dumping revision %d (local: %d)... done\n", repo_rev_number, rev_number);
		} else if (verbosity == 0) {
			fprintf(stderr, "* Dumped revision %d (local: %d)\n", repo_rev_number, rev_number);
		}

		++rev_number;
	}

	list_free(&rev_map);
	free(linebuffer);
	return 0;
}
