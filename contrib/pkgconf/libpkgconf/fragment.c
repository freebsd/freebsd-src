/*
 * fragment.c
 * Management of fragment lists.
 *
 * SPDX-License-Identifier: pkgconf
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

#include <libpkgconf/config.h>
#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>

#ifndef _WIN32
#include <locale.h>
#include <langinfo.h>
#endif

#ifdef __APPLE__
#include <xlocale.h>
#endif

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
	const char *token;
	size_t len;
};

static inline bool
pkgconf_fragment_is_greedy(const char *string)
{
	static const struct pkgconf_fragment_check check_fragments[] = {
		{"-F", 2},
		{"-I", 2},
		{"-L", 2},
		{"-D", 2},
		{"-l", 2},
	};

	if (*string != '-')
		return false;

	for (size_t i = 0; i < PKGCONF_ARRAY_SIZE(check_fragments); i++)
		if (!strncmp(string, check_fragments[i].token, check_fragments[i].len))
		{
			/* if it is the bare flag, then we want the next token to be the data */
			if (!*(string + check_fragments[i].len))
				return true;
		}

	return false;
}

/*
 * Length of the flag introducing a path which sysroot may be injected into, or
 * 0 if this is not such a flag.  The length matters as well as the answer: the
 * path starts there, and it is the path which has to be judged, not the whole
 * fragment.
 */
static inline size_t
pkgconf_fragment_sysroot_path_offset(const char *string)
{
	static const struct pkgconf_fragment_check check_fragments[] = {
		{"-F", 2},
		{"-I", 2},
		{"-L", 2},
		{"-isystem", 8},
		{"-idirafter", 10},
	};

	if (*string != '-')
		return 0;

	for (size_t i = 0; i < PKGCONF_ARRAY_SIZE(check_fragments); i++)
		if (!strncmp(string, check_fragments[i].token, check_fragments[i].len))
			return check_fragments[i].len;

	return 0;
}

/*
 * Length of a flag which names its path as a separate argument, or 0 if this is
 * not one of those.
 */
static inline size_t
pkgconf_fragment_separate_path_offset(const char *string)
{
	static const struct pkgconf_fragment_check check_fragments[] = {
		{"-isystem", 8},
		{"-idirafter", 10},
	};

	if (*string != '-')
		return 0;

	for (size_t i = 0; i < PKGCONF_ARRAY_SIZE(check_fragments); i++)
		if (!strncmp(string, check_fragments[i].token, check_fragments[i].len))
			return check_fragments[i].len;

	return 0;
}

/*
 * Whether `path` already lies under the sysroot, in which case injecting it
 * again would only produce a doubled prefix.
 *
 * This is read off the path rather than remembered from the expansion which
 * produced it.  A fragment carries everything the question needs, so the answer
 * holds per fragment however the .pc file arrived at the path -- whether the
 * sysroot came from ${pc_sysrootdir}, from a variable which happens to sit under
 * it, or was written out longhand -- and survives the path being split out of a
 * larger expansion, where provenance would not.
 */
static inline bool
pkgconf_fragment_under_sysroot(const pkgconf_client_t *client, const char *path)
{
	size_t len = strlen(client->sysroot_dir);

	if (strncmp(path, client->sysroot_dir, len))
		return false;

	return path[len] == '/' || path[len] == '\0';
}

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
pkgconf_fragment_only_group_one(const char *string)
{
	static const struct pkgconf_fragment_check check_fragments[] = {
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
pkgconf_fragment_is_groupable(const char *string)
{
	static const struct pkgconf_fragment_check check_fragments[] = {
		{"-Wl,--start-group", 17},
	};

	if (pkgconf_fragment_only_group_one(string))
		return true;

	for (size_t i = 0; i < PKGCONF_ARRAY_SIZE(check_fragments); i++)
		if (!strncmp(string, check_fragments[i].token, check_fragments[i].len))
			return true;

	return false;
}

static inline bool
pkgconf_fragment_is_terminus(const char *parent, const char *string)
{
	static const struct pkgconf_fragment_check check_fragments[] = {
		{"-Wl,--end-group", 15},
	};

	if (pkgconf_fragment_only_group_one(parent))
		return true;

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

static pkgconf_fragment_t *
fragment_new(char type, const char *data)
{
	size_t datalen = data != NULL ? strlen(data) : 0;
	pkgconf_fragment_t *frag = calloc(1, sizeof(*frag) + (data != NULL ? datalen + 1 : 0));

	if (frag == NULL)
		return NULL;

	frag->type = type;

	if (data != NULL)
	{
		frag->data = (char *)(frag + 1);
		memcpy(frag->data, data, datalen + 1);
	}

	return frag;
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
pkgconf_fragment_insert(pkgconf_client_t *client, pkgconf_list_t *list, char type, const char *data, bool tail)
{
	(void) client;

	pkgconf_fragment_t *frag;

	frag = fragment_new(type, data);
	if (frag == NULL)
		return;

	if (tail)
	{
		pkgconf_node_insert_tail(&frag->iter, frag, list);
		return;
	}

	pkgconf_node_insert(&frag->iter, frag, list);
}

static bool
should_inject_sysroot(const pkgconf_client_t *client, const char *string, unsigned int flags)
{
	size_t offset;

	if (client->flags & PKGCONF_PKG_PKGF_NO_SYSROOT_INJECTION)
		return false;

	/* we never automatically inject sysroot on -uninstalled packages */
	if (flags & PKGCONF_PKG_PROPF_UNINSTALLED)
	{
		/* ... unless we are emulating pkgconf 1.x */
		if (!(client->flags & PKGCONF_PKG_PKGF_PKGCONF1_SYSROOT_RULES))
			return false;
	}

	if (client->sysroot_dir == NULL)
		return false;

	offset = pkgconf_fragment_sysroot_path_offset(string);
	if (offset == 0)
		return false;

	return !pkgconf_fragment_under_sysroot(client, string + offset);
}

static bool
should_inject_sysroot_child(const pkgconf_client_t *client, const pkgconf_fragment_t *last, const char *string, unsigned int flags)
{
	if (client->flags & PKGCONF_PKG_PKGF_NO_SYSROOT_INJECTION)
		return false;

	/* we never automatically inject sysroot on -uninstalled packages */
	if (flags & PKGCONF_PKG_PROPF_UNINSTALLED)
	{
		/* ... unless we are emulating pkgconf 1.x */
		if (!(client->flags & PKGCONF_PKG_PKGF_PKGCONF1_SYSROOT_RULES))
			return false;
	}

	if (last->type)
		return false;

	if (last->data == NULL)
		return false;

	if (client->sysroot_dir == NULL)
		return false;

	/* the flag is the preceding fragment; this one is the bare path it takes.
	 * a flag which has already taken its argument takes no further ones, so
	 * what follows is a fragment in its own right and not a path at all. */
	if (last->flags & PKGCONF_PKG_FRAGF_TERMINATED)
		return false;

	if (pkgconf_fragment_sysroot_path_offset(last->data) == 0)
		return false;

	return !pkgconf_fragment_under_sysroot(client, string);
}

/*
 * Insert an already-expanded fragment string into the list.  No variable
 * substitution is performed here: `string` is taken verbatim, so this must
 * only be called with input that has already been through the bytecode
 * evaluator.  Whether sysroot injection is still wanted is read off `string`
 * itself; see pkgconf_fragment_under_sysroot.
 */
static bool
fragment_insert_evaluated(pkgconf_client_t *client, pkgconf_list_t *list, const char *string, unsigned int flags)
{
	pkgconf_list_t *target = list;
	pkgconf_fragment_t *terminate_parent = NULL;
	pkgconf_fragment_t *frag;
	size_t separate;

	if (string == NULL || *string == '\0')
		return true;

	/* A .pc file may join one of these flags to its path.  That names the same
	 * thing as writing them apart, so split it back apart and let there be one
	 * spelling of it in a fragment list: the flag, and the path it takes. */
	separate = pkgconf_fragment_separate_path_offset(string);
	if (separate != 0 && string[separate] != '\0')
	{
		pkgconf_buffer_t flagbuf = PKGCONF_BUFFER_INITIALIZER;
		bool ok = pkgconf_buffer_append_slice(&flagbuf, string, separate) &&
			fragment_insert_evaluated(client, list, pkgconf_buffer_str(&flagbuf), flags) &&
			fragment_insert_evaluated(client, list, string + separate, flags);

		pkgconf_buffer_finalize(&flagbuf);
		return ok;
	}

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

			if (pkgconf_fragment_is_terminus(parent->data, string))
				terminate_parent = parent;

			PKGCONF_TRACE(client, "adding fragment as child to list @%p", target);
		}
	}

	/* Compute the final data string first (borrowing sysroot_buf when we have to
	 * prepend the sysroot), then hand it to fragment_new(), which copies it
	 * inline.  data == NULL here means an allocation/append failure. */
	{
		char type = 0;
		const char *data = NULL;
		pkgconf_buffer_t sysroot_buf = PKGCONF_BUFFER_INITIALIZER;

		if (strlen(string) > 1 && !pkgconf_fragment_is_special(string))
		{
			type = *(string + 1);

			if (should_inject_sysroot(client, string, flags))
			{
				if (pkgconf_buffer_append(&sysroot_buf, client->sysroot_dir) &&
					pkgconf_buffer_append(&sysroot_buf, string + 2))
					data = pkgconf_buffer_str(&sysroot_buf);
			}
			else
				data = string + 2;
		}
		else
		{
			type = 0;

			if (client->sysroot_dir != NULL && list->tail != NULL && list->tail->data != NULL &&
				should_inject_sysroot_child(client, list->tail->data, string, flags))
			{
				if (pkgconf_buffer_append(&sysroot_buf, client->sysroot_dir) &&
					pkgconf_buffer_append(&sysroot_buf, string))
					data = pkgconf_buffer_str(&sysroot_buf);
			}
			else
				data = string;
		}

		frag = data != NULL ? fragment_new(type, data) : NULL;
		pkgconf_buffer_finalize(&sysroot_buf);
	}

	if (frag == NULL)
	{
		PKGCONF_TRACE(client, "failed to add new fragment due to allocation failure to list @%p", target);
		return false;
	}

	if (frag->type)
		PKGCONF_TRACE(client, "added fragment {%c, '%s'} to list @%p", frag->type, frag->data, list);
	else
		PKGCONF_TRACE(client, "created special fragment {'%s'} in list @%p", frag->data, target);

	pkgconf_node_insert_tail(&frag->iter, frag, target);
	if (terminate_parent != NULL)
		terminate_parent->flags |= PKGCONF_PKG_FRAGF_TERMINATED;

	return true;
}

/*
 * Split a string into whitespace-delimited fragments, honouring shell quoting
 * and greedy flags (e.g. "-I /usr/include" -> "-I/usr/include").
 *
 * When `evaluate` is true, `value` is a property as the .pc file spells it and
 * each token is handed to pkgconf_fragment_add to be expanded.  Splitting it
 * only settles where the tokens end, so the quoting is left in place for the
 * pass over the expansion to consume.
 *
 * When `evaluate` is false, `value` is the expansion of one such token.  This is
 * where the quoting is consumed, once, over text which is otherwise finished,
 * and the tokens are inserted as they stand.  It must not evaluate again, lest
 * an expansion yielding a literal "${...}" (via a "$$" escape) recurse forever.
 */
static bool
fragment_split(pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_list_t *vars, const char *value, unsigned int flags, bool evaluate)
{
	int i, ret, argc;
	char **argv;

	ret = evaluate
		? pkgconf_argv_split_raw(value, &argc, &argv)
		: pkgconf_argv_split(value, &argc, &argv);
	if (ret < 0)
	{
		PKGCONF_TRACE(client, "unable to parse fragment string [%s]", value);
		return false;
	}

	for (i = 0; i < argc; i++)
	{
		const char *token;
		pkgconf_buffer_t greedybuf = PKGCONF_BUFFER_INITIALIZER;
		bool ok;

		if (argv[i] == NULL)
		{
			PKGCONF_TRACE(client, "parsed fragment string is inconsistent: argc = %d while argv[%d] == NULL", argc, i);
			pkgconf_argv_free(argv);
			return false;
		}

		token = argv[i];

		PKGCONF_TRACE(client, "processing [%s]", argv[i]);

		if (pkgconf_fragment_is_greedy(argv[i]) && i + 1 < argc)
		{
			if (!pkgconf_buffer_append(&greedybuf, argv[i]) ||
				!pkgconf_buffer_append(&greedybuf, argv[i + 1]))
			{
				pkgconf_buffer_finalize(&greedybuf);
				pkgconf_argv_free(argv);
				return false;
			}

			token = pkgconf_buffer_str(&greedybuf);

			/* skip over next arg as we combined them */
			i++;
		}

		if (evaluate)
			ok = pkgconf_fragment_add(client, list, vars, token, flags);
		else
			ok = fragment_insert_evaluated(client, list, token, flags);

		pkgconf_buffer_finalize(&greedybuf);

		if (!ok)
		{
			pkgconf_argv_free(argv);
			return false;
		}
	}

	pkgconf_argv_free(argv);

	return true;
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_fragment_add(const pkgconf_client_t *client, pkgconf_list_t *list, const char *string, unsigned int flags)
 *
 *    Adds a `fragment` of text to a `fragment list`, possibly modifying the fragment if a sysroot is set.
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param pkgconf_list_t* list: The fragment list.
 *    :param char* string: The string of text to add as a fragment to the fragment list.
 *    :param uint flags: Parsing-related flags for the package.
 *    :return: true on success, false on parse error or allocation failure
 */
bool
pkgconf_fragment_add(pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_list_t *vars, const char *value, unsigned int flags)
{
	pkgconf_buffer_t evalbuf = PKGCONF_BUFFER_INITIALIZER;
	bool ret;

	if (!pkgconf_bytecode_eval_str_to_buf(client, vars, value, NULL, &evalbuf))
	{
		pkgconf_buffer_finalize(&evalbuf);
		return false;
	}

	if (pkgconf_buffer_len(&evalbuf) == 0)
	{
		pkgconf_buffer_finalize(&evalbuf);
		return true;
	}

	/* fragment_split() only reads the expanded string, so hand it the buffer
	 * contents directly rather than freezing (copying) them out.
	 *
	 * The expansion is split rather than taken whole: the quoting it carries is
	 * consumed there, and a value may in any case expand to several
	 * whitespace-separated fragments.
	 */
	ret = fragment_split(client, list, vars, pkgconf_buffer_str(&evalbuf), flags, false);

	pkgconf_buffer_finalize(&evalbuf);
	return ret;
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

		if (base->data == NULL || frag->data == NULL)
		{
			if (base->data == frag->data)
				return frag;

			continue;
		}

		if (!strcmp(base->data, frag->data))
			return frag;
	}

	return NULL;
}

/* Order fragments by (type, data) so that the cursor index can be searched
 * with bsearch().  NULL data sorts before any non-NULL data of the same type. */
static int
fragment_index_cmp(const void *a, const void *b)
{
	const pkgconf_fragment_t *fa = *(pkgconf_fragment_t * const *) a;
	const pkgconf_fragment_t *fb = *(pkgconf_fragment_t * const *) b;

	if (fa->type != fb->type)
		return (unsigned char) fa->type < (unsigned char) fb->type ? -1 : 1;

	if (fa->data == NULL || fb->data == NULL)
	{
		if (fa->data == fb->data)
			return 0;

		return fa->data == NULL ? -1 : 1;
	}

	return strcmp(fa->data, fb->data);
}

static pkgconf_fragment_t *
fragment_index_lookup(const pkgconf_fragment_cursor_t *cursor, const pkgconf_fragment_t *base)
{
	pkgconf_fragment_t **found;

	if (cursor->count == 0)
		return NULL;

	found = bsearch(&base, cursor->index, cursor->count, sizeof(void *), fragment_index_cmp);
	return found != NULL ? *found : NULL;
}

static bool
fragment_index_insert(pkgconf_fragment_cursor_t *cursor, pkgconf_fragment_t *frag)
{
	size_t lo = 0, hi = cursor->count;

	if (cursor->count == cursor->alloc)
	{
		size_t newalloc = cursor->alloc != 0 ? cursor->alloc * 2 : 16;
		pkgconf_fragment_t **newindex = pkgconf_reallocarray(cursor->index, newalloc, sizeof(void *));

		if (newindex == NULL)
			return false;

		cursor->index = newindex;
		cursor->alloc = newalloc;
	}

	while (lo < hi)
	{
		size_t mid = lo + (hi - lo) / 2;

		if (fragment_index_cmp(&cursor->index[mid], &frag) < 0)
			lo = mid + 1;
		else
			hi = mid;
	}

	memmove(&cursor->index[lo + 1], &cursor->index[lo], (cursor->count - lo) * sizeof(void *));
	cursor->index[lo] = frag;
	cursor->count++;

	return true;
}

static void
fragment_index_remove(pkgconf_fragment_cursor_t *cursor, const pkgconf_fragment_t *frag)
{
	pkgconf_fragment_t **found = bsearch(&frag, cursor->index, cursor->count, sizeof(void *), fragment_index_cmp);
	size_t idx;

	if (found == NULL)
		return;

	idx = (size_t) (found - cursor->index);
	memmove(&cursor->index[idx], &cursor->index[idx + 1], (cursor->count - idx - 1) * sizeof(void *));
	cursor->count--;
}

/* Look up an existing fragment matching `base`: via the cursor's sorted index
 * when one is provided, otherwise a linear scan of the list. */
static inline pkgconf_fragment_t *
fragment_lookup(pkgconf_list_t *list, const pkgconf_fragment_cursor_t *cursor, const pkgconf_fragment_t *base)
{
	if (cursor != NULL)
		return fragment_index_lookup(cursor, base);

	return pkgconf_fragment_lookup(list, base);
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

	if (base->data == NULL)
		return false;

	return pkgconf_fragment_is_unmergeable(base->data);
}

static inline pkgconf_fragment_t *
pkgconf_fragment_exists(pkgconf_list_t *list, const pkgconf_fragment_cursor_t *cursor, const pkgconf_fragment_t *base, unsigned int flags, bool is_private)
{
	if (!pkgconf_fragment_can_merge_back(base, flags, is_private))
		return NULL;

	if (!pkgconf_fragment_can_merge(base, flags, is_private))
		return NULL;

	return fragment_lookup(list, cursor, base);
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

static bool
pkgconf_fragment_copy_node(const pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_fragment_cursor_t *cursor, const pkgconf_fragment_t *base, bool is_private);

static bool
pkgconf_fragment_copy_list_node(const pkgconf_client_t *client, pkgconf_list_t *list, const pkgconf_list_t *base)
{
	pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY(base->head, node)
	{
		pkgconf_fragment_t *frag = node->data;

		/* children are copied into their own (small) list, which is never
		 * indexed, so no cursor is threaded down here. */
		if (!pkgconf_fragment_copy_node(client, list, NULL, frag, true))
			return false;
	}

	return true;
}

static bool
pkgconf_fragment_copy_node(const pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_fragment_cursor_t *cursor, const pkgconf_fragment_t *base, bool is_private)
{
	pkgconf_fragment_t *old_frag = NULL;
	pkgconf_fragment_t *frag;

	if ((old_frag = pkgconf_fragment_exists(list, cursor, base, client->flags, is_private)) != NULL)
	{
		if (!pkgconf_fragment_should_merge(old_frag))
			old_frag = NULL;
	}
	else if (!is_private && !pkgconf_fragment_can_merge_back(base, client->flags, is_private) && (fragment_lookup(list, cursor, base) != NULL))
		return true;

	frag = fragment_new(base->type, base->data);
	if (frag == NULL)
		return false;

	if (!pkgconf_fragment_copy_list_node(client, &frag->children, &base->children))
	{
		pkgconf_fragment_free(&frag->children);
		free(frag);
		return false;
	}

	if (old_frag != NULL)
	{
		if (cursor != NULL)
			fragment_index_remove(cursor, old_frag);

		pkgconf_fragment_delete(list, old_frag);
	}

	pkgconf_node_insert_tail(&frag->iter, frag, list);

	if (cursor != NULL && !fragment_index_insert(cursor, frag))
		return false;

	return true;
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
	(void) pkgconf_fragment_copy_node(client, list, NULL, base, is_private);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_fragment_cursor_init(pkgconf_fragment_cursor_t *cursor, pkgconf_list_t *list)
 *
 *    Initialises a `fragment cursor` bound to a (typically empty) destination list.  While the
 *    cursor is in use, fragments must be added to the list only via ``pkgconf_fragment_copy_cursor()``
 *    so that the cursor's index stays in sync.
 *
 *    :param pkgconf_fragment_cursor_t* cursor: The cursor to initialise.
 *    :param pkgconf_list_t* list: The destination fragment list.
 *    :return: nothing
 */
void
pkgconf_fragment_cursor_init(pkgconf_fragment_cursor_t *cursor, pkgconf_list_t *list)
{
	pkgconf_node_t *node;

	cursor->list = list;
	cursor->index = NULL;
	cursor->count = 0;
	cursor->alloc = 0;

	/* seed the index with anything already present, so dedup against pre-existing
	 * fragments behaves exactly as the linear scan would. */
	PKGCONF_FOREACH_LIST_ENTRY(list->head, node)
		(void) fragment_index_insert(cursor, node->data);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_fragment_cursor_deinit(pkgconf_fragment_cursor_t *cursor)
 *
 *    Releases the index held by a `fragment cursor`.  The destination list and its fragments are
 *    not affected.
 *
 *    :param pkgconf_fragment_cursor_t* cursor: The cursor to release.
 *    :return: nothing
 */
void
pkgconf_fragment_cursor_deinit(pkgconf_fragment_cursor_t *cursor)
{
	free(cursor->index);
	cursor->index = NULL;
	cursor->count = 0;
	cursor->alloc = 0;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_fragment_copy_cursor(const pkgconf_client_t *client, pkgconf_fragment_cursor_t *cursor, const pkgconf_fragment_t *base, bool is_private)
 *
 *    Like ``pkgconf_fragment_copy()``, but uses the cursor's sorted index for the mergeback lookup,
 *    turning what would be a linear scan of the destination list into a bsearch.
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param pkgconf_fragment_cursor_t* cursor: The cursor bound to the destination list.
 *    :param pkgconf_fragment_t* base: The fragment being copied.
 *    :param bool is_private: Whether the fragment list is a `private` fragment list (static linking).
 *    :return: nothing
 */
void
pkgconf_fragment_copy_cursor(const pkgconf_client_t *client, pkgconf_fragment_cursor_t *cursor, const pkgconf_fragment_t *base, bool is_private)
{
	(void) pkgconf_fragment_copy_node(client, cursor->list, cursor, base, is_private);
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
	(void) pkgconf_fragment_copy_list_node(client, list, base);
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
			(void) pkgconf_fragment_copy_node(client, dest, NULL, frag, true);
	}
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_fragment_filter_splice(const pkgconf_client_t *client, pkgconf_list_t *dest, pkgconf_list_t *src, pkgconf_fragment_filter_func_t filter_func, void *data)
 *
 *    Like ``pkgconf_fragment_filter()``, but moves the matching `fragments` to `dest` instead of
 *    copying them.  Fragments which do not match are left in `src` for the caller to release.
 *
 *    :param pkgconf_client_t* client: The pkgconf client being accessed.
 *    :param pkgconf_list_t* dest: The destination list.
 *    :param pkgconf_list_t* src: The source list.
 *    :param pkgconf_fragment_filter_func_t filter_func: The filter function to use.
 *    :param void* data: Optional data to pass to the filter function.
 *    :return: nothing
 */
void
pkgconf_fragment_filter_splice(const pkgconf_client_t *client, pkgconf_list_t *dest, pkgconf_list_t *src, pkgconf_fragment_filter_func_t filter_func, void *data)
{
	pkgconf_node_t *node, *next;

	if (dest == src)
		return;

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(src->head, next, node)
	{
		pkgconf_fragment_t *frag = node->data;

		if (!filter_func(client, frag, data))
			continue;

		pkgconf_node_delete(node, src);

		/* pkgconf_node_delete() leaves the node's links pointing into the
		 * source list, and pkgconf_node_insert_tail() only overwrites prev. */
		node->prev = node->next = NULL;

		pkgconf_node_insert_tail(node, frag, dest);
	}
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_is_locale_utf8(void)
 *
 *    Check whether text is expected to be UTF-8 encoded in the current environment.
 *
 *    :return: :code:`true` if text is expected to be UTF-8 encoded, :code:`false` otherwise.
 */
#ifndef _WIN32
static bool
codeset_is_utf8(const char *codeset)
{
	return codeset != NULL && (!strcasecmp(codeset, "UTF-8") || !strcasecmp(codeset, "UTF8"));
}
#endif

bool
pkgconf_is_locale_utf8(void)
{
#ifdef _WIN32
	return GetACP() == CP_UTF8;
#else
	static int cached = -1;

	if (cached >= 0)
		return cached;

#if HAVE_DECL_NL_LANGINFO_L
	locale_t loc = newlocale(LC_CTYPE_MASK, "", (locale_t)0);

	if (loc != (locale_t)0)
	{
		cached = codeset_is_utf8(nl_langinfo_l(CODESET, loc));
		freelocale(loc);
	}
	else
		cached = 0;
#else
	const char *prev_locale = setlocale(LC_CTYPE, NULL);
	char *saved_locale = prev_locale != NULL ? strdup(prev_locale) : NULL;

	setlocale(LC_CTYPE, "");
	cached = codeset_is_utf8(nl_langinfo(CODESET));
	setlocale(LC_CTYPE, saved_locale != NULL ? saved_locale : "C");
	free(saved_locale);
#endif

	return cached;
#endif
}

static const pkgconf_span_t quote_spans[] = {
	{ 0x00, 0x1f },
	{ (unsigned char)' ', (unsigned char)'#' },
	{ (unsigned char)'%', (unsigned char)'\'' },
	{ (unsigned char)'*', (unsigned char)'*' },
	{ (unsigned char)';', (unsigned char)'<' },
	{ (unsigned char)'>', (unsigned char)'?' },
	{ (unsigned char)'[', (unsigned char)']' },
	{ (unsigned char)'`', (unsigned char)'`' },
	{ (unsigned char)'{', (unsigned char)'}' },
	{ 0x7f, 0xff },
};

/* If the locale is UTF-8 we must not split character over 0x7f because it would add "\" between each bytes.
   So only DEL (0x7f) needs escaping */
static const pkgconf_span_t quote_spans_utf8[] = {
	{ 0x00, 0x1f },
	{ (unsigned char)' ', (unsigned char)'#' },
	{ (unsigned char)'%', (unsigned char)'\'' },
	{ (unsigned char)'*', (unsigned char)'*' },
	{ (unsigned char)';', (unsigned char)'<' },
	{ (unsigned char)'>', (unsigned char)'?' },
	{ (unsigned char)'[', (unsigned char)']' },
	{ (unsigned char)'`', (unsigned char)'`' },
	{ (unsigned char)'{', (unsigned char)'}' },
	{ 0x7f, 0x7f },
};

/*
 * The set of bytes needing a backslash is fixed for the life of the process
 * (pkgconf_is_locale_utf8() is itself cached), so resolve the spans into a
 * byte-set once instead of walking them for every byte of every fragment.
 */
static const pkgconf_charset_t *
fragment_quote_charset(void)
{
	static pkgconf_charset_t charset;
	static bool have_charset = false;

	if (!have_charset)
	{
		if (pkgconf_is_locale_utf8())
			pkgconf_charset_from_spans(&charset, quote_spans_utf8, PKGCONF_ARRAY_SIZE(quote_spans_utf8));
		else
			pkgconf_charset_from_spans(&charset, quote_spans, PKGCONF_ARRAY_SIZE(quote_spans));

		have_charset = true;
	}

	return &charset;
}

static bool
fragment_quote(pkgconf_buffer_t *out, const pkgconf_fragment_t *frag)
{
	if (frag->data == NULL)
		return true;

	return pkgconf_buffer_escape_charset(out, PKGCONF_BUFFER_FROM_STR(frag->data), fragment_quote_charset());
}

static bool
fragment_render(const pkgconf_fragment_render_ctx_t *ctx, const pkgconf_fragment_t *frag, pkgconf_buffer_t *buf)
{
	const pkgconf_node_t *iter;

	if (frag->type &&
		(!pkgconf_buffer_push_byte(buf, '-') || !pkgconf_buffer_push_byte(buf, frag->type)))
		return false;

	if (!fragment_quote(buf, frag))
		return false;

	PKGCONF_FOREACH_LIST_ENTRY(frag->children.head, iter)
	{
		const pkgconf_fragment_t *child_frag = iter->data;

		if (!pkgconf_buffer_push_byte(buf, ctx->delim) ||
			!fragment_render(ctx, child_frag, buf))
			return false;
	}

	return true;
}

static const pkgconf_fragment_render_ops_t default_render_ops = {
	.render = fragment_render
};

/*
 * !doc
 *
 * .. c:function:: void pkgconf_fragment_render_buf(const pkgconf_list_t *list, char *buf, size_t buflen, bool escape, const pkgconf_fragment_render_ops_t *ops, char delim)
 *
 *    Renders a `fragment list` into a buffer.
 *
 *    :param pkgconf_list_t* list: The `fragment list` being rendered.
 *    :param pkgconf_buffer_t* buf: The buffer to render the fragment list into.
 *    :param bool escape: Whether or not to escape special shell characters (deprecated).
 *    :param pkgconf_fragment_render_ops_t* ops: An optional ops structure to use for custom renderers, else ``NULL``.
 *    :param char delim: The delimiter to use between fragments.
 *    :return: nothing
 */
bool
pkgconf_fragment_render_buf(const pkgconf_list_t *list, pkgconf_buffer_t *buf, bool escape, const pkgconf_fragment_render_ops_t *ops, char delim)
{
	pkgconf_node_t *node;
	pkgconf_fragment_render_ctx_t ctx = {
		.escape = escape,
		.delim = delim,
	};

	ops = ops != NULL ? ops : &default_render_ops;

	PKGCONF_FOREACH_LIST_ENTRY(list->head, node)
	{
		const pkgconf_fragment_t *frag = node->data;
		if (!ops->render(&ctx, frag, buf))
			return false;

		if (node->next != NULL && !pkgconf_buffer_push_byte(buf, ctx.delim))
			return false;
	}

	return true;
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

	pkgconf_fragment_free(&node->children);
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
		free(frag);
	}

	pkgconf_list_zero(list);
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
pkgconf_fragment_parse(pkgconf_client_t *client, pkgconf_list_t *list, pkgconf_list_t *vars, const char *value, unsigned int flags)
{
	return fragment_split(client, list, vars, value, flags, true);
}
