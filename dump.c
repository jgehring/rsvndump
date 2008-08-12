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

#include <stdlib.h>
#include <string.h>


#define MAX_LINE_SIZE 4096 
#define MAX_COMMENT_SIZE (MAX_LINE_SIZE*64) 
#define SEPERATOR "------------------------------------------------------------------------\n"
#define PROPS_END "PROPS-END\n"
#define PROPS_END_LEN 10 


// File globals
static int rev_number = 0; 
static int repo_rev_number;


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


// Gets the string length of a property
static int strlen_property(prop_t *prop)
{
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
}


// Dumps a node
static void dump_node(change_entry_t *entry)
{
	char *realpath = malloc(strlen(repo_url)+strlen(entry->path+repo_prefix_len)+2);	
	strcpy(realpath, repo_url);
	if ((repo_url[strlen(repo_url)-1] != '/') && (entry->path[repo_prefix_len] != '/')) {
		strcat(realpath, "/");
	}
	strcat(realpath, entry->path+repo_prefix_len);

	// Write node header
	if (*(entry->path+repo_prefix_len) == '/') {
		fprintf(output, "Node-path: %s\n", entry->path+repo_prefix_len+1);
	} else {
		fprintf(output, "Node-path: %s\n", entry->path+repo_prefix_len);
	}
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

	if (entry->action != NK_DELETE) {
		int prop_length = PROPS_END_LEN;
		svn_stream_t *stream = NULL; 
		char *textbuffer = NULL;
		int textlen = 0;

		list_t props = svn_list_props(realpath, repo_rev_number);
		int i;
		for (i = 0; i < props.size; i++) {
			prop_length += strlen_property((prop_t *)props.elements + i);
		}

		fprintf(output, "Prop-content-length: %d\n", prop_length);
		if (entry->kind == NK_FILE && entry->action != NK_DELETE) {
			stream = svn_open(realpath, repo_rev_number, &textbuffer, &textlen); 
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
			for (i = 0; i < textlen; i++) {
				fputc(textbuffer[i], output);
			}
			svn_close(stream);
		}
	}

	fprintf(output, "\n");
	fprintf(output, "\n");

	free(realpath);
}


#if 0
// Dumps a complete revision
static char dump_revision(revision_t *rev)
{
	int i;

	// Write revision header
	fprintf(output, "Revision-number: %d\n", rev_number);	
	fprintf(output, "Prop-content-length: %d\n", rev->props_length);
	fprintf(output, "Content-length: %d\n", rev->props_length);	

	// Write properties
	for (i = 0; i < rev->properties.size; i++) {
		dump_property((prop_t *)rev->properties.elements + i);
	}
	fprintf(output, "PROPS-END\n");
	fprintf(output, "\n");

	// Write nodes 
	for (i = 0; i < rev->nodes.size; i++) {
		dump_node((node_t *)rev->nodes.elements + i);
	}
	fprintf(output, "\n");
	
	++rev_number;
	return 0;
}
#endif


// Dumps an entire repository
char dump_repository()
{
	int i;
	char *linebuffer = malloc(MAX_LINE_SIZE);
	char *commentbuffer = malloc(MAX_COMMENT_SIZE);

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

	fgets(linebuffer, MAX_LINE_SIZE-1, input);
	while (linebuffer != NULL && !feof(input)) {
		if (!strcmp(linebuffer, SEPERATOR)) {
			fgets(linebuffer, MAX_LINE_SIZE-1, input);
			continue;
		}

		// Parse log entry 
		repo_rev_number = atoi(linebuffer+1);
		char *author = NULL;
		char *lptr = linebuffer+2;
		while (lptr && *lptr) {
			if (*lptr == '|' && *(lptr+1) && *(lptr+2) && author == NULL) {
				lptr += 2;
				int l = strchr(lptr, '|')-lptr;
				author = malloc(l);
				strncpy(author, lptr, l-1);
				author[l-1] = '\0';
			}
			++lptr;
		}

		if (!quiet) {
			fprintf(stderr, "Dumping revision %d (local: %d)...\n", repo_rev_number, rev_number);
		}

		props_length = PROPS_END_LEN;
		
		// Write revision properties
		char *logmsg, *date;
		char t[10];
		svn_log(repo_url, repo_rev_number, &logmsg, &date);
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

		// Write nodes
		list_t changes = svn_list_changes(repo_url, repo_rev_number);
		for (i = 0; i < changes.size; i++) {
			change_entry_t *entry = ((change_entry_t *)changes.elements + i);
			char *realpath = malloc(strlen(repo_url)+strlen(entry->path+repo_prefix_len)+2);
			strcpy(realpath, repo_url);
			if ((repo_url[strlen(repo_url)-1] != '/') && (entry->path[repo_prefix_len] != '/')) {
				strcat(realpath, "/");
			}
			strcat(realpath, entry->path+repo_prefix_len);

			if (entry->action != NK_DELETE) {
				entry->kind = svn_get_kind(realpath, repo_rev_number);
			} else {
				entry->kind = NK_FILE;
			}

			free(realpath);
			if (!quiet) {
				fprintf(stderr, "\033[1A");
				fprintf(stderr, "Dumping revision %d (local: %d)... %d%%\n", repo_rev_number, rev_number, (i*50)/changes.size);
			}
		}
		qsort(changes.elements, changes.size, changes.elsize, compare_changes);
		for (i = 0; i < changes.size; i++) {
			dump_node((change_entry_t *)changes.elements + i);
			free_node((change_entry_t *)changes.elements + i);
			if (!quiet) {
				fprintf(stderr, "\033[1A");
				fprintf(stderr, "Dumping revision %d (local: %d)... %d%%\n", repo_rev_number, rev_number, 50+(i*50)/changes.size);
			}
		}
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

		if (!quiet) {
			fprintf(stderr, "\033[1A");
			fprintf(stderr, "Dumping revision %d (local: %d)... done\n", repo_rev_number, rev_number);
		}

		++rev_number;
	}

	free(linebuffer);
	free(commentbuffer);
	return 0;
}

