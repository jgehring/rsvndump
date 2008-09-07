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

#include <svn_repos.h>


#define MAX_LINE_SIZE 4096 
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
	nodeaction_t aa = ((change_entry_t *)a)->action;
	nodeaction_t ba = ((change_entry_t *)b)->action;
	if (!strncmp(ap, bp, strlen(ap)) && strlen(ap) < strlen(bp)) {
		// a is prefix of b and must be commited first
		return -1; 
	} else if (!strncmp(ap, bp, strlen(bp)) && strlen(bp) < strlen(ap)) {
		return 1;
	} else if (aa != ba) {
		return (aa < ba ? -1 : 1);
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
		realpath = malloc(strlen(repo_dir)+strlen(nodename+strlen(repo_prefix))+2);
		strcpy(realpath, repo_dir);
		if ((repo_dir[strlen(repo_dir)-1] != '/') && (nodename[strlen(repo_prefix)] != '/')) {
			strcat(realpath, "/");
		}
		strcat(realpath, nodename+strlen(repo_prefix));
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
#ifdef DEBUG
		fprintf(stderr, "\nThis should not happen.. \n\n");
#endif
		return;
	}

	char *realpath = get_real_path((char *)entry->path);

	if (entry->copy_from_path) {
		// The node is a copy, but there is always the possibility that the source revision has not been dumped
		// because it resides on a top-level path. Or, the source path will not be dumped at all
		int r;
		for (r = rev_number; r > 0; r--) {
			if (((int *)rev_map.elements)[r] == entry->copy_from_rev) {
				break;
			}
		}
		if (r == 0 || strncmp(entry->path, entry->copy_from_path, strlen(repo_prefix))) {
			// Here we are. This is really painful, because we need to dump the source of the copy now.
			// If the entry is a directory, it needs to be dumped recursivly (handled by dump_copy later)
			entry->action = NK_ADD;
			entry->use_copy = 0;
		} else {
			entry->copy_from_rev = r;
			entry->use_copy = 1;
		}
	}

	// Write node header
	if (strlen(tpath)) {
		if (user_prefix != NULL) {
			fprintf(output, "%s: %s%s\n", SVN_REPOS_DUMPFILE_NODE_PATH, user_prefix, tpath);
		} else {
			fprintf(output, "%s: %s\n", SVN_REPOS_DUMPFILE_NODE_PATH, tpath);
		}
		if (entry->action != NK_DELETE) {
			fprintf(output, "%s: %s\n", SVN_REPOS_DUMPFILE_NODE_KIND, entry->kind == NK_FILE ? "file" : "dir");
		}
		fprintf(output, "%s: ", SVN_REPOS_DUMPFILE_NODE_ACTION ); 
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
				if (user_prefix != NULL) {
					fprintf(output, "%s: %s%s\n", SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH, user_prefix, entry->copy_from_path+strlen(repo_prefix)+1);
				} else {
					fprintf(output, "%s: %s\n", SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH, entry->copy_from_path+strlen(repo_prefix)+1);
				}
			} else {
				if (user_prefix != NULL) {
					fprintf(output, "%s: %s%s\n", SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH, user_prefix, entry->copy_from_path+strlen(repo_prefix));
				} else {
					fprintf(output, "%s: %s\n", SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH, entry->copy_from_path+strlen(repo_prefix));
				}
			}
			fprintf(output, "%s: %d\n", SVN_REPOS_DUMPFILE_NODE_COPYFROM_REV, entry->copy_from_rev);
		}

		if (entry->action != NK_DELETE) {
			int prop_length = 0;
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
			if (props.size > 0 || entry->action == NK_ADD) { // svnadmin's behaviour
				prop_length += PROPS_END_LEN;
				fprintf(output, "%s: %d\n", SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH, prop_length);
			}

			if (entry->kind == NK_FILE && entry->action != NK_DELETE) {
				if (online) {
					stream = svn_open(realpath, repo_rev_number, &textbuffer, &textlen); 
				} else {
					struct stat st;
					stat(realpath, &st);
					textlen = (int)st.st_size;
				}
				fprintf(output, "%s: %d\n", SVN_REPOS_DUMPFILE_TEXT_CONTENT_LENGTH, textlen);
			}
			fprintf(output, "%s: %d\n", SVN_REPOS_DUMPFILE_CONTENT_LENGTH, prop_length+textlen);
			fprintf(output, "\n");

			if (props.size > 0 || entry->action == NK_ADD) { // svnadmin's behaviour
				for (i = 0; i < props.size; i++) {
					dump_property((prop_t *)props.elements + i);
					free_property((prop_t *)props.elements + i);
				}
				fprintf(output, PROPS_END);
			}


			if (entry->kind == NK_FILE && entry->action != NK_DELETE) {
				if (online) {
					if (stream != NULL) {
						for (i = 0; i < textlen; i++) {
							fputc(textbuffer[i], output);
						}
						svn_close(stream);
					}
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
#ifdef DEBUG
	fprintf(stderr, "dump_copy(%s)\n", entry->path);
#endif
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
			te->kind = svn_get_kind(mrealpath, entry->copy_from_rev);
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


// Create, and maybe cleanup, user prefix path
static void create_user_prefix()
{
	if (user_prefix == NULL) {
		return;
	}
	char *new_prefix = malloc(strlen(user_prefix));
	new_prefix[0] = '\0';
	char *s = user_prefix, *e = user_prefix;
	while ((e = strchr(s, '/')) != NULL) {
		if (e-s < 1) {
			++s;
			continue;
		}
		strncpy(new_prefix+strlen(new_prefix), s, e-s+1);
		fprintf(output, "%s: ", SVN_REPOS_DUMPFILE_NODE_PATH);
		fwrite(new_prefix, 1, strlen(new_prefix)-1, output);
		fputc('\n', output);
		fprintf(output, "%s: dir\n", SVN_REPOS_DUMPFILE_NODE_KIND);
		fprintf(output, "%s: add\n\n", SVN_REPOS_DUMPFILE_NODE_ACTION ); 
		s = e+1;
	}
	strcat(new_prefix, s);
	free(user_prefix);
	user_prefix = new_prefix;
}	


// Dumps an entire repository
char dump_repository()
{
	int i;
	char first = 1;
	char *linebuffer = malloc(MAX_LINE_SIZE);
	list_init(&rev_map, sizeof(int)),

	fprintf(output, "%s: %d\n", SVN_REPOS_DUMPFILE_MAGIC_HEADER, DUMPFORMAT_VERSION);
	fprintf(output, "\n");

	// TODO
	if (repo_uuid) {
		fprintf(output, "UUID: %s\n", repo_uuid);
		fprintf(output, "\n");
	}

	// Write initial revision header
	int props_length = PROPS_END_LEN;
	fprintf(output, "%s: %d\n", SVN_REPOS_DUMPFILE_REVISION_NUMBER, rev_number);	
	fprintf(output, "%s: %d\n", SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH, props_length);
	fprintf(output, "%s: %d\n\n", SVN_REPOS_DUMPFILE_CONTENT_LENGTH, props_length);	

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
		fprintf(output, "%s: %d\n", SVN_REPOS_DUMPFILE_REVISION_NUMBER, rev_number);	
		fprintf(output, "%s: %d\n", SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH, props_length);
		fprintf(output, "%s: %d\n\n", SVN_REPOS_DUMPFILE_CONTENT_LENGTH, props_length);	

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

		// The first revision must contain an eventual user prefix
		if (user_prefix != NULL && rev_number == 1) {
			create_user_prefix();
		}

		list_t changes;
		changes = svn_list_changes(online ? repo_url : repo_dir, repo_rev_number);

		// Get node type info and insert into hash to prevent duplicates (this can happen
		// when nodes are copied)
		hcreate((int)((float)changes.size*1.5f));
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
