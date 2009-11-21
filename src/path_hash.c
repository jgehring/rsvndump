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
 *      file: path_hash.c
 *      desc: A global hash that stores paths of specific revisions
 *
 *      The path_hash is used to track the repository layout at all
 *      previous revisions. This is useful when handling directory
 *      copies and deciding which nodes will actually be copied.
 *
 *      The current implementation stores tree deltas instead of full
 *      layouts for each revision to minimize memory usage. Therefore,
 *
 */


#include <stdio.h>

#include <svn_path.h>

#include <apr_hash.h>
#include <apr_tables.h>

#include "main.h"

#include "path_hash.h"


/*---------------------------------------------------------------------------*/
/* Local data structures                                                     */
/*---------------------------------------------------------------------------*/

/* A tree delta structure */
typedef struct {
	/*
	 * For nodes that are being added, a hash tree is used for every
	 * part of the path. For deletions, there's a simple array of strings.
	 */
	apr_hash_t *added;
	apr_array_header_t *deleted;
} tree_delta_t;



/*---------------------------------------------------------------------------*/
/* Static variables                                                          */
/*---------------------------------------------------------------------------*/

/* Global pool */
static apr_pool_t *ph_pool = NULL;

/* Array storing all deltas */
static apr_array_header_t *ph_revisions = NULL;

/* Additional session prefix (for copyfrom_path information) */
static const char *ph_session_prefix = "";

/* The current (head) delta */
static tree_delta_t *ph_head = NULL;


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/


/* Adds a path to a tree */
static apr_hash_t *path_hash_add(apr_hash_t *tree, const char *path, apr_pool_t *pool)
{
	const char *parent, *child;
	apr_hash_t *parent_tree, *child_tree;

	/* Determine tree of parent node via recursion */
	svn_path_split(path, &parent, &child, pool);
	if (!strcmp(parent, child)) {
		return tree;
	}
	parent_tree = path_hash_add(tree, parent, pool);

	/* Add the node to the parent tree if neede */
	child_tree = apr_hash_get(parent_tree, child, APR_HASH_KEY_STRING);
	if (child_tree == NULL) {
		child_tree = apr_hash_make(ph_pool);
		apr_hash_set(parent_tree, apr_pstrdup(ph_pool, child), APR_HASH_KEY_STRING, child_tree);
	}
	DEBUG_MSG("/%s", child);
	return child_tree;
}


/* Adds a path to a tree, allocating everything in pool */
static apr_hash_t *path_hash_add_local(apr_hash_t *tree, const char *path, apr_pool_t *pool)
{
	const char *parent, *child;
	apr_hash_t *parent_tree, *child_tree;

	/* Determine tree of parent node via recursion */
	svn_path_split(path, &parent, &child, pool);
	if (!strcmp(parent, child)) {
		return tree;
	}
	parent_tree = path_hash_add_local(tree, parent, pool);

	/* Add the node to the parent tree if neede */
	child_tree = apr_hash_get(parent_tree, child, APR_HASH_KEY_STRING);
	if (child_tree == NULL) {
		child_tree = apr_hash_make(pool);
		apr_hash_set(parent_tree, apr_pstrdup(pool, child), APR_HASH_KEY_STRING, child_tree);
	}
	return child_tree;
}


/* Returns the given subtree for path or NULL if path is not present in the tree */
static apr_hash_t *path_hash_subtree(apr_hash_t *tree, const char *path, apr_pool_t *pool)
{
	const char *parent, *child;
	apr_hash_t *parent_tree;

	/* Determine tree of parent node via recursion */
	svn_path_split(path, &parent, &child, pool);
	if (!strcmp(parent, child)) {
		return tree;
	}
	parent_tree = path_hash_subtree(tree, parent, pool);
	if (parent_tree == NULL) {
		return NULL;
	}

	return apr_hash_get(parent_tree, child, APR_HASH_KEY_STRING);
}


/* Deletes a path (and thus, all its children) from a tree. This doesn't free
 * the memory, though */
static void path_hash_delete(apr_hash_t *tree, const char *path, apr_pool_t *pool)
{
	const char *parent, *child;
	apr_hash_t *parent_tree;

	/* Determine tree of parent node subtree method */
	svn_path_split(path, &parent, &child, pool);
	if (!strcmp(parent, child)) {
		return;
	}
	parent_tree = path_hash_subtree(tree, parent, pool);

	/* Remove the child */
	if (parent_tree) {
		apr_hash_set(parent_tree, child, APR_HASH_KEY_STRING, NULL);
	}
}


/* Reconstructs the complete tree at the given revision, allocated in pool */
static apr_hash_t *path_hash_reconstruct(svn_revnum_t rev, apr_pool_t *pool)
{
	apr_hash_t *tree;
	apr_array_header_t *stack, *path;
	apr_hash_index_t *hi;
	int i, j;

	if (ph_revisions->nelts < rev) {
		DEBUG_MSG("path_hash_reconstruct(%ld): revision not available\n");
		return NULL;
	}

	tree = apr_hash_make(pool);
	stack = apr_array_make(pool, 0, sizeof(apr_hash_t *));
	path = apr_array_make(pool, 0, sizeof(const char *));

	for (i = 0; i <= rev; i++) {
		tree_delta_t *delta = APR_ARRAY_IDX(ph_revisions, i, tree_delta_t *);

		/* Skip padding revisions */
		if (delta == NULL) {
			continue;
		}

		/* Apply delta: deletions */
		for (j = 0; j < delta->deleted->nelts; j++) {
			path_hash_delete(tree, APR_ARRAY_IDX(delta->deleted, j, const char *), pool);
		}

		/* Apply delta: additions */
		APR_ARRAY_PUSH(stack, apr_hash_t *) = delta->added;
		APR_ARRAY_PUSH(path, const char *) = apr_pstrdup(pool, "/");
		while (stack->nelts > 0) {
			apr_hash_t *top_hash = *((apr_hash_t **)apr_array_pop(stack));
			const char *top_path = *((const char **)apr_array_pop(path));

			/*
			 * Push child nodes to the stack. If there aren't any
			 * left, add the path to the tree. The parents will be
			 * added automatically
			 */
			if (apr_hash_count(top_hash) == 0) {
				path_hash_add_local(tree, top_path, pool);
				continue;
			}

			for (hi = apr_hash_first(pool, top_hash); hi; hi = apr_hash_next(hi)) {
				const char *key;
				apr_hash_t *value;
				apr_hash_this(hi, (const void **)(void *)&key, NULL, (void **)(void *)&value);

				APR_ARRAY_PUSH(stack, apr_hash_t *) = value;
				APR_ARRAY_PUSH(path, const char *) = svn_path_join(top_path, key, pool);
			}
		}
	}

	return tree;
}


/* Recursively copies the contents of one hash to another */
static void path_hash_copy_deep(apr_hash_t *dest, apr_hash_t *source, apr_pool_t *pool)
{
	apr_hash_index_t *hi;

	for (hi = apr_hash_first(pool, source); hi; hi = apr_hash_next(hi)) {
		const char *key;
		apr_hash_t *child_source, *child_dest;
		apr_hash_this(hi, (const void **)(void *)&key, NULL, (void **)(void *)&child_source);

		/* Create a new child hash under the same name and copy its contents */
		child_dest = apr_hash_make(ph_pool);
		path_hash_copy_deep(child_dest, child_source, pool);

		apr_hash_set(dest, apr_pstrdup(ph_pool, key), APR_HASH_KEY_STRING, child_dest);
	}
}


/* Copies a path from a specific revision (recursively) */
static void path_hash_copy(apr_hash_t *tree, const char *path, const char *from, svn_revnum_t revnum, apr_pool_t *pool)
{
	apr_hash_t *recon, *subtree, *newtree;
	apr_pool_t *recon_pool = svn_pool_create(pool);

	/* First, reconstruct tree at the given revision */
	recon = path_hash_reconstruct(revnum, recon_pool);
	if (recon == NULL) {
		DEBUG_MSG("path_hash: !!! reconstruction failed for %ld!\n", revnum);
		return;
	}

	/* Determine location of the path */
	subtree = path_hash_subtree(recon, from, recon_pool);
	if (subtree == NULL) {
		DEBUG_MSG("path_hash: !!! no subtree for %s@%ld\n", from, revnum);
		return;
	}

	/* Add the complete subtree */
	DEBUG_MSG("path_hash:     +++ ");
	newtree = path_hash_add(tree, path, pool);
	DEBUG_MSG("\n");
	path_hash_copy_deep(newtree, subtree, pool);

	svn_pool_destroy(recon_pool);
}


/* Mainly used for debugging */
static void path_hash_dump(apr_hash_t *tree, apr_pool_t *pool)
{
	apr_hash_index_t *hi;
	apr_array_header_t *recon_stack = apr_array_make(pool, 0, sizeof(apr_hash_t *));
	apr_array_header_t *recon_path = apr_array_make(pool, 0, sizeof(const char *));

	DEBUG_MSG("path_hash: ----------------------------------------------\n");
	APR_ARRAY_PUSH(recon_stack, apr_hash_t *) = tree;
	APR_ARRAY_PUSH(recon_path, const char *) = "/";
	while (recon_stack->nelts > 0) {
		apr_hash_t *top_hash = *(apr_hash_t **)apr_array_pop(recon_stack);
		const char *top_path = *(const char **)apr_array_pop(recon_path);

		if (apr_hash_count(top_hash) == 0) {
			DEBUG_MSG("path_hash: %s\n", top_path);
			continue;
		}

		for (hi = apr_hash_first(pool, top_hash); hi; hi = apr_hash_next(hi)) {
			const char *key;
			apr_hash_t *value;
			apr_hash_this(hi, (const void **)(void *)&key, NULL, (void **)(void *)&value);

			APR_ARRAY_PUSH(recon_stack, apr_hash_t *) = value;
			APR_ARRAY_PUSH(recon_path, const char *) = svn_path_join(top_path, key, pool);
		}
	}
	DEBUG_MSG("path_hash: ----------------------------------------------\n");
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/


/* Initializes the path hash using the given pool */
void path_hash_initialize(const char *session_prefix, apr_pool_t *parent_pool)
{
	if (ph_pool == NULL) {
		ph_pool = svn_pool_create(parent_pool);
		ph_revisions = apr_array_make(ph_pool, 0, sizeof(tree_delta_t *));
		ph_session_prefix = apr_pstrdup(ph_pool, session_prefix);
	}
}


/* Manually adds a new path to the head revision (without committing it) */
void path_hash_add_path(const char *path)
{
	apr_pool_t *pool = svn_pool_create(ph_pool);

	if (ph_head == NULL) {
		ph_head = apr_palloc(ph_pool, sizeof(tree_delta_t));
		ph_head->added = apr_hash_make(ph_pool);
		ph_head->deleted = apr_array_make(ph_pool, 0, sizeof(const char *));
	}

	DEBUG_MSG("path_hash: M++ ");
	path_hash_add(ph_head->added, path, pool);
	DEBUG_MSG("\n");

	svn_pool_destroy(pool);
}


/* Adds a new revision to the path hash */
void path_hash_commit(log_revision_t *log, svn_revnum_t revnum)
{
	apr_hash_index_t *hi;
	apr_pool_t *pool = svn_pool_create(ph_pool);

	if (ph_revisions->nelts > revnum) {
		return;
	}

	if (ph_head == NULL) {
		ph_head = apr_palloc(ph_pool, sizeof(tree_delta_t));
		ph_head->added = apr_hash_make(ph_pool);
		ph_head->deleted = apr_array_make(ph_pool, 0, sizeof(const char *));
	}

	/* Iterate over the changed paths of this revision and store them
	 * in the current tree delta */
	for (hi = apr_hash_first(pool, log->changed_paths); hi; hi = apr_hash_next(hi)) {
		const char *path;
		svn_log_changed_path_t *info;
		apr_hash_this(hi, (const void **)(void *)&path, NULL, (void **)(void *)&info);

		if (info->action == 'A') {
			if (info->copyfrom_path) {
				int offset = strlen(ph_session_prefix);
				while (*(info->copyfrom_path+offset) == '/') {
					++offset;
				}

				DEBUG_MSG("path_hash: +++ %s@%d -> %s [prefix = %s]\n", info->copyfrom_path, info->copyfrom_rev, path, ph_session_prefix);
				path_hash_copy(ph_head->added, path, info->copyfrom_path + offset, info->copyfrom_rev, pool);
			} else {
				DEBUG_MSG("path_hash: +++ ");
				path_hash_add(ph_head->added, path, pool);
				DEBUG_MSG("\n");
			}
		} else if (info->action == 'D') {
			DEBUG_MSG("path_hash: --- %s\n", path);
			APR_ARRAY_PUSH(ph_head->deleted, const char *) = apr_pstrdup(ph_pool, path);
		}
	}

	/* Finally, add the new revision after possible padding */
	while (ph_revisions->nelts < revnum) {
		APR_ARRAY_PUSH(ph_revisions, tree_delta_t *) = NULL;
	}
	APR_ARRAY_PUSH(ph_revisions, tree_delta_t *) = ph_head;
	ph_head = NULL;

#if 0 && defined(DEBUG)
	DEBUG_MSG("path_hash: for revision %ld:\n", revnum);
	path_hash_dump(path_hash_reconstruct(revnum, pool), pool);
#endif

	svn_pool_destroy(pool);
}


/* Checks the parent relation of two paths at a given revision */
char path_hash_check_parent(const char *parent, const char *child, svn_revnum_t revnum, apr_pool_t *pool)
{
	apr_hash_t *recon, *subtree;

	/* First, reconstruct tree at the given revision */
	recon = path_hash_reconstruct(revnum, pool);
	if (recon == NULL) {
		DEBUG_MSG("path_hash: unable to reconstruct hash for revision %ld\n", revnum);
		return 0;
	}

	/* Find the subtree of the parent node */
	subtree = path_hash_subtree(recon, parent, pool);
	if (subtree == NULL) {
		DEBUG_MSG("path_hash: subtree for %s in %ld not found:\n", parent, revnum);
#if 0 && defined(DEBUG)
		path_hash_dump(recon, pool);
#endif
	}

	/* Find the subtree of the child */
	return (subtree && path_hash_subtree(subtree, child, pool) != NULL);
}
