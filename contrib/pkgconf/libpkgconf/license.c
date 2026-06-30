/*
 * license.c
 * Evaluate SPDX license expressions
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

#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>

static void license_sanitize_string(const char *s, pkgconf_buffer_t *buf)
{
	unsigned int i = 0;
	/*
	 * Allowed chars are:
	 * - a-z
	 * - 0-9
	 * - chars '-', '+', '.', '(' and ')'
	 */
	for (i = 0; i < strlen(s); i ++)
	{
		if (isalnum(s[i]) || s[i] == '-' || s[i] == '+' || s[i] == '(' || s[i] == ')' || s[i] == '.' || s[i] == ':')
		{
			pkgconf_buffer_push_byte(buf, s[i]);
		}
	}
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_license_copy_list(const pkgconf_client_t *client, pkgconf_list_t *list, const pkgconf_list_t *base)
 *
 *    Copies a `license list` to another `license list`
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param pkgconf_list_t* list: The list the fragments are being added to.
 *    :param pkgconf_list_t* base: The list the fragments are being copied from.
 *    :return: nothing
 */
void
pkgconf_license_copy_list(const pkgconf_client_t *client, pkgconf_list_t *list, const pkgconf_list_t *base)
{
	pkgconf_node_t *node;
	(void) client;

	PKGCONF_FOREACH_LIST_ENTRY(base->head, node)
	{
		pkgconf_license_t *license = node->data;
		pkgconf_license_t *cpy_license = calloc(1, sizeof(pkgconf_license_t));

		cpy_license->type = license->type;

		if (license->data != NULL)
		{
			cpy_license->data = strdup(license->data);
		}

		pkgconf_node_insert_tail(&cpy_license->iter, cpy_license, list);
	}
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_license_free(pkgconf_list_t *list)
 *
 *    Delete an entire `license list`.
 *
 *    :param pkgconf_list_t* list: The `license list` to delete.
 *    :return: nothing
 */
void
pkgconf_license_free(pkgconf_list_t *list)
{
	if (!list)
		return;

	pkgconf_node_t *node, *next;

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(list->head, next, node)
	{
		pkgconf_license_t *license = node->data;
		free(license->data);
		free(license);
	}
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_license_insert(pkgconf_client_t *client, pkgconf_list_t *list, unsigned char type, const char *data)
 *
 *    Adds a `license` of text to a `license list` directly without interpreting it.
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param pkgconf_list_t* list: The fragment list.
 *    :param char type: The type of the license.
 *    :param char* data: The data of the license
 *    :return: nothing
 */
void
pkgconf_license_insert(pkgconf_client_t *client, pkgconf_list_t *list, unsigned char type, const char *data)
{
	(void) client;

	pkgconf_license_t *license;

	license = calloc(1, sizeof(pkgconf_license_t));
	if (!license)
	{
		pkgconf_error(client, "pkgconf_license_insert: out of memory");
		return;
	}
	license->type = type;
	license->data = strdup(data);

	pkgconf_node_insert_tail(&license->iter, license, list);
}

/*
 * !doc
 *
 * .. c:function:: pkgconf_license_evaluate_str(pkgconf_client_t *client, pkgconf_list_t *license_list, const char *expression, unsigned int flags)
 *
 * Evaluates SPDX expression strings like:
 * - BSD-3-Clause
 * - LGPL-2.1-only OR MIT
 * - LGPL-2.1-only OR MIT OR BSD-3-Clause
 * - ISC AND (BSD-3-Clause AND BSD-2-Clause)
 *
 * Function parses and sanitizes license strings. Also adding multiple
 * 'License:'-keys is supported like:
 * License: BSD-3-Clause
 * License: BSD-2-Clause
 *
 * Which will evaluate to: BSD-3-Clause AND BSD-2-Clause
 *
 * :param pkgconf_client_t* client: The client object that owns the package this dependency list belongs to.
 * :param pkgconf_list_t* license_list: The dependency list to populate with dependency nodes.
 * :param char* expression: The dependency data to parse.
 * :param uint flags: Any flags to attach to the dependency nodes.
 * :return: nothing
 */
void
pkgconf_license_evaluate_str(pkgconf_client_t *client, pkgconf_list_t *license_list, const char *expression, unsigned int flags)
{
	pkgconf_buffer_t out_buffer = PKGCONF_BUFFER_INITIALIZER;
	size_t buf_size = 0;
	char *cur_word = NULL;
	int i, ret, argc;
	char **argv;
	size_t string_len = 0;

	(void)flags;

	buf_size = strlen(expression) + 1;
	ret = pkgconf_argv_split(expression, &argc, &argv);
	if (!ret)
	{
		/* This is not the first License:
		 * so add AND
		 */
		if (license_list->head)
		{
			pkgconf_license_insert(client, license_list, PKGCONF_LICENSE_AND, "AND");
		}

		i = 0;
		while (i < argc)
		{
			string_len = strnlen(argv[i], buf_size);
			cur_word = argv[i];
			if (string_len >= 1)
			{
				license_sanitize_string(cur_word, &out_buffer);
				cur_word = (char *)pkgconf_buffer_str_or_empty(&out_buffer);
				if (!strnlen(cur_word, buf_size))
				{
					i ++;
					pkgconf_buffer_finalize(&out_buffer);
					continue;
				}

				if (cur_word[0] == '(')
				{
					pkgconf_license_insert(client, license_list, PKGCONF_LICENSE_BRACKET_OPEN, "(");
					/* If there is more after '(' like '(BSD-2-Clause'
					 * Then append rest to fragments as license.
					 * This is expression like GPL-2.0-only OR (BSD-2-Clause AND ISC)
					 */
					if (string_len >= 2)
					{
						cur_word ++;
						pkgconf_license_insert(client, license_list, PKGCONF_LICENSE_EXPRESSION, cur_word);
					}
				}
				else if (cur_word[string_len - 1] == ')')
				{
					if (string_len >= 2)
					{
						argv[i][string_len - 1] = 0x00;
						pkgconf_license_insert(client, license_list, PKGCONF_LICENSE_EXPRESSION, argv[i]);
					}
					pkgconf_license_insert(client, license_list, PKGCONF_LICENSE_BRACKET_CLOSE, ")");
				}
				else if (!strncasecmp(cur_word, "and", 3))
				{
					pkgconf_license_insert(client, license_list, PKGCONF_LICENSE_AND, "AND");
				}
				else if (!strncasecmp(cur_word, "or", 2))
				{
					pkgconf_license_insert(client, license_list, PKGCONF_LICENSE_OR, "OR");
				}
				else if (!strncasecmp(cur_word, "with", 2))
				{
					pkgconf_license_insert(client, license_list, PKGCONF_LICENSE_WITH, "WITH");
				}
				else
				{
					pkgconf_license_insert(client, license_list, PKGCONF_LICENSE_EXPRESSION, cur_word);
				}
				pkgconf_buffer_finalize(&out_buffer);
			}
			i++;
		}
	}

	pkgconf_argv_free(argv);
}

/*
 * !doc
 *
 * .. c:function:: pkgconf_license_evaluate_str(pkgconf_client_t *client, pkgconf_list_t *license_list, const char *expression, unsigned int flags)
 *
 * Evaluates SPDX expression strings like:
 * - BSD-3-Clause
 * - LGPL-2.1-only OR MIT
 * - LGPL-2.1-only OR MIT OR BSD-3-Clause
 * - ISC AND (BSD-3-Clause AND BSD-2-Clause)
 *
 * Function parses and sanitizes license strings. Also adding multiple
 * 'License:'-keys is supported like:
 * License: BSD-3-Clause
 * License: BSD-2-Clause
 *
 * Which will evaluate to: BSD-3-Clause AND BSD-2-Clause
 *
 * :param pkgconf_client_t* client: The client object that owns the package this dependency list belongs to.
 * :param pkgconf_list_t* license_list: The dependency list to populate with dependency nodes.
 * :param char* expression: The dependency data to parse.
 * :param uint flags: Any flags to attach to the dependency nodes.
 * :return: nothing
 */
void
pkgconf_license_evaluate(pkgconf_client_t *client, pkgconf_pkg_t *pkg, pkgconf_list_t *license_list, const char *license_str, unsigned int flags)
{
	char *license_expression = pkgconf_bytecode_eval_str(client, &pkg->vars, license_str, NULL);

	pkgconf_license_evaluate_str(client, license_list, license_expression, flags);
	free(license_expression);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_dependency_parse_str(pkgconf_list_t *deplist_head, const char *depends)
 *
 * Renders license fragments back to SPDX expression string. Tries
 * to keep output as close as possible to original input
 *
 * :param pkgconf_client_t* client: The client object that owns the package this dependency list belongs to.
 * :param pkgconf_list_t* deplist_head: The dependency list to populate with dependency nodes.
 * :param char* depends: The dependency data to parse.
 * :param uint flags: Any flags to attach to the dependency nodes.
 * :return: nothing
 */
void
pkgconf_license_render(pkgconf_client_t *client, const pkgconf_list_t *list, pkgconf_buffer_t *buf)
{
	const pkgconf_buffer_t *frag_string = NULL;
	pkgconf_node_t *node;
	bool is_delim = true;

	(void)client;

	PKGCONF_FOREACH_LIST_ENTRY(list->head, node)
	{
		pkgconf_license_t *license = node->data;
		is_delim = true;
		frag_string = PKGCONF_BUFFER_FROM_STR(license->data);
		pkgconf_buffer_append(buf, pkgconf_buffer_str_or_empty(frag_string));

		if (license->type == PKGCONF_LICENSE_BRACKET_OPEN || (node->next != NULL && ((const pkgconf_license_t *)node->next)->type == PKGCONF_LICENSE_BRACKET_CLOSE))
		{
			is_delim = false;
		}

		if (node->next != NULL && is_delim)
		{
			pkgconf_buffer_push_byte(buf, ' ');
		}
	}
}
