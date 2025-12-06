/* $Id: manpath.c,v 1.45 2025/06/26 17:26:23 schwarze Exp $ */
/*
 * Copyright (c) 2011,2014,2015,2017-2021 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "manconf.h"

static	void	 manconf_file(struct manconf *, const char *, int);
static	void	 manpath_add(struct manpaths *, const char *, char);
static	void	 manpath_parseline(struct manpaths *, char *, char);


void
manconf_parse(struct manconf *conf, const char *file, char *pend, char *pbeg)
{
	int use_path_from_file = 1;

	/* Always prepend -m. */
	manpath_parseline(&conf->manpath, pbeg, 'm');

	if (pend != NULL && *pend != '\0') {
		/* If -M is given, it overrides everything else. */
		manpath_parseline(&conf->manpath, pend, 'M');
		use_path_from_file = 0;
		pbeg = pend = NULL;
	} else if ((pbeg = getenv("MANPATH")) == NULL || *pbeg == '\0') {
		/* No MANPATH; use man.conf(5) only. */
		pbeg = pend = NULL;
	} else if (*pbeg == ':') {
		/* Prepend man.conf(5) to MANPATH. */
		pend = pbeg + 1;
		pbeg = NULL;
	} else if ((pend = strstr(pbeg, "::")) != NULL) {
		/* Insert man.conf(5) into MANPATH. */
		*pend = '\0';
		pend += 2;
	} else if (pbeg[strlen(pbeg) - 1] == ':') {
		/* Append man.conf(5) to MANPATH. */
		pend = NULL;
	} else {
		/* MANPATH overrides man.conf(5) completely. */
		use_path_from_file = 0;
		pend = NULL;
	}

	manpath_parseline(&conf->manpath, pbeg, '\0');

	if (file == NULL)
		file = MAN_CONF_FILE;
	manconf_file(conf, file, use_path_from_file);

	manpath_parseline(&conf->manpath, pend, '\0');
}

void
manpath_base(struct manpaths *dirs)
{
	char path_base[] = MANPATH_BASE;
	manpath_parseline(dirs, path_base, '\0');
}

/*
 * Parse a FULL pathname from a colon-separated list of arrays.
 */
static void
manpath_parseline(struct manpaths *dirs, char *path, char option)
{
	char	*dir;

	if (NULL == path)
		return;

	for (dir = strtok(path, ":"); dir; dir = strtok(NULL, ":"))
		manpath_add(dirs, dir, option);
}

/*
 * Add a directory to the array, ignoring bad directories.
 * Grow the array one-by-one for simplicity's sake.
 */
static void
manpath_add(struct manpaths *dirs, const char *dir, char option)
{
	char		 buf[PATH_MAX];
	struct stat	 sb;
	char		*cp;
	size_t		 i;

	if ((cp = realpath(dir, buf)) == NULL)
		goto fail;

	for (i = 0; i < dirs->sz; i++)
		if (strcmp(dirs->paths[i], dir) == 0)
			return;

	if (stat(cp, &sb) == -1)
		goto fail;

	dirs->paths = mandoc_reallocarray(dirs->paths,
	    dirs->sz + 1, sizeof(*dirs->paths));
	dirs->paths[dirs->sz++] = mandoc_strdup(cp);
	return;

fail:
	if (option != '\0')
		mandoc_msg(MANDOCERR_BADARG_BAD, 0, 0,
		    "-%c %s: %s", option, dir, strerror(errno));
}

void
manconf_free(struct manconf *conf)
{
	size_t		 i;

	for (i = 0; i < conf->manpath.sz; i++)
		free(conf->manpath.paths[i]);

	free(conf->manpath.paths);
	free(conf->output.includes);
	free(conf->output.man);
	free(conf->output.paper);
	free(conf->output.style);
}

static void
manconf_file(struct manconf *conf, const char *file, int use_path_from_file)
{
	const char *const toks[] = { "manpath", "output" };
	char manpath_default[] = MANPATH_DEFAULT;

	FILE		*stream;
	char		*line, *cp, *ep;
	size_t		 linesz, tok, toklen;
	ssize_t		 linelen;

	if ((stream = fopen(file, "r")) == NULL)
		goto out;

	line = NULL;
	linesz = 0;

	while ((linelen = getline(&line, &linesz, stream)) != -1) {
		cp = line;
		ep = cp + linelen - 1;
		while (ep > cp && isspace((unsigned char)*ep))
			*ep-- = '\0';
		while (isspace((unsigned char)*cp))
			cp++;
		if (cp == ep || *cp == '#')
			continue;

		for (tok = 0; tok < sizeof(toks)/sizeof(toks[0]); tok++) {
			toklen = strlen(toks[tok]);
			if (cp + toklen < ep &&
			    isspace((unsigned char)cp[toklen]) &&
			    strncmp(cp, toks[tok], toklen) == 0) {
				cp += toklen;
				while (isspace((unsigned char)*cp))
					cp++;
				break;
			}
		}

		switch (tok) {
		case 0:  /* manpath */
			if (use_path_from_file)
				manpath_add(&conf->manpath, cp, '\0');
			*manpath_default = '\0';
			break;
		case 1:  /* output */
			manconf_output(&conf->output, cp, 1);
			break;
		default:
			break;
		}
	}
	free(line);
	fclose(stream);

out:
	if (use_path_from_file && *manpath_default != '\0')
		manpath_parseline(&conf->manpath, manpath_default, '\0');
}

int
manconf_output(struct manoutput *conf, const char *cp, int fromfile)
{
	const char *const toks[] = {
	    /* Tokens requiring an argument. */
	    "includes", "man", "paper", "style", "indent", "width",
	    "outfilename", "tagfilename",
	    /* Token taking an optional argument. */
	    "tag",
	    /* Tokens not taking arguments. */
	    "fragment", "noval", "toc"
	};
	const size_t ntoks = sizeof(toks) / sizeof(toks[0]);

	const char	*errstr;
	char		*oldval;
	size_t		 len, tok;

	for (tok = 0; tok < ntoks; tok++) {
		len = strlen(toks[tok]);
		if (strncmp(cp, toks[tok], len) == 0 &&
		    strchr(" =	", cp[len]) != NULL) {
			cp += len;
			if (*cp == '=')
				cp++;
			while (isspace((unsigned char)*cp))
				cp++;
			break;
		}
	}

	if (tok < 8 && *cp == '\0') {
		mandoc_msg(MANDOCERR_BADVAL_MISS, 0, 0, "-O %s=?", toks[tok]);
		return -1;
	}
	if (tok > 8 && tok < ntoks && *cp != '\0') {
		mandoc_msg(MANDOCERR_BADVAL, 0, 0, "-O %s=%s", toks[tok], cp);
		return -1;
	}

	switch (tok) {
	case 0:
		if (conf->includes != NULL) {
			oldval = mandoc_strdup(conf->includes);
			break;
		}
		conf->includes = mandoc_strdup(cp);
		return 0;
	case 1:
		if (conf->man != NULL) {
			oldval = mandoc_strdup(conf->man);
			break;
		}
		conf->man = mandoc_strdup(cp);
		return 0;
	case 2:
		if (conf->paper != NULL) {
			oldval = mandoc_strdup(conf->paper);
			break;
		}
		conf->paper = mandoc_strdup(cp);
		return 0;
	case 3:
		if (conf->style != NULL) {
			oldval = mandoc_strdup(conf->style);
			break;
		}
		conf->style = mandoc_strdup(cp);
		return 0;
	case 4:
		if (conf->indent) {
			mandoc_asprintf(&oldval, "%zu", conf->indent);
			break;
		}
		conf->indent = strtonum(cp, 0, 1000, &errstr);
		if (errstr == NULL)
			return 0;
		mandoc_msg(MANDOCERR_BADVAL_BAD, 0, 0,
		    "-O indent=%s is %s", cp, errstr);
		return -1;
	case 5:
		if (conf->width) {
			mandoc_asprintf(&oldval, "%zu", conf->width);
			break;
		}
		conf->width = strtonum(cp, 1, 1000, &errstr);
		if (errstr == NULL)
			return 0;
		mandoc_msg(MANDOCERR_BADVAL_BAD, 0, 0,
		    "-O width=%s is %s", cp, errstr);
		return -1;
	case 6:
		if (conf->outfilename != NULL) {
			oldval = mandoc_strdup(conf->outfilename);
			break;
		}
		conf->outfilename = mandoc_strdup(cp);
		return 0;
	case 7:
		if (conf->tagfilename != NULL) {
			oldval = mandoc_strdup(conf->tagfilename);
			break;
		}
		conf->tagfilename = mandoc_strdup(cp);
		return 0;
	/*
	 * If the index of the following token changes,
	 * do not forget to adjust the range check above the switch.
	 */
	case 8:
		if (conf->tag != NULL) {
			oldval = mandoc_strdup(conf->tag);
			break;
		}
		conf->tag = mandoc_strdup(cp);
		return 0;
	case 9:
		conf->fragment = 1;
		return 0;
	case 10:
		conf->noval = 1;
		return 0;
	case 11:
		conf->toc = 1;
		return 0;
	default:
		mandoc_msg(MANDOCERR_BADARG_BAD, 0, 0, "-O %s", cp);
		return -1;
	}
	if (fromfile) {
		free(oldval);
		return 0;
	} else {
		mandoc_msg(MANDOCERR_BADVAL_DUPE, 0, 0,
		    "-O %s=%s: already set to %s", toks[tok], cp, oldval);
		free(oldval);
		return -1;
	}
}
