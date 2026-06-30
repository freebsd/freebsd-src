/*
 * tuple.c
 * management of key->value tuples
 *
 * Copyright (c) 2011, 2012 pkgconf authors (see AUTHORS).
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
 * libpkgconf `tuple` module
 * =========================
 *
 * The `tuple` module provides key-value mappings backed by a linked list.  The key-value
 * mapping is mainly used for variable substitution when parsing .pc files.
 *
 * There are two sets of mappings: a ``pkgconf_pkg_t`` specific mapping, and a `global` mapping.
 * The `tuple` module provides convenience wrappers for managing the `global` mapping, which is
 * attached to a given client object.
 */

/*
 * !doc
 *
 * .. c:function:: void pkgconf_tuple_add_global(pkgconf_client_t *client, const char *key, const char *value)
 *
 *    Defines a global variable, replacing the previous declaration if one was set.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to modify.
 *    :param char* key: The key for the mapping (variable name).
 *    :param char* value: The value for the mapped entry.
 *    :return: nothing
 */
void
pkgconf_tuple_add_global(pkgconf_client_t *client, const char *key, const char *value)
{
	pkgconf_tuple_add(client, &client->global_vars, key, value, false, 0);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_tuple_find_global(const pkgconf_client_t *client, const char *key)
 *
 *    Looks up a global variable.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to access.
 *    :param char* key: The key or variable name to look up.
 *    :return: the contents of the variable or ``NULL``
 *    :rtype: char *
 */
const char *
pkgconf_tuple_find_global(pkgconf_client_t *client, const char *key)
{
	pkgconf_variable_t *v;
	bool saw_sysroot = false;

	if (client == NULL || key == NULL)
		return NULL;

	v = pkgconf_variable_find(&client->global_vars, key);

	pkgconf_buffer_reset(&client->_scratch_buffer);
	(void) pkgconf_variable_eval(client, &client->global_vars, v, &client->_scratch_buffer, &saw_sysroot);

	return pkgconf_buffer_str_or_empty(&client->_scratch_buffer);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_tuple_free_global(pkgconf_client_t *client)
 *
 *    Delete all global variables associated with a pkgconf client object.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to modify.
 *    :return: nothing
 */
void
pkgconf_tuple_free_global(pkgconf_client_t *client)
{
	pkgconf_tuple_free(&client->global_vars);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_tuple_define_global(pkgconf_client_t *client, const char *kv)
 *
 *    Parse and define a global variable.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to modify.
 *    :param char* kv: The variable in the form of ``key=value``.
 *    :return: nothing
 */
void
pkgconf_tuple_define_global(pkgconf_client_t *client, const char *kv)
{
	char *workbuf = strdup(kv);
	char *value;
	pkgconf_tuple_t *tuple;

	if (workbuf == NULL)
		goto out;

	value = strchr(workbuf, '=');
	if (value == NULL)
		goto out;

	*value++ = '\0';

	tuple = pkgconf_tuple_add(client, &client->global_vars, workbuf, value, false, 0);
	if (tuple != NULL)
		tuple->flags = PKGCONF_PKG_TUPLEF_OVERRIDE;

out:
	free(workbuf);
}

static char *
dequote(const char *value)
{
	char *buf = calloc(1, (strlen(value) + 1) * 2);
	char *bptr = buf;
	const char *i;
	char quote = 0;

	if (buf == NULL)
		return NULL;

	if (*value == '\'' || *value == '"')
		quote = *value;

	for (i = value; *i != '\0'; i++)
	{
		if (*i == '\\' && quote && *(i + 1) == quote)
		{
			i++;
			*bptr++ = *i;
		}
		else if (*i != quote)
			*bptr++ = *i;
	}

	return buf;
}

/*
 * !doc
 *
 * .. c:function:: pkgconf_tuple_t *pkgconf_tuple_add(const pkgconf_client_t *client, pkgconf_list_t *list, const char *key, const char *value, bool parse)
 *
 *    Wrapper around pkgconf_variable_get_or_create(list, key) and bytecode compiler.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to access.
 *    :param pkgconf_list_t* list: The variable list to add the new variable to.
 *    :param char* key: The name of the variable being added.
 *    :param char* value: The value of the variable being added.
 *    :param bool parse: Whether or not to parse the value for variable substitution.
 *    :return: a variable object
 *    :rtype: pkgconf_tuple_t *
 */
pkgconf_tuple_t *
pkgconf_tuple_add(const pkgconf_client_t *client, pkgconf_list_t *list, const char *key, const char *value, bool parse, unsigned int flags)
{
	char *dequote_value;
	pkgconf_buffer_t rhs_bcbuf = PKGCONF_BUFFER_INITIALIZER;

	(void) client;

	if (list == NULL || key == NULL || value == NULL)
		return NULL;

	dequote_value = dequote(value);
	if (dequote_value == NULL)
		return NULL;

	pkgconf_variable_t *v = pkgconf_variable_get_or_create(list, key);
	if (v == NULL)
	{
		free(dequote_value);
		return NULL;
	}

	v->flags = flags;

	if (!parse)
	{
		pkgconf_buffer_reset(&v->bcbuf);
		pkgconf_bytecode_emit_text(&v->bcbuf, dequote_value, strlen(dequote_value));
		pkgconf_bytecode_from_buffer(&v->bc, &v->bcbuf);
		free(dequote_value);
		return (pkgconf_tuple_t *) v;
	}

	pkgconf_bytecode_compile(&rhs_bcbuf, dequote_value);
	free(dequote_value);

	/* ugh, we are doing var=${var}/foo stuff */
	if (pkgconf_bytecode_references_var(&rhs_bcbuf, key))
	{
		pkgconf_buffer_t old_bcbuf = PKGCONF_BUFFER_INITIALIZER;
		pkgconf_buffer_t new_bcbuf = PKGCONF_BUFFER_INITIALIZER;

		/* preserve the old bytecode */
		pkgconf_buffer_copy(&v->bcbuf, &old_bcbuf);

		/* splice the selfrefs, using the old bytecode instead of ${var} */
		if (!pkgconf_bytecode_rewrite_selfrefs(&new_bcbuf, &rhs_bcbuf, key, &old_bcbuf))
		{
			pkgconf_buffer_finalize(&old_bcbuf);
			pkgconf_buffer_finalize(&new_bcbuf);
			pkgconf_buffer_finalize(&rhs_bcbuf);

			return NULL;
		}

		/* copy the spliced bytecode back to &rhs_bcbuf, replacing its contents */
		pkgconf_buffer_copy(&new_bcbuf, &rhs_bcbuf);

		pkgconf_buffer_finalize(&old_bcbuf);
		pkgconf_buffer_finalize(&new_bcbuf);
	}

	pkgconf_buffer_copy(&rhs_bcbuf, &v->bcbuf);
	pkgconf_bytecode_from_buffer(&v->bc, &v->bcbuf);
	pkgconf_buffer_finalize(&rhs_bcbuf);

	return (pkgconf_tuple_t *) v;
}

/*
 * !doc
 *
 * .. c:function:: char *pkgconf_tuple_find(const pkgconf_client_t *client, pkgconf_list_t *list, const char *key)
 *
 *    Look up a variable in a variable list.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to access.
 *    :param pkgconf_list_t* list: The variable list to search.
 *    :param char* key: The variable name to search for.
 *    :return: the value of the variable or ``NULL``
 *    :rtype: char *
 */
const char *
pkgconf_tuple_find(pkgconf_client_t *client, pkgconf_list_t *list, const char *key)
{
	pkgconf_variable_t *v;

	if (client == NULL || list == NULL || key == NULL)
		return NULL;

	v = pkgconf_variable_find(list, key);
	if (v == NULL)
		v = pkgconf_variable_find(&client->global_vars, key);

	pkgconf_buffer_reset(&client->_scratch_buffer);

	(void) pkgconf_variable_eval(client, list, v, &client->_scratch_buffer, NULL);

	return pkgconf_buffer_str_or_empty(&client->_scratch_buffer);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_tuple_free_entry(pkgconf_tuple_t *tuple, pkgconf_list_t *list)
 *
 *    Deletes a variable object, removing it from any variable lists and releasing any memory associated
 *    with it.
 *
 *    :param pkgconf_tuple_t* tuple: The variable object to release.
 *    :param pkgconf_list_t* list: The variable list the variable object is attached to.
 *    :return: nothing
 */
void
pkgconf_tuple_free_entry(pkgconf_tuple_t *tuple, pkgconf_list_t *list)
{
	pkgconf_node_delete(&tuple->iter, list);
	pkgconf_variable_free(tuple);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_tuple_free(pkgconf_list_t *list)
 *
 *    Deletes a variable list and any variables attached to it.
 *
 *    :param pkgconf_list_t* list: The variable list to delete.
 *    :return: nothing
 */
void
pkgconf_tuple_free(pkgconf_list_t *list)
{
	pkgconf_node_t *node, *next;

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(list->head, next, node)
		pkgconf_tuple_free_entry(node->data, list);

	pkgconf_list_zero(list);
}
