/*
 * queue.c
 * compilation of a list of packages into a world dependency set
 *
 * Copyright (c) 2012 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>

/*
 * !doc
 *
 * libpkgconf `queue` module
 * =========================
 *
 * The `queue` module provides an interface that allows easily building a dependency graph from an
 * arbitrary set of dependencies.  It also provides support for doing "preflight" checks on the entire
 * dependency graph prior to working with it.
 *
 * Using the `queue` module functions is the recommended way of working with dependency graphs.
 */

/*
 * !doc
 *
 * .. c:function:: void pkgconf_queue_push_dependency(pkgconf_list_t *list, const pkgconf_dependency_t *dep)
 *
 *    Pushes a requested dependency onto the dependency resolver's queue which is described by
 *    a pkgconf_dependency_t node.
 *
 *    :param pkgconf_list_t* list: the dependency resolution queue to add the package request to.
 *    :param pkgconf_dependency_t* dep: the dependency requested
 *    :return: nothing
 */
void
pkgconf_queue_push_dependency(pkgconf_list_t *list, const pkgconf_dependency_t *dep)
{
	pkgconf_buffer_t depbuf = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_queue_t *pkgq = calloc(1, sizeof(pkgconf_queue_t));

	pkgconf_buffer_append(&depbuf, dep->package);
	if (dep->version != NULL)
		pkgconf_buffer_append_fmt(&depbuf, " %s %s", pkgconf_pkg_get_comparator(dep), dep->version);

	pkgq->package = pkgconf_buffer_freeze(&depbuf);
	pkgconf_node_insert_tail(&pkgq->iter, pkgq, list);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_queue_push(pkgconf_list_t *list, const char *package)
 *
 *    Pushes a requested dependency onto the dependency resolver's queue.
 *
 *    :param pkgconf_list_t* list: the dependency resolution queue to add the package request to.
 *    :param char* package: the dependency atom requested
 *    :return: nothing
 */
void
pkgconf_queue_push(pkgconf_list_t *list, const char *package)
{
	pkgconf_queue_t *pkgq = calloc(1, sizeof(pkgconf_queue_t));

	pkgq->package = strdup(package);
	pkgconf_node_insert_tail(&pkgq->iter, pkgq, list);
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_queue_compile(pkgconf_client_t *client, pkgconf_pkg_t *world, pkgconf_list_t *list)
 *
 *    Compile a dependency resolution queue into a dependency resolution problem if possible, otherwise report an error.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
 *    :param pkgconf_pkg_t* world: The designated root of the dependency graph.
 *    :param pkgconf_list_t* list: The list of dependency requests to consider.
 *    :return: true if the built dependency resolution problem is consistent, else false
 *    :rtype: bool
 */
bool
pkgconf_queue_compile(pkgconf_client_t *client, pkgconf_pkg_t *world, pkgconf_list_t *list)
{
	pkgconf_node_t *iter;

	PKGCONF_FOREACH_LIST_ENTRY(list->head, iter)
	{
		pkgconf_queue_t *pkgq;

		pkgq = iter->data;
		pkgconf_dependency_parse(client, world, &world->required, pkgq->package, PKGCONF_PKG_DEPF_QUERY);
	}

	return (world->required.head != NULL);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_queue_free(pkgconf_list_t *list)
 *
 *    Release any memory related to a dependency resolution queue.
 *
 *    :param pkgconf_list_t* list: The dependency resolution queue to release.
 *    :return: nothing
 */
void
pkgconf_queue_free(pkgconf_list_t *list)
{
	pkgconf_node_t *node, *tnode;

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(list->head, tnode, node)
	{
		pkgconf_queue_t *pkgq = node->data;

		free(pkgq->package);
		free(pkgq);
	}
}

static void
pkgconf_queue_mark_public(pkgconf_client_t *client, pkgconf_pkg_t *pkg, void *data)
{
	if (pkg->flags & PKGCONF_PKG_PROPF_VISITED_PRIVATE)
	{
		pkgconf_list_t *list = data;
		pkgconf_node_t *node;

		PKGCONF_FOREACH_LIST_ENTRY(list->head, node)
		{
			pkgconf_dependency_t *dep = node->data;
			if (dep->match == pkg)
				dep->flags &= ~PKGCONF_PKG_DEPF_PRIVATE;
		}

		pkg->flags &= ~PKGCONF_PKG_PROPF_VISITED_PRIVATE;

		PKGCONF_TRACE(client, "%s: updated, public", pkg->id);
	}
}

static unsigned int
pkgconf_queue_collect_dependencies_main(pkgconf_client_t *client,
	pkgconf_pkg_t *root,
	void *data,
	int maxdepth);

static inline unsigned int
pkgconf_queue_collect_dependencies_walk(pkgconf_client_t *client,
	pkgconf_list_t *deplist,
	void *data,
	int depth)
{
	unsigned int eflags = PKGCONF_PKG_ERRF_OK;
	pkgconf_node_t *node;
	pkgconf_pkg_t *world = data;

	PKGCONF_FOREACH_LIST_ENTRY_REVERSE(deplist->tail, node)
	{
		pkgconf_dependency_t *dep = node->data;
		pkgconf_dependency_t *flattened_dep;
		pkgconf_pkg_t *pkg = dep->match;

		if (*dep->package == '\0')
			continue;

		if (pkg == NULL)
		{
			PKGCONF_TRACE(client, "WTF: unmatched dependency %p <%s>", dep, dep->package);
			continue;
		}

		if (pkg->serial == client->serial)
			continue;

		if (client->flags & PKGCONF_PKG_PKGF_ITER_PKG_IS_PRIVATE)
			pkg->flags |= PKGCONF_PKG_PROPF_VISITED_PRIVATE;
		else
			pkg->flags &= ~PKGCONF_PKG_PROPF_VISITED_PRIVATE;

		eflags |= pkgconf_queue_collect_dependencies_main(client, pkg, data, depth - 1);

		flattened_dep = pkgconf_dependency_copy(client, dep);
		pkgconf_node_insert(&flattened_dep->iter, flattened_dep, &world->required);
	}

	return eflags;
}

static unsigned int
pkgconf_queue_collect_dependencies_main(pkgconf_client_t *client,
	pkgconf_pkg_t *root,
	void *data,
	int maxdepth)
{
	unsigned int eflags = PKGCONF_PKG_ERRF_OK;

	if (maxdepth == 0)
		return eflags;

	/* Short-circuit if we have already visited this node.
	 */
	if (root->serial == client->serial)
		return eflags;

	root->serial = client->serial;

	PKGCONF_TRACE(client, "%s: collecting private dependencies, level %d", root->id, maxdepth);

	/* XXX: ugly */
	const unsigned int saved_flags = client->flags;
	client->flags |= PKGCONF_PKG_PKGF_ITER_PKG_IS_PRIVATE;
	eflags = pkgconf_queue_collect_dependencies_walk(client, &root->requires_private, data, maxdepth);
	client->flags = saved_flags;
	if (eflags != PKGCONF_PKG_ERRF_OK)
		return eflags;

	PKGCONF_TRACE(client, "%s: collecting public dependencies, level %d", root->id, maxdepth);

	eflags = pkgconf_queue_collect_dependencies_walk(client, &root->required, data, maxdepth);
	if (eflags != PKGCONF_PKG_ERRF_OK)
		return eflags;

	PKGCONF_TRACE(client, "%s: finished, %s", root->id, (root->flags & PKGCONF_PKG_PROPF_VISITED_PRIVATE) ? "private" : "public");

	return eflags;
}

static inline unsigned int
pkgconf_queue_collect_dependencies(pkgconf_client_t *client,
	pkgconf_pkg_t *root,
	void *data,
	int maxdepth)
{
	++client->serial;
	return pkgconf_queue_collect_dependencies_main(client, root, data, maxdepth);
}

static inline unsigned int
pkgconf_queue_collect_conflicts(pkgconf_client_t *client,
	pkgconf_pkg_t *root,
	pkgconf_pkg_t *world,
	int maxdepth)
{
	unsigned int eflags = PKGCONF_PKG_ERRF_OK;
	pkgconf_node_t *node;

	PKGCONF_TRACE(client, "%s: collecting conflicts, level %d", root->id, maxdepth);

	PKGCONF_FOREACH_LIST_ENTRY(root->required.head, node)
	{
		pkgconf_dependency_t *dep = node->data;
		pkgconf_pkg_t *pkg = dep->match;
		pkgconf_node_t *cnode;

		if (*dep->package == '\0')
			continue;

		if (pkg == NULL)
		{
			PKGCONF_TRACE(client, "WTF: unmatched dependency %p <%s>", dep, dep->package);
			continue;
		}

		PKGCONF_FOREACH_LIST_ENTRY(pkg->conflicts.head, cnode)
		{
			pkgconf_dependency_t *conflict = cnode->data;
			pkgconf_dependency_t *flattened_conflict = pkgconf_dependency_copy(client, conflict);

			flattened_conflict->why = strdup(pkg->id);
			pkgconf_node_insert(&flattened_conflict->iter, flattened_conflict, &world->conflicts);
		}
	}

	return eflags;
}

static inline unsigned int
pkgconf_queue_verify(pkgconf_client_t *client, pkgconf_pkg_t *world, pkgconf_list_t *list, int maxdepth)
{
	unsigned int result;
	const unsigned int saved_flags = client->flags;
	pkgconf_pkg_t initial_world = {
		.id = "user:request",
		.realname = "virtual world package",
		.flags = PKGCONF_PKG_PROPF_STATIC | PKGCONF_PKG_PROPF_VIRTUAL,
	};

	if (!pkgconf_queue_compile(client, &initial_world, list))
	{
		pkgconf_solution_free(client, &initial_world);
		return PKGCONF_PKG_ERRF_DEPGRAPH_BREAK;
	}

	PKGCONF_TRACE(client, "solving");
	result = pkgconf_pkg_traverse(client, &initial_world, NULL, NULL, maxdepth, 0);
	if (result != PKGCONF_PKG_ERRF_OK)
	{
		pkgconf_solution_free(client, &initial_world);
		return result;
	}

	PKGCONF_TRACE(client, "flattening");
	result = pkgconf_queue_collect_dependencies(client, &initial_world, world, maxdepth);
	if (result != PKGCONF_PKG_ERRF_OK)
	{
		pkgconf_solution_free(client, &initial_world);
		return result;
	}

	result = pkgconf_queue_collect_conflicts(client, world, world, maxdepth);
	if (result != PKGCONF_PKG_ERRF_OK)
	{
		pkgconf_solution_free(client, &initial_world);
		return result;
	}

	if (client->flags & PKGCONF_PKG_PKGF_SEARCH_PRIVATE)
	{
		PKGCONF_TRACE(client, "marking public deps");
		client->flags &= ~PKGCONF_PKG_PKGF_SEARCH_PRIVATE;
		client->flags |= PKGCONF_PKG_PKGF_SKIP_CONFLICTS;
		result = pkgconf_pkg_traverse(client, &initial_world, pkgconf_queue_mark_public, &world->required, maxdepth, 0);
		client->flags = saved_flags;
		if (result != PKGCONF_PKG_ERRF_OK)
		{
			pkgconf_solution_free(client, &initial_world);
			return result;
		}
	}

	if (!(client->flags & PKGCONF_PKG_PKGF_SKIP_CONFLICTS))
	{
		PKGCONF_TRACE(client, "checking for conflicts");

		result = pkgconf_pkg_walk_conflicts_list(client, world, &world->conflicts);
		if (result != PKGCONF_PKG_ERRF_OK)
		{
			pkgconf_solution_free(client, &initial_world);
			return result;
		}
	}

	/* free the initial solution */
	pkgconf_solution_free(client, &initial_world);

	return PKGCONF_PKG_ERRF_OK;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_solution_free(pkgconf_client_t *client, pkgconf_pkg_t *world, int maxdepth)
 *
 *    Removes references to package nodes contained in a solution.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
 *    :param pkgconf_pkg_t* world: The root for the generated dependency graph.  Should have PKGCONF_PKG_PROPF_VIRTUAL flag.
 *    :returns: nothing
 */
void
pkgconf_solution_free(pkgconf_client_t *client, pkgconf_pkg_t *world)
{
	(void) client;

	if (world->flags & PKGCONF_PKG_PROPF_VIRTUAL)
	{
		pkgconf_dependency_free(&world->required);
		pkgconf_dependency_free(&world->requires_private);
		pkgconf_dependency_free(&world->conflicts);
	}
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_queue_solve(pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_pkg_t *world, int maxdepth)
 *
 *    Solves and flattens the dependency graph for the supplied dependency list.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
 *    :param pkgconf_list_t* list: The list of dependency requests to consider.
 *    :param pkgconf_pkg_t* world: The root for the generated dependency graph, provided by the caller.  Should have PKGCONF_PKG_PROPF_VIRTUAL flag.
 *    :param int maxdepth: The maximum allowed depth for the dependency resolver.  A depth of -1 means unlimited.
 *    :returns: true if the dependency resolver found a solution, otherwise false.
 *    :rtype: bool
 */
bool
pkgconf_queue_solve(pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_pkg_t *world, int maxdepth)
{
	/* if maxdepth is one, then we will not traverse deeper than our virtual package. */
	if (!maxdepth)
		maxdepth = -1;

	unsigned int flags = client->flags;
	client->flags |= PKGCONF_PKG_PKGF_SEARCH_PRIVATE;

	unsigned int ret = pkgconf_queue_verify(client, world, list, maxdepth);
	client->flags = flags;

	return ret == PKGCONF_PKG_ERRF_OK;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_queue_apply(pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_queue_apply_func_t func, int maxdepth, void *data)
 *
 *    Attempt to compile a dependency resolution queue into a dependency resolution problem, then attempt to solve the problem and
 *    feed the solution to a callback function if a complete dependency graph is found.
 *
 *    This function should not be used in new code.  Use pkgconf_queue_solve instead.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
 *    :param pkgconf_list_t* list: The list of dependency requests to consider.
 *    :param pkgconf_queue_apply_func_t func: The callback function to call if a solution is found by the dependency resolver.
 *    :param int maxdepth: The maximum allowed depth for the dependency resolver.  A depth of -1 means unlimited.
 *    :param void* data: An opaque pointer which is passed to the callback function.
 *    :returns: true if the dependency resolver found a solution, otherwise false.
 *    :rtype: bool
 */
bool
pkgconf_queue_apply(pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_queue_apply_func_t func, int maxdepth, void *data)
{
	bool ret = false;
	pkgconf_pkg_t world = {
		.id = "virtual:world",
		.realname = "virtual world package",
		.flags = PKGCONF_PKG_PROPF_STATIC | PKGCONF_PKG_PROPF_VIRTUAL,
	};

	/* if maxdepth is one, then we will not traverse deeper than our virtual package. */
	if (!maxdepth)
		maxdepth = -1;

	if (!pkgconf_queue_solve(client, list, &world, maxdepth))
		goto cleanup;

	/* the world dependency set is flattened after it is returned from pkgconf_queue_verify */
	if (!func(client, &world, data, maxdepth))
		goto cleanup;

	ret = true;

cleanup:
	pkgconf_pkg_free(client, &world);
	return ret;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_queue_validate(pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_queue_apply_func_t func, int maxdepth, void *data)
 *
 *    Attempt to compile a dependency resolution queue into a dependency resolution problem, then attempt to solve the problem.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
 *    :param pkgconf_list_t* list: The list of dependency requests to consider.
 *    :param int maxdepth: The maximum allowed depth for the dependency resolver.  A depth of -1 means unlimited.
 *    :returns: true if the dependency resolver found a solution, otherwise false.
 *    :rtype: bool
 */
bool
pkgconf_queue_validate(pkgconf_client_t *client, pkgconf_list_t *list, int maxdepth)
{
	bool retval = true;
	pkgconf_pkg_t world = {
		.id = "virtual:world",
		.realname = "virtual world package",
		.flags = PKGCONF_PKG_PROPF_STATIC | PKGCONF_PKG_PROPF_VIRTUAL,
	};

	/* if maxdepth is one, then we will not traverse deeper than our virtual package. */
	if (!maxdepth)
		maxdepth = -1;

	if (pkgconf_queue_verify(client, &world, list, maxdepth) != PKGCONF_PKG_ERRF_OK)
		retval = false;

	pkgconf_pkg_free(client, &world);

	return retval;
}
