/*-
 * Copyright (c) 2003-2006, Maxime Henrion <mux@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "globtree.h"
#include "keyword.h"
#include "misc.h"
#include "parse.h"
#include "stream.h"
#include "token.h"

static int		 config_parse_refusefiles(struct coll *);
static int		 config_parse_refusefile(struct coll *, char *);

extern FILE *yyin;

/* These are globals because I can't think of a better way with yacc. */
static STAILQ_HEAD(, coll) colls;
static struct coll *cur_coll;
static struct coll *defaults;
static struct coll *ovcoll;
static int ovmask;
static const char *cfgfile;

/*
 * Extract all the configuration information from the config
 * file and some command line parameters.
 */
struct config *
config_init(const char *file, struct coll *override, int overridemask)
{
	struct config *config;
	struct coll *coll;
	size_t slen;
	char *prefix;
	int error;
	mode_t mask;

	config = xmalloc(sizeof(struct config));
	memset(config, 0, sizeof(struct config));
	STAILQ_INIT(&colls);

	defaults = coll_new(NULL);
	/* Set the default umask. */
	mask = umask(0);
	umask(mask);
	defaults->co_umask = mask;
	ovcoll = override;
	ovmask = overridemask;

	/* Extract a list of collections from the configuration file. */
	cur_coll = coll_new(defaults);
	yyin = fopen(file, "r");
	if (yyin == NULL) {
		lprintf(-1, "Cannot open \"%s\": %s\n", file, strerror(errno));
		goto bad;
	}
	cfgfile = file;
	error = yyparse();
	fclose(yyin);
	if (error)
		goto bad;

	memcpy(&config->colls, &colls, sizeof(colls));
	if (STAILQ_EMPTY(&config->colls)) {
		lprintf(-1, "Empty supfile\n");
		goto bad;
	}

	/* Fixup the list of collections. */
	STAILQ_FOREACH(coll, &config->colls, co_next) {
 		if (coll->co_base == NULL)
			coll->co_base = xstrdup("/usr/local/etc/cvsup");
		if (coll->co_colldir == NULL)
			coll->co_colldir = "sup";
		if (coll->co_prefix == NULL) {
			coll->co_prefix = xstrdup(coll->co_base);
		/*
		 * If prefix is not an absolute pathname, it is
		 * interpreted relative to base.
		 */
		} else if (coll->co_prefix[0] != '/') {
			slen = strlen(coll->co_base);
			if (slen > 0 && coll->co_base[slen - 1] != '/')
				xasprintf(&prefix, "%s/%s", coll->co_base,
				    coll->co_prefix);
			else
				xasprintf(&prefix, "%s%s", coll->co_base,
				    coll->co_prefix);
			free(coll->co_prefix);
			coll->co_prefix = prefix;
		}
		coll->co_prefixlen = strlen(coll->co_prefix);
		/* Determine whether to checksum RCS files or not. */
		if (coll->co_options & CO_EXACTRCS)
			coll->co_options |= CO_CHECKRCS;
		else
			coll->co_options &= ~CO_CHECKRCS;
		/* In recent versions, we always try to set the file modes. */
		coll->co_options |= CO_SETMODE;
		coll->co_options |= CO_NORSYNC;
		error = config_parse_refusefiles(coll);
		if (error)
			goto bad;
	}

	coll_free(cur_coll);
	coll_free(defaults);
	config->host = STAILQ_FIRST(&config->colls)->co_host;
	return (config);
bad:
	coll_free(cur_coll);
	coll_free(defaults);
	config_free(config);
	return (NULL);
}

int
config_checkcolls(struct config *config)
{
	char linkname[4];
	struct stat sb;
	struct coll *coll;
	int error, numvalid, ret;

	numvalid = 0;
	STAILQ_FOREACH(coll, &config->colls, co_next) {
		error = stat(coll->co_prefix, &sb);
		if (error || !S_ISDIR(sb.st_mode)) {
			/* Skip this collection, and warn about it unless its
			   prefix is a symbolic link pointing to "SKIP". */
			coll->co_options |= CO_SKIP;
			ret = readlink(coll->co_prefix, linkname,
			    sizeof(linkname));
			if (ret != 4 || memcmp(linkname, "SKIP", 4) != 0) {
				lprintf(-1,"Nonexistent prefix \"%s\" for "
				    "%s/%s\n", coll->co_prefix, coll->co_name,
				    coll->co_release);
			}
			continue;
		}
		numvalid++;
	}
	return (numvalid);
}

static int
config_parse_refusefiles(struct coll *coll)
{
	char *collstem, *suffix, *supdir, *path;
	int error;

	if (coll->co_colldir[0] == '/')
		supdir = xstrdup(coll->co_colldir);
	else
		xasprintf(&supdir, "%s/%s", coll->co_base, coll->co_colldir);

	/* First, the global refuse file that applies to all collections. */
	xasprintf(&path, "%s/refuse", supdir);
	error = config_parse_refusefile(coll, path);
	free(path);
	if (error) {
		free(supdir);
		return (error);
	}

	/* Next the per-collection refuse files that applies to all release/tag
	   combinations. */
	xasprintf(&collstem, "%s/%s/refuse", supdir, coll->co_name);
	free(supdir);
	error = config_parse_refusefile(coll, collstem);
	if (error) {
		free(collstem);
		return (error);
	}

	/* Finally, the per-release and per-tag refuse file. */
	suffix = coll_statussuffix(coll);
	if (suffix != NULL) {
		xasprintf(&path, "%s%s", collstem, suffix);
		free(suffix);
		error = config_parse_refusefile(coll, path);
		free(path);
	}
	free(collstem);
	return (error);
}

/*
 * Parses a "refuse" file, and records the relevant information in
 * coll->co_refusals.  If the file does not exist, it is silently
 * ignored.
 */
static int
config_parse_refusefile(struct coll *coll, char *path)
{
	struct stream *rd;
	char *cp, *line, *pat;

	rd = stream_open_file(path, O_RDONLY);
	if (rd == NULL)
		return (0);
	while ((line = stream_getln(rd, NULL)) != NULL) {
		pat = line;
		for (;;) {
			/* Trim leading whitespace. */
			pat += strspn(pat, " \t");
			if (pat[0] == '\0')
				break;
			cp = strpbrk(pat, " \t");
			if (cp != NULL)
				*cp = '\0';
			pattlist_add(coll->co_refusals, pat);
			if (cp == NULL)
				break;
			pat = cp + 1;
		}
	}
	if (!stream_eof(rd)) {
		stream_close(rd);
		lprintf(-1, "Read failure from \"%s\": %s\n", path,
		    strerror(errno));
		return (-1);
	}
	stream_close(rd);
	return (0);
}

void
config_free(struct config *config)
{
	struct coll *coll;

	while (!STAILQ_EMPTY(&config->colls)) {
		coll = STAILQ_FIRST(&config->colls);
		STAILQ_REMOVE_HEAD(&config->colls, co_next);
		coll_free(coll);
	}
	if (config->server != NULL)
		stream_close(config->server);
	if (config->laddr != NULL)
		free(config->laddr);
	free(config);
}

/* Create a new collection, inheriting options from the default collection. */
struct coll *
coll_new(struct coll *def)
{
	struct coll *new;

	new = xmalloc(sizeof(struct coll));
	memset(new, 0, sizeof(struct coll));
	if (def != NULL) {
		new->co_options = def->co_options;
		new->co_umask = def->co_umask;
		if (def->co_host != NULL)
			new->co_host = xstrdup(def->co_host);
		if (def->co_base != NULL)
			new->co_base = xstrdup(def->co_base);
		if (def->co_date != NULL)
			new->co_date = xstrdup(def->co_date);
		if (def->co_prefix != NULL)
			new->co_prefix = xstrdup(def->co_prefix);
		if (def->co_release != NULL)
			new->co_release = xstrdup(def->co_release);
		if (def->co_tag != NULL)
			new->co_tag = xstrdup(def->co_tag);
		if (def->co_listsuffix != NULL)
			new->co_listsuffix = xstrdup(def->co_listsuffix);
	} else {
		new->co_tag = xstrdup(".");
		new->co_date = xstrdup(".");
	}
	new->co_keyword = keyword_new();
	new->co_accepts = pattlist_new();
	new->co_refusals = pattlist_new();
	new->co_attrignore = FA_DEV | FA_INODE;
	return (new);
}

void
coll_override(struct coll *coll, struct coll *from, int mask)
{
	size_t i;
	int newoptions, oldoptions;

	newoptions = from->co_options & mask;
	oldoptions = coll->co_options & (CO_MASK & ~mask);

	if (from->co_release != NULL) {
		if (coll->co_release != NULL)
			free(coll->co_release);
		coll->co_release = xstrdup(from->co_release);
	}
	if (from->co_host != NULL) {
		if (coll->co_host != NULL)
			free(coll->co_host);
		coll->co_host = xstrdup(from->co_host);
	}
	if (from->co_base != NULL) {
		if (coll->co_base != NULL)
			free(coll->co_base);
		coll->co_base = xstrdup(from->co_base);
	}
	if (from->co_colldir != NULL)
		coll->co_colldir = from->co_colldir;
	if (from->co_prefix != NULL) {
		if (coll->co_prefix != NULL)
			free(coll->co_prefix);
		coll->co_prefix = xstrdup(from->co_prefix);
	}
	if (newoptions & CO_CHECKOUTMODE) {
		if (from->co_tag != NULL) {
			if (coll->co_tag != NULL)
				free(coll->co_tag);
			coll->co_tag = xstrdup(from->co_tag);
		}
		if (from->co_date != NULL) {
			if (coll->co_date != NULL)
				free(coll->co_date);
			coll->co_date = xstrdup(from->co_date);
		}
	}
	if (from->co_listsuffix != NULL) {
		if (coll->co_listsuffix != NULL)
			free(coll->co_listsuffix);
		coll->co_listsuffix = xstrdup(from->co_listsuffix);
	}
	for (i = 0; i < pattlist_size(from->co_accepts); i++) {
		pattlist_add(coll->co_accepts,
		    pattlist_get(from->co_accepts, i));
	}
	for (i = 0; i < pattlist_size(from->co_refusals); i++) {
		pattlist_add(coll->co_refusals,
		    pattlist_get(from->co_refusals, i));
	}
	coll->co_options = oldoptions | newoptions;
}

char *
coll_statussuffix(struct coll *coll)
{
	const char *tag;
	char *suffix;

	if (coll->co_listsuffix != NULL) {
		xasprintf(&suffix, ".%s", coll->co_listsuffix);
	} else if (coll->co_options & CO_USERELSUFFIX) {
		if (coll->co_tag == NULL)
			tag = ".";
		else
			tag = coll->co_tag;
		if (coll->co_release != NULL) {
			if (coll->co_options & CO_CHECKOUTMODE) {
				xasprintf(&suffix, ".%s:%s",
				    coll->co_release, tag);
			} else {
				xasprintf(&suffix, ".%s", coll->co_release);
			}
		} else if (coll->co_options & CO_CHECKOUTMODE) {
			xasprintf(&suffix, ":%s", tag);
		}
	} else
		suffix = NULL;
	return (suffix);
}

char *
coll_statuspath(struct coll *coll)
{
	char *path, *suffix;

	suffix = coll_statussuffix(coll);
	if (suffix != NULL) {
		if (coll->co_colldir[0] == '/')
			xasprintf(&path, "%s/%s/checkouts%s", coll->co_colldir,
			    coll->co_name, suffix);
		else
			xasprintf(&path, "%s/%s/%s/checkouts%s", coll->co_base,
			    coll->co_colldir, coll->co_name, suffix);
	} else {
		if (coll->co_colldir[0] == '/')
			xasprintf(&path, "%s/%s/checkouts", coll->co_colldir,
			    coll->co_name);
		else
			xasprintf(&path, "%s/%s/%s/checkouts", coll->co_base,
			    coll->co_colldir, coll->co_name);
	}
	free(suffix);
	return (path);
}

void
coll_add(char *name)
{
	struct coll *coll;

	cur_coll->co_name = name;
	coll_override(cur_coll, ovcoll, ovmask);
	if (cur_coll->co_release == NULL) {
		lprintf(-1, "Release not specified for collection "
		    "\"%s\"\n", cur_coll->co_name);
		exit(1);
	}
	if (cur_coll->co_host == NULL) {
		lprintf(-1, "Host not specified for collection "
		    "\"%s\"\n", cur_coll->co_name);
		exit(1);
	}
	if (!STAILQ_EMPTY(&colls)) {
		coll = STAILQ_LAST(&colls, coll, co_next);
		if (strcmp(coll->co_host, cur_coll->co_host) != 0) {
			lprintf(-1, "All \"host\" fields in the supfile "
			    "must be the same\n");
			exit(1);
		}
	}
	STAILQ_INSERT_TAIL(&colls, cur_coll, co_next);
	cur_coll = coll_new(defaults);
}

void
coll_free(struct coll *coll)
{

	if (coll == NULL)
		return;
	if (coll->co_host != NULL)
		free(coll->co_host);
	if (coll->co_base != NULL)
		free(coll->co_base);
	if (coll->co_date != NULL)
		free(coll->co_date);
	if (coll->co_prefix != NULL)
		free(coll->co_prefix);
	if (coll->co_release != NULL)
		free(coll->co_release);
	if (coll->co_tag != NULL)
		free(coll->co_tag);
	if (coll->co_cvsroot != NULL)
		free(coll->co_cvsroot);
	if (coll->co_name != NULL)
		free(coll->co_name);
	if (coll->co_listsuffix != NULL)
		free(coll->co_listsuffix);
	keyword_free(coll->co_keyword);
	if (coll->co_dirfilter != NULL)
		globtree_free(coll->co_dirfilter);
	if (coll->co_dirfilter != NULL)
		globtree_free(coll->co_filefilter);
	if (coll->co_norsync != NULL)
		globtree_free(coll->co_norsync);
	if (coll->co_accepts != NULL)
		pattlist_free(coll->co_accepts);
	if (coll->co_refusals != NULL)
		pattlist_free(coll->co_refusals);
	free(coll);
}

void
coll_setopt(int opt, char *value)
{
	struct coll *coll;
	int error, mask;

	coll = cur_coll;
	switch (opt) {
	case PT_HOST:
		if (coll->co_host != NULL)
			free(coll->co_host);
		coll->co_host = value;
		break;
	case PT_BASE:
		if (coll->co_base != NULL)
			free(coll->co_base);
		coll->co_base = value;
		break;
	case PT_DATE:
		if (coll->co_date != NULL)
			free(coll->co_date);
		coll->co_date = value;
		coll->co_options |= CO_CHECKOUTMODE;
		break;
	case PT_PREFIX:
		if (coll->co_prefix != NULL)
			free(coll->co_prefix);
		coll->co_prefix = value;
		break;
	case PT_RELEASE:
		if (coll->co_release != NULL)
			free(coll->co_release);
		coll->co_release = value;
		break;
	case PT_TAG:
		if (coll->co_tag != NULL)
			free(coll->co_tag);
		coll->co_tag = value;
		coll->co_options |= CO_CHECKOUTMODE;
		break;
	case PT_LIST:
		if (strchr(value, '/') != NULL) {
			lprintf(-1, "Parse error in \"%s\": \"list\" suffix "
			    "must not contain slashes\n", cfgfile);
			exit(1);
		}
		if (coll->co_listsuffix != NULL)
			free(coll->co_listsuffix);
		coll->co_listsuffix = value;
		break;
	case PT_UMASK:
		error = asciitoint(value, &mask, 8);
		free(value);
		if (error) {
			lprintf(-1, "Parse error in \"%s\": Invalid "
			    "umask value\n", cfgfile);
			exit(1);
		}
		coll->co_umask = mask;
		break;
	case PT_USE_REL_SUFFIX:
		coll->co_options |= CO_USERELSUFFIX;
		break;
	case PT_DELETE:
		coll->co_options |= CO_DELETE | CO_EXACTRCS;
		break;
	case PT_COMPRESS:
		coll->co_options |= CO_COMPRESS;
		break;
	case PT_NORSYNC:
		coll->co_options |= CO_NORSYNC;
		break;
	}
}

/* Set "coll" as being the default collection. */
void
coll_setdef(void)
{

	coll_free(defaults);
	defaults = cur_coll;
	cur_coll = coll_new(defaults);
}
