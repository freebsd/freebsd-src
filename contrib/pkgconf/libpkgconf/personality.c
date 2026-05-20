/*
 * personality.c
 * libpkgconf cross-compile personality database
 *
 * Copyright (c) 2018 pkgconf authors (see AUTHORS).
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

/*
 * !doc
 *
 * libpkgconf `personality` module
 * =========================
 */

#ifdef _WIN32
#	define strcasecmp _stricmp
#endif

/*
 * Increment each time the default personality is inited, decrement each time
 * it's deinited. Whenever it is 0, then the deinit frees the personality. In
 * that case an additional call to init will create it anew.
 */
static unsigned default_personality_init = 0;

static pkgconf_cross_personality_t default_personality = {
	.name = "default",
};

static inline void
build_default_search_path(pkgconf_list_t* dirlist)
{
#ifdef _WIN32
	char namebuf[MAX_PATH];
	char outbuf[MAX_PATH];
	char *p;

	int sizepath = GetModuleFileName(NULL, namebuf, sizeof namebuf);
	char * winslash;
	namebuf[sizepath] = '\0';
	while ((winslash = strchr (namebuf, '\\')) != NULL)
	{
		*winslash = '/';
	}
	p = strrchr(namebuf, '/');
	if (p == NULL)
		pkgconf_path_split(PKG_DEFAULT_PATH, dirlist, true);

	*p = '\0';
	pkgconf_strlcpy(outbuf, namebuf, sizeof outbuf);
	pkgconf_strlcat(outbuf, "/", sizeof outbuf);
	pkgconf_strlcat(outbuf, "../lib/pkgconfig", sizeof outbuf);
	pkgconf_path_add(outbuf, dirlist, true);
	pkgconf_strlcpy(outbuf, namebuf, sizeof outbuf);
	pkgconf_strlcat(outbuf, "/", sizeof outbuf);
	pkgconf_strlcat(outbuf, "../share/pkgconfig", sizeof outbuf);
	pkgconf_path_add(outbuf, dirlist, true);
#elif __HAIKU__
	char **paths;
	size_t count;
	if (find_paths(B_FIND_PATH_DEVELOP_LIB_DIRECTORY, "pkgconfig", &paths, &count) == B_OK) {
		for (size_t i = 0; i < count; i++)
			pkgconf_path_add(paths[i], dirlist, true);
		free(paths);
		paths = NULL;
	}
	if (find_paths(B_FIND_PATH_DATA_DIRECTORY, "pkgconfig", &paths, &count) == B_OK) {
		for (size_t i = 0; i < count; i++)
			pkgconf_path_add(paths[i], dirlist, true);
		free(paths);
		paths = NULL;
	}
#else
	pkgconf_path_split(PKG_DEFAULT_PATH, dirlist, true);
#endif
}

/*
 * !doc
 *
 * .. c:function:: const pkgconf_cross_personality_t *pkgconf_cross_personality_default(void)
 *
 *    Returns the default cross-compile personality.
 *
 *    Not thread safe.
 *
 *    :rtype: pkgconf_cross_personality_t*
 *    :return: the default cross-compile personality
 */
pkgconf_cross_personality_t *
pkgconf_cross_personality_default(void)
{
	if (default_personality_init) {
		++default_personality_init;
		return &default_personality;
	}

	build_default_search_path(&default_personality.dir_list);

	pkgconf_path_split(SYSTEM_LIBDIR, &default_personality.filter_libdirs, false);
	pkgconf_path_split(SYSTEM_INCLUDEDIR, &default_personality.filter_includedirs, false);

	++default_personality_init;
	return &default_personality;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_cross_personality_deinit(pkgconf_cross_personality_t *)
 *
 *    Destroys a cross personality object and/or decreases the reference count on the
 *    default cross personality object.
 *
 *    Not thread safe.
 *
 *    :rtype: void
 */
void
pkgconf_cross_personality_deinit(pkgconf_cross_personality_t *personality)
{
	/* allow NULL parameter for API backwards compatibility */
	if (personality == NULL)
		return;

	/* XXX: this hack is rather ugly, but it works for now... */
	if (personality == &default_personality && --default_personality_init > 0)
		return;

	pkgconf_path_free(&personality->dir_list);
	pkgconf_path_free(&personality->filter_libdirs);
	pkgconf_path_free(&personality->filter_includedirs);

	if (personality->sysroot_dir != NULL)
		free(personality->sysroot_dir);

	if (personality == &default_personality)
		return;

	if (personality->name != NULL)
		free(personality->name);

	free(personality);
}

#ifndef PKGCONF_LITE
static bool
valid_triplet(const char *triplet)
{
	const char *c = triplet;

	for (; *c; c++)
		if (!isalnum((unsigned char)*c) && *c != '-' && *c != '_')
			return false;

	return true;
}

typedef void (*personality_keyword_func_t)(pkgconf_cross_personality_t *p, const char *keyword, const size_t lineno, const ptrdiff_t offset, char *value);
typedef struct {
	const char *keyword;
	const personality_keyword_func_t func;
	const ptrdiff_t offset;
} personality_keyword_pair_t;

static void
personality_bool_func(pkgconf_cross_personality_t *p, const char *keyword, const size_t lineno, const ptrdiff_t offset, char *value)
{
	(void) keyword;
	(void) lineno;

	bool *dest = (bool *)((char *) p + offset);
	*dest = strcasecmp(value, "true") || strcasecmp(value, "yes") || *value == '1';
}

static void
personality_copy_func(pkgconf_cross_personality_t *p, const char *keyword, const size_t lineno, const ptrdiff_t offset, char *value)
{
	(void) keyword;
	(void) lineno;

	char **dest = (char **)((char *) p + offset);
	*dest = strdup(value);
}

static void
personality_fragment_func(pkgconf_cross_personality_t *p, const char *keyword, const size_t lineno, const ptrdiff_t offset, char *value)
{
	(void) keyword;
	(void) lineno;

	pkgconf_list_t *dest = (pkgconf_list_t *)((char *) p + offset);
	pkgconf_path_split(value, dest, false);
}

/* keep in alphabetical order! */
static const personality_keyword_pair_t personality_keyword_pairs[] = {
	{"DefaultSearchPaths", personality_fragment_func, offsetof(pkgconf_cross_personality_t, dir_list)},
	{"SysrootDir", personality_copy_func, offsetof(pkgconf_cross_personality_t, sysroot_dir)},
	{"SystemIncludePaths", personality_fragment_func, offsetof(pkgconf_cross_personality_t, filter_includedirs)},
	{"SystemLibraryPaths", personality_fragment_func, offsetof(pkgconf_cross_personality_t, filter_libdirs)},
	{"Triplet", personality_copy_func, offsetof(pkgconf_cross_personality_t, name)},
	{"WantDefaultPure", personality_bool_func, offsetof(pkgconf_cross_personality_t, want_default_pure)},
	{"WantDefaultStatic", personality_bool_func, offsetof(pkgconf_cross_personality_t, want_default_static)},
};

static int
personality_keyword_pair_cmp(const void *key, const void *ptr)
{
	const personality_keyword_pair_t *pair = ptr;
	return strcasecmp(key, pair->keyword);
}

static void
personality_keyword_set(pkgconf_cross_personality_t *p, const size_t lineno, const char *keyword, char *value)
{
	const personality_keyword_pair_t *pair = bsearch(keyword,
		personality_keyword_pairs, PKGCONF_ARRAY_SIZE(personality_keyword_pairs),
		sizeof(personality_keyword_pair_t), personality_keyword_pair_cmp);

	if (pair == NULL || pair->func == NULL)
		return;

	pair->func(p, keyword, lineno, pair->offset, value);
}

static const pkgconf_parser_operand_func_t personality_parser_ops[256] = {
	[':'] = (pkgconf_parser_operand_func_t) personality_keyword_set
};

static void personality_warn_func(void *p, const char *fmt, ...) PRINTFLIKE(2, 3);

static void
personality_warn_func(void *p, const char *fmt, ...)
{
	va_list va;

	(void) p;

	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
}

static pkgconf_cross_personality_t *
load_personality_with_path(const char *path, const char *triplet, bool datadir)
{
	char pathbuf[PKGCONF_ITEM_SIZE];
	FILE *f;
	pkgconf_cross_personality_t *p;

	/* if triplet is null, assume that path is a direct path to the personality file */
	if (triplet == NULL)
		pkgconf_strlcpy(pathbuf, path, sizeof pathbuf);
	else if (datadir)
		snprintf(pathbuf, sizeof pathbuf, "%s/pkgconfig/personality.d/%s.personality", path, triplet);
	else
		snprintf(pathbuf, sizeof pathbuf, "%s/%s.personality", path, triplet);

	p = calloc(1, sizeof(pkgconf_cross_personality_t));
	if (p == NULL)
		return NULL;

	if (triplet != NULL)
		p->name = strdup(triplet);

	f = fopen(pathbuf, "r");
	if (f == NULL) {
		pkgconf_cross_personality_deinit(p);
		return NULL;
	}

	pkgconf_parser_parse(f, p, personality_parser_ops, personality_warn_func, pathbuf);

	return p;
}

/*
 * !doc
 *
 * .. c:function:: pkgconf_cross_personality_t *pkgconf_cross_personality_find(const char *triplet)
 *
 *    Attempts to find a cross-compile personality given a triplet.
 *
 *    :rtype: pkgconf_cross_personality_t*
 *    :return: the default cross-compile personality
 */
pkgconf_cross_personality_t *
pkgconf_cross_personality_find(const char *triplet)
{
	pkgconf_list_t plist = PKGCONF_LIST_INITIALIZER;
	pkgconf_node_t *n;
	pkgconf_cross_personality_t *out = NULL;
#if ! defined(_WIN32) && ! defined(__HAIKU__)
	char pathbuf[PKGCONF_ITEM_SIZE];
	const char *envvar;
#endif

	out = load_personality_with_path(triplet, NULL, false);
	if (out != NULL)
		return out;

	if (!valid_triplet(triplet))
		return NULL;

#if ! defined(_WIN32) && ! defined(__HAIKU__)
	envvar = getenv("XDG_DATA_HOME");
	if (envvar != NULL)
		pkgconf_path_add(envvar, &plist, true);
	else {
		envvar = getenv("HOME");
		if (envvar != NULL) {
			pkgconf_strlcpy(pathbuf, envvar, sizeof pathbuf);
			pkgconf_strlcat(pathbuf, "/.local/share", sizeof pathbuf);
			pkgconf_path_add(pathbuf, &plist, true);
		}
	}

	pkgconf_path_build_from_environ("XDG_DATA_DIRS", "/usr/local/share" PKG_CONFIG_PATH_SEP_S "/usr/share", &plist, true);

	PKGCONF_FOREACH_LIST_ENTRY(plist.head, n)
	{
		pkgconf_path_t *pn = n->data;

		out = load_personality_with_path(pn->path, triplet, true);
		if (out != NULL)
			goto finish;
	}
	pkgconf_path_free(&plist);
#endif

	pkgconf_path_split(PERSONALITY_PATH, &plist, true);

	PKGCONF_FOREACH_LIST_ENTRY(plist.head, n)
	{
		pkgconf_path_t *pn = n->data;

		out = load_personality_with_path(pn->path, triplet, false);
		if (out != NULL)
			goto finish;
	}

finish:
	pkgconf_path_free(&plist);
	return out != NULL ? out : pkgconf_cross_personality_default();
}
#endif
