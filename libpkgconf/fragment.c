/*
 * fragment.c
 * Management of fragment lists.
 *
 * Copyright (c) 2012, 2013, 2014 pkgconf authors (see AUTHORS).
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
 * libpkgconf `fragment` module
 * ============================
 *
 * The `fragment` module provides low-level management and rendering of fragment lists.  A
 * `fragment list` contains various `fragments` of text (such as ``-I /usr/include``) in a matter
 * which is composable, mergeable and reorderable.
 */

struct pkgconf_fragment_check {
	char *token;
	size_t len;
};

static inline bool
pkgconf_fragment_is_unmergeable(const char *string)
{
	static const struct pkgconf_fragment_check check_fragments[] = {
		{"-framework", 10},
		{"-isystem", 8},
		{"-idirafter", 10},
		{"-pthread", 8},
		{"-Wa,", 4},
		{"-Wl,", 4},
		{"-Wp,", 4},
		{"-trigraphs", 10},
		{"-pedantic", 9},
		{"-ansi", 5},
		{"-std=", 5},
		{"-stdlib=", 8},
		{"-include", 8},
		{"-nostdinc", 9},
		{"-nostdlibinc", 12},
		{"-nobuiltininc", 13},
		{"-nodefaultlibs", 14},
	};

	if (*string != '-')
		return true;

	for (size_t i = 0; i < PKGCONF_ARRAY_SIZE(check_fragments); i++)
		if (!strncmp(string, check_fragments[i].token, check_fragments[i].len))
			return true;

	/* only one pair of {-flag, arg} may be merged together */
	if (strchr(string, ' ') != NULL)
		return false;

	return false;
}

static inline bool
pkgconf_fragment_should_munge(const char *string, const char *sysroot_dir)
{
	if (*string != '/')
		return false;

	if (sysroot_dir != NULL && strncmp(sysroot_dir, string, strlen(sysroot_dir)))
		return true;

	return false;
}

static inline bool
pkgconf_fragment_is_groupable(const char *string)
{
	static const struct pkgconf_fragment_check check_fragments[] = {
		{"-Wl,--start-group", 17},
		{"-framework", 10},
		{"-isystem", 8},
		{"-idirafter", 10},
		{"-include", 8},
	};

	for (size_t i = 0; i < PKGCONF_ARRAY_SIZE(check_fragments); i++)
		if (!strncmp(string, check_fragments[i].token, check_fragments[i].len))
			return true;

	return false;
}

static inline bool
pkgconf_fragment_is_terminus(const char *string)
{
	static const struct pkgconf_fragment_check check_fragments[] = {
		{"-Wl,--end-group", 15},
	};

	for (size_t i = 0; i < PKGCONF_ARRAY_SIZE(check_fragments); i++)
		if (!strncmp(string, check_fragments[i].token, check_fragments[i].len))
			return true;

	return false;
}

static inline bool
pkgconf_fragment_is_special(const char *string)
{
	if (*string != '-')
		return true;

	if (!strncmp(string, "-lib:", 5))
		return true;

	return pkgconf_fragment_is_unmergeable(string);
}

static inline void
pkgconf_fragment_munge(const pkgconf_client_t *client, char *buf, size_t buflen, const char *source, const char *sysroot_dir, unsigned int flags)
{
	*buf = '\0';

	if (!(flags & PKGCONF_PKG_PROPF_UNINSTALLED) || (client->flags & PKGCONF_PKG_PKGF_PKGCONF1_SYSROOT_RULES))
	{
		if (sysroot_dir == NULL)
			sysroot_dir = pkgconf_tuple_find_global(client, "pc_sysrootdir");

		if (sysroot_dir != NULL && pkgconf_fragment_should_munge(source, sysroot_dir))
			pkgconf_strlcat(buf, sysroot_dir, buflen);
	}

	pkgconf_strlcat(buf, source, buflen);

	if (*buf == '/' && !(client->flags & PKGCONF_PKG_PKGF_DONT_RELOCATE_PATHS))
		pkgconf_path_relocate(buf, buflen);
}

static inline char *
pkgconf_fragment_copy_munged(const pkgconf_client_t *client, const char *source, unsigned int flags)
{
	char mungebuf[PKGCONF_ITEM_SIZE];
	pkgconf_fragment_munge(client, mungebuf, sizeof mungebuf, source, client->sysroot_dir, flags);
	return strdup(mungebuf);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_fragment_insert(const pkgconf_client_t *client, pkgconf_list_t *list, char type, const char *data, bool tail)
 *
 *    Adds a `fragment` of text to a `fragment list` directly without interpreting it.
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param pkgconf_list_t* list: The fragment list.
 *    :param char type: The type of the fragment.
 *    :param char* data: The data of the fragment.
 *    :param bool tail: Whether to place the fragment at the beginning of the list or the end.
 *    :return: nothing
 */
void
pkgconf_fragment_insert(const pkgconf_client_t *client, pkgconf_list_t *list, char type, const char *data, bool tail)
{
	pkgconf_fragment_t *frag;

	frag = calloc(1, sizeof(pkgconf_fragment_t));
	frag->type = type;
	frag->data = pkgconf_fragment_copy_munged(client, data, 0);

	if (tail)
	{
		pkgconf_node_insert_tail(&frag->iter, frag, list);
		return;
	}

	pkgconf_node_insert(&frag->iter, frag, list);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_fragment_add(const pkgconf_client_t *client, pkgconf_list_t *list, const char *string, unsigned int flags)
 *
 *    Adds a `fragment` of text to a `fragment list`, possibly modifying the fragment if a sysroot is set.
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param pkgconf_list_t* list: The fragment list.
 *    :param char* string: The string of text to add as a fragment to the fragment list.
 *    :param uint flags: Parsing-related flags for the package.
 *    :return: nothing
 */
void
pkgconf_fragment_add(const pkgconf_client_t *client, pkgconf_list_t *list, const char *string, unsigned int flags)
{
	pkgconf_list_t *target = list;
	pkgconf_fragment_t *frag;

	if (*string == '\0')
		return;

	if (list->tail != NULL && list->tail->data != NULL &&
	    !(client->flags & PKGCONF_PKG_PKGF_DONT_MERGE_SPECIAL_FRAGMENTS))
	{
		pkgconf_fragment_t *parent = list->tail->data;

		/* only attempt to merge 'special' fragments together */
		if (!parent->type && parent->data != NULL &&
		    pkgconf_fragment_is_unmergeable(parent->data) &&
		    !(parent->flags & PKGCONF_PKG_FRAGF_TERMINATED))
		{
			if (pkgconf_fragment_is_groupable(parent->data))
				target = &parent->children;

			if (pkgconf_fragment_is_terminus(string))
				parent->flags |= PKGCONF_PKG_FRAGF_TERMINATED;

			PKGCONF_TRACE(client, "adding fragment as child to list @%p", target);
		}
	}

	if (strlen(string) > 1 && !pkgconf_fragment_is_special(string))
	{
		frag = calloc(1, sizeof(pkgconf_fragment_t));

		frag->type = *(string + 1);
		frag->data = pkgconf_fragment_copy_munged(client, string + 2, flags);

		PKGCONF_TRACE(client, "added fragment {%c, '%s'} to list @%p", frag->type, frag->data, list);
	}
	else
	{
		frag = calloc(1, sizeof(pkgconf_fragment_t));

		frag->type = 0;
		frag->data = pkgconf_fragment_copy_munged(client, string, flags);

		PKGCONF_TRACE(client, "created special fragment {'%s'} in list @%p", frag->data, target);
	}

	pkgconf_node_insert_tail(&frag->iter, frag, target);
}

static inline pkgconf_fragment_t *
pkgconf_fragment_lookup(pkgconf_list_t *list, const pkgconf_fragment_t *base)
{
	pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY_REVERSE(list->tail, node)
	{
		pkgconf_fragment_t *frag = node->data;

		if (base->type != frag->type)
			continue;

		if (!strcmp(base->data, frag->data))
			return frag;
	}

	return NULL;
}

static inline bool
pkgconf_fragment_can_merge_back(const pkgconf_fragment_t *base, unsigned int flags, bool is_private)
{
	(void) flags;

	if (base->type == 'l')
	{
		if (is_private)
			return false;

		return true;
	}

	if (base->type == 'F')
		return false;
	if (base->type == 'L')
		return false;
	if (base->type == 'I')
		return false;

	return true;
}

static inline bool
pkgconf_fragment_can_merge(const pkgconf_fragment_t *base, unsigned int flags, bool is_private)
{
	(void) flags;

	if (is_private)
		return false;

	if (base->children.head != NULL)
		return false;

	return pkgconf_fragment_is_unmergeable(base->data);
}

static inline pkgconf_fragment_t *
pkgconf_fragment_exists(pkgconf_list_t *list, const pkgconf_fragment_t *base, unsigned int flags, bool is_private)
{
	if (!pkgconf_fragment_can_merge_back(base, flags, is_private))
		return NULL;

	if (!pkgconf_fragment_can_merge(base, flags, is_private))
		return NULL;

	return pkgconf_fragment_lookup(list, base);
}

static inline bool
pkgconf_fragment_should_merge(const pkgconf_fragment_t *base)
{
	const pkgconf_fragment_t *parent;

	/* if we are the first fragment, that means the next fragment is the same, so it's always safe. */
	if (base->iter.prev == NULL)
		return true;

	/* this really shouldn't ever happen, but handle it */
	parent = base->iter.prev->data;
	if (parent == NULL)
		return true;

	switch (parent->type)
	{
	case 'l':
	case 'L':
	case 'I':
		return true;
	default:
		return !base->type || parent->type == base->type;
	}
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_fragment_has_system_dir(const pkgconf_client_t *client, const pkgconf_fragment_t *frag)
 *
 *    Checks if a `fragment` contains a `system path`.  System paths are detected at compile time and optionally overridden by
 *    the ``PKG_CONFIG_SYSTEM_INCLUDE_PATH`` and ``PKG_CONFIG_SYSTEM_LIBRARY_PATH`` environment variables.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object the fragment belongs to.
 *    :param pkgconf_fragment_t* frag: The fragment being checked.
 *    :return: true if the fragment contains a system path, else false
 *    :rtype: bool
 */
bool
pkgconf_fragment_has_system_dir(const pkgconf_client_t *client, const pkgconf_fragment_t *frag)
{
	const pkgconf_list_t *check_paths = NULL;

	switch (frag->type)
	{
	case 'L':
		check_paths = &client->filter_libdirs;
		break;
	case 'I':
		check_paths = &client->filter_includedirs;
		break;
	default:
		return false;
	}

	return pkgconf_path_match_list(frag->data, check_paths);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_fragment_copy(const pkgconf_client_t *client, pkgconf_list_t *list, const pkgconf_fragment_t *base, bool is_private)
 *
 *    Copies a `fragment` to another `fragment list`, possibly removing a previous copy of the `fragment`
 *    in a process known as `mergeback`.
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param pkgconf_list_t* list: The list the fragment is being added to.
 *    :param pkgconf_fragment_t* base: The fragment being copied.
 *    :param bool is_private: Whether the fragment list is a `private` fragment list (static linking).
 *    :return: nothing
 */
void
pkgconf_fragment_copy(const pkgconf_client_t *client, pkgconf_list_t *list, const pkgconf_fragment_t *base, bool is_private)
{
	pkgconf_fragment_t *frag;

	if ((frag = pkgconf_fragment_exists(list, base, client->flags, is_private)) != NULL)
	{
		if (pkgconf_fragment_should_merge(frag))
			pkgconf_fragment_delete(list, frag);
	}
	else if (!is_private && !pkgconf_fragment_can_merge_back(base, client->flags, is_private) && (pkgconf_fragment_lookup(list, base) != NULL))
		return;

	frag = calloc(1, sizeof(pkgconf_fragment_t));

	frag->type = base->type;
	pkgconf_fragment_copy_list(client, &frag->children, &base->children);
	if (base->data != NULL)
		frag->data = strdup(base->data);

	pkgconf_node_insert_tail(&frag->iter, frag, list);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_fragment_copy_list(const pkgconf_client_t *client, pkgconf_list_t *list, const pkgconf_list_t *base)
 *
 *    Copies a `fragment list` to another `fragment list`, possibly removing a previous copy of the fragments
 *    in a process known as `mergeback`.
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param pkgconf_list_t* list: The list the fragments are being added to.
 *    :param pkgconf_list_t* base: The list the fragments are being copied from.
 *    :return: nothing
 */
void
pkgconf_fragment_copy_list(const pkgconf_client_t *client, pkgconf_list_t *list, const pkgconf_list_t *base)
{
	pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY(base->head, node)
	{
		pkgconf_fragment_t *frag = node->data;

		pkgconf_fragment_copy(client, list, frag, true);
	}
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_fragment_filter(const pkgconf_client_t *client, pkgconf_list_t *dest, pkgconf_list_t *src, pkgconf_fragment_filter_func_t filter_func)
 *
 *    Copies a `fragment list` to another `fragment list` which match a user-specified filtering function.
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param pkgconf_list_t* dest: The destination list.
 *    :param pkgconf_list_t* src: The source list.
 *    :param pkgconf_fragment_filter_func_t filter_func: The filter function to use.
 *    :param void* data: Optional data to pass to the filter function.
 *    :return: nothing
 */
void
pkgconf_fragment_filter(const pkgconf_client_t *client, pkgconf_list_t *dest, pkgconf_list_t *src, pkgconf_fragment_filter_func_t filter_func, void *data)
{
	pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY(src->head, node)
	{
		pkgconf_fragment_t *frag = node->data;

		if (filter_func(client, frag, data))
			pkgconf_fragment_copy(client, dest, frag, true);
	}
}

static inline char *
fragment_quote(const pkgconf_fragment_t *frag)
{
	const char *src = frag->data;
	ssize_t outlen = strlen(src) + 10;
	char *out, *dst;

	if (frag->data == NULL)
		return NULL;

	out = dst = calloc(1, outlen);

	for (; *src; src++)
	{
		if (((*src < ' ') ||
		    (*src >= (' ' + (frag->children.head != NULL ? 1 : 0)) && *src < '$') ||
		    (*src > '$' && *src < '(') ||
		    (*src > ')' && *src < '+') ||
		    (*src > ':' && *src < '=') ||
		    (*src > '=' && *src < '@') ||
		    (*src > 'Z' && *src < '\\') ||
#ifndef _WIN32
		    (*src == '\\') ||
#endif
		    (*src > '\\' && *src < '^') ||
		    (*src == '`') ||
		    (*src > 'z' && *src < '~') ||
		    (*src > '~')))
			*dst++ = '\\';

		*dst++ = *src;

		if ((ptrdiff_t)(dst - out) + 2 > outlen)
		{
			ptrdiff_t offset = dst - out;
			outlen *= 2;
			out = realloc(out, outlen);
			dst = out + offset;
		}
	}

	*dst = 0;
	return out;
}

static inline size_t
pkgconf_fragment_len(const pkgconf_fragment_t *frag)
{
	size_t len = 1;

	if (frag->type)
		len += 2;

	if (frag->data != NULL)
	{
		pkgconf_node_t *iter;

		char *quoted = fragment_quote(frag);
		len += strlen(quoted);
		free(quoted);

		PKGCONF_FOREACH_LIST_ENTRY(frag->children.head, iter)
		{
			const pkgconf_fragment_t *child_frag = iter->data;
			len += pkgconf_fragment_len(child_frag) + 1;
		}
	}

	return len;
}

static size_t
fragment_render_len(const pkgconf_list_t *list, bool escape)
{
	(void) escape;

	size_t out = 1;		/* trailing nul */
	pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY(list->head, node)
	{
		const pkgconf_fragment_t *frag = node->data;
		out += pkgconf_fragment_len(frag);
	}

	return out;
}

static inline size_t
fragment_render_item(const pkgconf_fragment_t *frag, char *bptr, size_t bufremain)
{
	char *base = bptr;
	char *quoted = fragment_quote(frag);
	const pkgconf_node_t *iter;

	if (strlen(quoted) > bufremain)
	{
		free(quoted);
		return 0;
	}

	if (frag->type)
	{
		*bptr++ = '-';
		*bptr++ = frag->type;
	}

	if (quoted != NULL)
	{
		bptr += pkgconf_strlcpy(bptr, quoted, bufremain - (bptr - base));
		free(quoted);
	}

	PKGCONF_FOREACH_LIST_ENTRY(frag->children.head, iter)
	{
		const pkgconf_fragment_t *child_frag = iter->data;

		*bptr++ = ' ';
		bptr += fragment_render_item(child_frag, bptr, bufremain - (bptr - base));
	}

	return bptr - base;
}

static void
fragment_render_buf(const pkgconf_list_t *list, char *buf, size_t buflen, bool escape)
{
	(void) escape;

	pkgconf_node_t *node;
	char *bptr = buf;

	memset(buf, 0, buflen);

	PKGCONF_FOREACH_LIST_ENTRY(list->head, node)
	{
		const pkgconf_fragment_t *frag = node->data;
		size_t buf_remaining = buflen - (bptr - buf);
		size_t written = fragment_render_item(frag, bptr, buf_remaining);

		bptr += written;

		if (node->next != NULL)
			*bptr++ = ' ';
	}
}

static const pkgconf_fragment_render_ops_t default_render_ops = {
	.render_len = fragment_render_len,
	.render_buf = fragment_render_buf
};

/*
 * !doc
 *
 * .. c:function:: size_t pkgconf_fragment_render_len(const pkgconf_list_t *list, bool escape, const pkgconf_fragment_render_ops_t *ops)
 *
 *    Calculates the required memory to store a `fragment list` when rendered as a string.
 *
 *    :param pkgconf_list_t* list: The `fragment list` being rendered.
 *    :param bool escape: Whether or not to escape special shell characters (deprecated).
 *    :param pkgconf_fragment_render_ops_t* ops: An optional ops structure to use for custom renderers, else ``NULL``.
 *    :return: the amount of bytes required to represent the `fragment list` when rendered
 *    :rtype: size_t
 */
size_t
pkgconf_fragment_render_len(const pkgconf_list_t *list, bool escape, const pkgconf_fragment_render_ops_t *ops)
{
	(void) escape;

	ops = ops != NULL ? ops : &default_render_ops;
	return ops->render_len(list, true);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_fragment_render_buf(const pkgconf_list_t *list, char *buf, size_t buflen, bool escape, const pkgconf_fragment_render_ops_t *ops)
 *
 *    Renders a `fragment list` into a buffer.
 *
 *    :param pkgconf_list_t* list: The `fragment list` being rendered.
 *    :param char* buf: The buffer to render the fragment list into.
 *    :param size_t buflen: The length of the buffer.
 *    :param bool escape: Whether or not to escape special shell characters (deprecated).
 *    :param pkgconf_fragment_render_ops_t* ops: An optional ops structure to use for custom renderers, else ``NULL``.
 *    :return: nothing
 */
void
pkgconf_fragment_render_buf(const pkgconf_list_t *list, char *buf, size_t buflen, bool escape, const pkgconf_fragment_render_ops_t *ops)
{
	(void) escape;

	ops = ops != NULL ? ops : &default_render_ops;
	ops->render_buf(list, buf, buflen, true);
}

/*
 * !doc
 *
 * .. c:function:: char *pkgconf_fragment_render(const pkgconf_list_t *list)
 *
 *    Allocate memory and render a `fragment list` into it.
 *
 *    :param pkgconf_list_t* list: The `fragment list` being rendered.
 *    :param bool escape: Whether or not to escape special shell characters (deprecated).
 *    :param pkgconf_fragment_render_ops_t* ops: An optional ops structure to use for custom renderers, else ``NULL``.
 *    :return: An allocated string containing the rendered `fragment list`.
 *    :rtype: char *
 */
char *
pkgconf_fragment_render(const pkgconf_list_t *list, bool escape, const pkgconf_fragment_render_ops_t *ops)
{
	(void) escape;

	size_t buflen = pkgconf_fragment_render_len(list, true, ops);
	char *buf = calloc(1, buflen);

	pkgconf_fragment_render_buf(list, buf, buflen, true, ops);

	return buf;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_fragment_delete(pkgconf_list_t *list, pkgconf_fragment_t *node)
 *
 *    Delete a `fragment node` from a `fragment list`.
 *
 *    :param pkgconf_list_t* list: The `fragment list` to delete from.
 *    :param pkgconf_fragment_t* node: The `fragment node` to delete.
 *    :return: nothing
 */
void
pkgconf_fragment_delete(pkgconf_list_t *list, pkgconf_fragment_t *node)
{
	pkgconf_node_delete(&node->iter, list);

	free(node->data);
	free(node);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_fragment_free(pkgconf_list_t *list)
 *
 *    Delete an entire `fragment list`.
 *
 *    :param pkgconf_list_t* list: The `fragment list` to delete.
 *    :return: nothing
 */
void
pkgconf_fragment_free(pkgconf_list_t *list)
{
	pkgconf_node_t *node, *next;

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(list->head, next, node)
	{
		pkgconf_fragment_t *frag = node->data;

		pkgconf_fragment_free(&frag->children);
		free(frag->data);
		free(frag);
	}
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_fragment_parse(const pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_list_t *vars, const char *value)
 *
 *    Parse a string into a `fragment list`.
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param pkgconf_list_t* list: The `fragment list` to add the fragment entries to.
 *    :param pkgconf_list_t* vars: A list of variables to use for variable substitution.
 *    :param uint flags: Any parsing flags to be aware of.
 *    :param char* value: The string to parse into fragments.
 *    :return: true on success, false on parse error
 */
bool
pkgconf_fragment_parse(const pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_list_t *vars, const char *value, unsigned int flags)
{
	int i, ret, argc;
	char **argv;
	char *repstr = pkgconf_tuple_parse(client, vars, value, flags);

	PKGCONF_TRACE(client, "post-subst: [%s] -> [%s]", value, repstr);

	ret = pkgconf_argv_split(repstr, &argc, &argv);
	if (ret < 0)
	{
		PKGCONF_TRACE(client, "unable to parse fragment string [%s]", repstr);
		free(repstr);
		return false;
	}

	for (i = 0; i < argc; i++)
	{
		PKGCONF_TRACE(client, "processing %s", argv[i]);

		if (argv[i] == NULL)
		{
			PKGCONF_TRACE(client, "parsed fragment string is inconsistent: argc = %d while argv[%d] == NULL", argc, i);
			pkgconf_argv_free(argv);
			free(repstr);
			return false;
		}

		pkgconf_fragment_add(client, list, argv[i], flags);
	}

	pkgconf_argv_free(argv);
	free(repstr);

	return true;
}
