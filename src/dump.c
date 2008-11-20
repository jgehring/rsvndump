/*
 *	rsvndump - remote svn repository dump
 *	Copyright (C) 2008 Jonas Gehring
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
 *      file: dump.c
 *      desc: The main work is done here 
 */


#include <string.h>
#include <sys/stat.h>

#include "main.h"
#include "dump.h"
#include "list.h"
#include "logentry.h"
#include "whash.h"
#include "wsvn.h"
#ifdef USE_TIMING
 #include "utils.h"
#endif

#include <svn_pools.h>
#include <svn_repos.h>
#include <svn_time.h>


/*---------------------------------------------------------------------------*/
/* Static variables                                                          */
/*---------------------------------------------------------------------------*/

static dump_options_t *dopts = NULL;
static list_t *logs = NULL;


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/

/* Create, and maybe cleanup, user prefix path */
static void dump_create_user_prefix()
{
	char *new_prefix, *s, *e;
	if (dopts->user_prefix == NULL) {
		return;
	}
	new_prefix = calloc(strlen(dopts->user_prefix)+1, 1);
	new_prefix[0] = '\0';
	s = e = dopts->user_prefix;
	while ((e = strchr(s, '/')) != NULL) {
		if (e-s < 1) {
			++s;
			continue;
		}
		strncpy(new_prefix+strlen(new_prefix), s, e-s+1);
		fprintf(dopts->output, "%s: ", SVN_REPOS_DUMPFILE_NODE_PATH);
		fwrite(new_prefix, 1, strlen(new_prefix)-1, dopts->output);
		fputc('\n', dopts->output);
		fprintf(dopts->output, "%s: dir\n", SVN_REPOS_DUMPFILE_NODE_KIND);
		fprintf(dopts->output, "%s: add\n\n", SVN_REPOS_DUMPFILE_NODE_ACTION ); 
		s = e+1;
	}
	strcat(new_prefix, s);
	free(dopts->user_prefix);
	dopts->user_prefix = new_prefix;
}


/* Dumps a revision */
static char dump_revision(logentry_t *entry, svn_revnum_t local_revnum)
{
	int props_length;
	int i;
	int failed;
	list_t nodes;
#ifdef USE_TIMING
	stopwatch_t watch = stopwatch_create();
#endif
	if (dopts->verbosity > 0) {
		if (dopts->keep_revnums) {
			fprintf(stderr, "* Dumping revision %ld ... 0%%\n", entry->revision);
		} else {
			fprintf(stderr, "* Dumping revision %ld (local: %ld) ... 0%%\n", entry->revision, local_revnum);
		}
	}

	/* Update working if needed */
	if (dopts->online == 0) {
		wsvn_update(entry->revision);
	}

	/* Write revision header */
	props_length = 0;
	props_length += property_strlen(&entry->author);
	props_length += property_strlen(&entry->date);
	props_length += property_strlen(&entry->msg);
	if (props_length > 0) {
		props_length += PROPS_END_LEN;
	}

	if (dopts->keep_revnums) {
		fprintf(dopts->output, "%s: %ld\n", SVN_REPOS_DUMPFILE_REVISION_NUMBER, entry->revision);	
	} else {
		fprintf(dopts->output, "%s: %ld\n", SVN_REPOS_DUMPFILE_REVISION_NUMBER, local_revnum);	
	}
	fprintf(dopts->output, "%s: %d\n", SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH, props_length);
	fprintf(dopts->output, "%s: %d\n\n", SVN_REPOS_DUMPFILE_CONTENT_LENGTH, props_length);

	if (props_length > 0) {
		property_dump(&entry->msg, dopts->output);
		property_dump(&entry->author, dopts->output);
		property_dump(&entry->date, dopts->output);

		fprintf(dopts->output, PROPS_END);
		fprintf(dopts->output, "\n");
	}

	/* The first revision must contain the user prefix */
	if (local_revnum == (svn_revnum_t)1 && dopts->user_prefix != NULL) {
		dump_create_user_prefix();
	}

	/* We need a hash to check what elements have already been dumped (if copies occur) */
	if (whash_create()) {
		fprintf(stderr, "Error allocating memory.\n");
		return 1;
	}

	/* Fetch changed paths of this revision */
	failed = 0;
	nodes = list_create(sizeof(node_t));
	wsvn_get_changeset(entry, &nodes);

	/* Stat nodes (this is needed for proper sorting) */
	for (i = 0; i < nodes.size; i++) {
		node_t *n = (node_t *)nodes.elements + i;
		if (dopts->online) {
			if (n->action != NA_DELETE && wsvn_stat(n, entry->revision)) {
				failed = i;
				break;
			}
		} else if (n->action != NA_DELETE) {
			struct stat st;
			char *path = malloc(strlen(dopts->repo_dir)+strlen(n->path)+2);
			sprintf(path, "%s/%s", dopts->repo_dir, n->path);
			if (stat(path, &st) == 0) {
				if (st.st_mode & S_IFDIR) {
					n->kind = NK_DIRECTORY;
					n->size = 0;
				} else {
					n->kind = NK_FILE;
					n->size = st.st_size;
				}
			} else {
				failed = i;
				free(path);
				break;
			}
			free(path);
		}
		whash_insert(n->path);
		if (dopts->verbosity > 0) {
			fprintf(stderr, "\033[1A\033[K");
			if (dopts->keep_revnums) {
				fprintf(stderr, "* Dumping revision %ld ... %d%%\n", entry->revision, (i*50)/nodes.size);
			} else {
				fprintf(stderr, "* Dumping revision %ld (local: %ld) ... %d%%\n", entry->revision, local_revnum, (i*50)/nodes.size);
			}
		}
	}

	if (failed == 0) {
		failed = nodes.size; 

		/* Sort nodes to get a dump order (mainly for directories 
		   being dumped prior to their contents) */
		list_qsort(&nodes, nodecmp);

		/* Dump and free nodes */
		for (i = 0; i < nodes.size; i++) {
			node_t *n = (node_t *)nodes.elements + i;
			if (node_dump(n, dopts, logs, entry->revision, local_revnum)) {
				failed = i;
				break;
			}
			node_free(n);
			if (dopts->verbosity > 0) {
				fprintf(stderr, "\033[1A\033[K");
				if (dopts->keep_revnums) {
					fprintf(stderr, "* Dumping revision %ld ... %d%%\n", entry->revision, 50+(i*50)/nodes.size);
				} else {
					fprintf(stderr, "* Dumping revision %ld (local: %ld) ... %d%%\n", entry->revision, local_revnum, 50+(i*50)/nodes.size);
				}
			}
		}
	} else {
		failed = 0;
	}

	/* Free nodes that have not been dumped */
	for (i = failed; i < nodes.size; i++) {
		node_t *n = (node_t *)nodes.elements + i;
		node_free(n);
	}

	whash_free();

	if (failed != nodes.size)  {
		list_free(&nodes);
		return 1;
	}

	list_free(&nodes);
	
	if (dopts->verbosity >= 0) {
		if (dopts->verbosity > 0) {
			fprintf(stderr, "\033[1A\033[K");
		}
		if (dopts->keep_revnums) {
			fprintf(stderr, "* Dumped revision %ld.\n", entry->revision);
		} else {
			fprintf(stderr, "* Dumped revision %ld (local %ld).\n", entry->revision, local_revnum);
		}
	}

#ifdef USE_TIMING
	fprintf(stderr, "[[ Revision dumped in %.3f seconds ]]\n", (float)stopwatch_elapsed(&watch));
#endif
	return 0;
}


/* Dumps empty revisions between entry1 and entry2 for padding */
static void dump_pad_revisions(logentry_t *entry1, logentry_t *entry2)
{
	svn_revnum_t i;
	property_t msg;
	int props_length = PROPS_END_LEN;

	/* The message is the same as in svndumpfilter */
	msg.key = "svn:log";
	msg.value = "This is an empty revision for padding.";

	props_length += property_strlen(&msg);

	for (i = entry1->revision+1; i < entry2->revision; i++) {
		fprintf(dopts->output, "%s: %ld\n", SVN_REPOS_DUMPFILE_REVISION_NUMBER, i);
		fprintf(dopts->output, "%s: %d\n", SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH, props_length);
		fprintf(dopts->output, "%s: %d\n\n", SVN_REPOS_DUMPFILE_CONTENT_LENGTH, props_length);

		property_dump(&msg, dopts->output);
		fprintf(dopts->output, PROPS_END);
		fprintf(dopts->output, "\n");

		if (dopts->verbosity > 0) {
			fprintf(stderr, "* Padded revision %ld.\n", i);
		}
	}
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/

/* Creats default dumping options */
dump_options_t dump_options_create()
{
	dump_options_t opts;
	opts.verbosity = 0;
	opts.online = 0; 
	opts.keep_revnums = 0;
	opts.dump_uuid = 0;
	opts.repo_url = NULL;
	opts.repo_eurl = NULL;
	opts.repo_base = NULL;
	opts.repo_uuid = NULL;
	opts.repo_dir = NULL;
	opts.repo_prefix = NULL;
	opts.username = NULL;
	opts.password = NULL;
	opts.user_prefix = NULL;
	opts.prefix_is_file = 0;
	opts.output = stdout;
	opts.startrev = 0;
	opts.endrev = HEAD_REVISION;
#ifdef USE_DELTAS
	opts.deltas = 0;
#endif /* USE_DELTAS */
	return opts;
}


/* Destroys dumping options, freeing char-pointers if they are not NULL */
void dump_options_free(dump_options_t *opts)
{
	if (opts->repo_url) {
		free(opts->repo_url);
	}
	if (opts->repo_eurl) {
		free(opts->repo_eurl);
	}
	if (opts->repo_base) {
		free(opts->repo_base);
	}
	if (opts->repo_uuid) {
		free(opts->repo_uuid);
	}
	if (opts->repo_dir) {
		free(opts->repo_dir);
	}
	if (opts->repo_prefix) {
		free(opts->repo_prefix);
	}
	if (opts->username) {
		free(opts->username);
	}
	if (opts->password) {
		free(opts->password);
	}
	if (opts->user_prefix) {
		free(opts->user_prefix);
	}
	if (opts->output && opts->output != stdout) {
		fclose(opts->output);
	}
}


/* Dumps the complete history of the given url in opts, returning 0 on success */
char dump(dump_options_t *opts)
{
	list_t log;
	logentry_t current = logentry_create();
	logentry_t next = logentry_create();
	node_t node;
	svn_revnum_t i, headrev;
	int off;
#ifdef USE_TIMING
	stopwatch_t watch = stopwatch_create();
#endif

	/* Initialize and open the svn session */
	if (wsvn_init(opts)) {
		fprintf(stderr, "Error load subversion library / initializing connection.\n");
		logentry_free(&current);
		logentry_free(&next);
		return 1;
	}

	/* Fetch repository information */
	if (wsvn_repo_info(opts->repo_eurl, &opts->repo_base, &opts->repo_prefix, &opts->repo_uuid, &headrev)) {
		fprintf(stderr, "Error fetching repository information.\n");
		logentry_free(&current);
		logentry_free(&next);
		return 1;
	}
	if (opts->endrev > headrev || opts->endrev == HEAD_REVISION) {
		opts->endrev = headrev;
	}

	/* Check if --dump-uuid can be used (if it is given) */
	if (opts->dump_uuid) {
		if (strlen(opts->repo_prefix) || opts->user_prefix != NULL) {
			fprintf(stderr, "Sorry, '--dump-uuid' can only be used when dumping a repository root without any user prefix\n");
			logentry_free(&current);
			logentry_free(&next);
			return 1;
		}
	}

	/* Determine node kind of dumped root */
	node = node_create();
	node.path = ".";
	wsvn_stat(&node, SVN_INVALID_REVNUM);
	if (node.kind == NK_FILE) {
		opts->prefix_is_file = 1;
		if (opts->online == 0) {
			if (opts->verbosity >= 0) {
				fprintf(stderr, "Switched to online mode because the url refers to a file.\n");
			}
			opts->online = 1;
		}
	}

	/* Check if the url is a file:// url and switch mode if neccessary */
	if (!strncmp(opts->repo_eurl, "file://", 7) && opts->online == 0) {
		if (opts->verbosity >= 0) {
			fprintf(stderr, "Switched to online mode for performance reasons.\n");
		}
		opts->online = 1;
	}

	/* Write dumpfile header */
	fprintf(opts->output, "%s: 2\n", SVN_REPOS_DUMPFILE_MAGIC_HEADER);
	fprintf(opts->output, "\n");
	if (opts->dump_uuid) {
		fprintf(opts->output, "UUID: %s\n", opts->repo_uuid);
		fprintf(opts->output, "\n");
	}

	/* Fetch first history item */
	log = list_create(sizeof(logentry_t));
	if (wsvn_next_log(NULL, &next)) {
		fprintf(stderr, "Error fetching repository log info.\n");
		list_free(&log);
		logentry_free(&current);
		logentry_free(&next);
		return 1;
	}

	/* Write initial revision header if not starting at revision 0 */
	if (next.revision != 0) {
		fprintf(opts->output, "%s: %d\n", SVN_REPOS_DUMPFILE_REVISION_NUMBER, 0);
		fprintf(opts->output, "%s: %d\n", SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH, PROPS_END_LEN);
		fprintf(opts->output, "%s: %d\n\n", SVN_REPOS_DUMPFILE_CONTENT_LENGTH, PROPS_END_LEN);
		fprintf(opts->output, PROPS_END);
		fprintf(opts->output, "\n");
	}

	/* Dump all revisions */
	dopts = opts;
	logs = &log;
	off = (next.revision == 0) ? 0 : 1;
	current.revision = 0;	/* Start padding at 0 */
	i = 0;
	do {
		list_append(&log, &next);
		if (opts->keep_revnums) {
			dump_pad_revisions(&current, &next);
		}
		current = next;
		if (dump_revision(&current, i+off)) {
			break;
		}
		++i;
		/* This frees all log entry strings */
		logentry_free(&next);
	/* Fetch next log entry */
	} while (current.revision < opts->endrev &&
	         wsvn_next_log(&current, &next) == 0 &&
	         current.revision != next.revision);

	logentry_free(&next);
	list_free(&log);

	wsvn_free();

#ifdef USE_TIMING
	fprintf(stderr, "[ Dumping done in %.3f seconds ]]\n", (float)stopwatch_elapsed(&watch));
#endif
	return 0;
}
