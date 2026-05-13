/*
 * variable.c
 * variable management
 *
 * Copyright (c) 2026 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <libpkgconf/libpkgconf.h>
#include <libpkgconf/stdinc.h>

/*
 * !doc
 *
 * libpkgconf `variable` module
 * ============================
 *
 * The libpkgconf `variable` module contains the functions related to
 * managing variables.  It replaces the old `tuple` module.
 */

pkgconf_variable_t *
pkgconf_variable_new(const char *key)
{
	pkgconf_variable_t *v;

	if (key == NULL)
		return NULL;

	v = calloc(1, sizeof(*v));
	if (v == NULL)
		return NULL;

	v->key = strdup(key);
	if (v->key == NULL)
	{
		free(v);
		return NULL;
	}

	return v;
}

void
pkgconf_variable_free(pkgconf_variable_t *v)
{
	if (v == NULL)
		return;

	pkgconf_buffer_finalize(&v->bcbuf);
	free(v->key);
	free(v);
}

pkgconf_variable_t *
pkgconf_variable_find(const pkgconf_list_t *vars, const char *key)
{
	const pkgconf_node_t *n;

	if (vars == NULL || key == NULL)
		return NULL;

	PKGCONF_FOREACH_LIST_ENTRY(vars->head, n)
	{
		pkgconf_variable_t *v = n->data;

		if (!strcmp(v->key, key))
			return v;
	}

	return NULL;
}

pkgconf_variable_t *
pkgconf_variable_get_or_create(pkgconf_list_t *vars, const char *key)
{
	pkgconf_variable_t *v;

	if (vars == NULL || key == NULL)
		return NULL;

	v = pkgconf_variable_find(vars, key);
	if (v != NULL)
		return v;

	v = pkgconf_variable_new(key);
	if (v == NULL)
		return NULL;

	pkgconf_node_insert_tail(&v->iter, v, vars);

	return v;
}

void
pkgconf_variable_delete(pkgconf_list_t *vars, pkgconf_variable_t *v)
{
	if (vars == NULL || v == NULL)
		return;

	pkgconf_node_delete(&v->iter, vars);
	pkgconf_variable_free(v);
}

void
pkgconf_variable_list_free(pkgconf_list_t *vars)
{
	pkgconf_node_t *node, *tmp;

	if (vars == NULL)
		return;

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(vars->head, tmp, node)
	{
		pkgconf_variable_t *v = node->data;

		pkgconf_node_delete(node, vars);
		pkgconf_variable_free(v);
	}
}

bool
pkgconf_variable_eval(pkgconf_client_t *client,
	const pkgconf_list_t *tuples,
	const pkgconf_variable_t *v,
	pkgconf_buffer_t *out,
	bool *saw_sysroot)
{
	if (client == NULL || tuples == NULL || v == NULL || out == NULL)
		return false;

	return pkgconf_bytecode_eval(client, tuples, &v->bc, out, saw_sysroot);
}

char *
pkgconf_variable_eval_str(pkgconf_client_t *client,
	const pkgconf_list_t *tuples,
	const pkgconf_variable_t *v,
	bool *saw_sysroot)
{
	pkgconf_buffer_t out = PKGCONF_BUFFER_INITIALIZER;

	if (client == NULL || tuples == NULL || v == NULL)
		return NULL;

	if (!pkgconf_variable_eval(client, tuples, v, &out, saw_sysroot))
	{
		pkgconf_buffer_finalize(&out);
		return NULL;
	}

	return pkgconf_buffer_freeze(&out);
}
