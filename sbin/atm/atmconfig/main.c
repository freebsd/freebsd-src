/*
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * Author: Hartmut Brandt <harti@freebsd.org>
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/sysctl.h>
#include <netdb.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <inttypes.h>
#include "atmconfig.h"
#include "private.h"

/* verbosity level */
int verbose;

/* notitle option */
static int notitle;

/* need to put heading before next output */
static int need_heading;

/*
 * TOP LEVEL commands
 */
static void help_func(int argc, char *argv[]) __dead2;

static const struct cmdtab main_tab[] = {
	{ "help",	NULL,		help_func },
	{ "options",	NULL,		NULL },
	{ "commands",	NULL,		NULL },
	{ "diag",	diag_tab,	NULL },
	{ "natm",	natm_tab,	NULL },
	{ NULL,		NULL,		NULL }
};

static int
substr(const char *s1, const char *s2)
{
	return (strlen(s1) <= strlen(s2) && strncmp(s1, s2, strlen(s1)) == 0);
}

/*
 * Function to print help. The help argument is in argv[0] here.
 */
static void
help_func(int argc, char *argv[])
{
	FILE *hp;
	const char *start, *end;
	char *fname;
	off_t match, last_match;
	char line[LINE_MAX];
	char key[100];
	int level, i;

	/*
	 * Find the help file
	 */
	hp = NULL;
	for (start = PATH_HELP; *start != '\0'; start = end + 1) {
		for (end = start; *end != ':' && *end != '\0'; end++)
			;
		if (start == end) {
			if (asprintf(&fname, "%s", FILE_HELP) == -1)
				err(1, NULL);
		} else {
			if (asprintf(&fname, "%.*s/%s", (int)(end - start),
			    start, FILE_HELP) == -1)
				err(1, NULL);
		}
		if ((hp = fopen(fname, "r")) != NULL)
			break;
		free(fname);
	}
	if (hp == NULL)
		errx(1, "help file not found");

	if (argc == 0) {
		argv[0] = __DECONST(char *, "intro");
		argc = 1;
	}

	optind = 0;
	match = -1;
	last_match = -1;
	for (;;) {
		/* read next line */
		if (fgets(line, sizeof(line), hp) == NULL) {
			if (ferror(hp))
				err(1, fname);
			/* EOF */
			clearerr(hp);
			level = 999;
			goto stop;
		}
		if (line[0] != '^')
			continue;

		if (sscanf(line + 1, "%d%99s", &level, key) != 2)
			errx(1, "error in help file '%s'", line);

		if (level < optind) {
  stop:
			/* next higher level entry - stop this level */
			if (match == -1) {
				/* not found */
				goto not_found;
			}
			/* go back to the match */
			if (fseeko(hp, match, SEEK_SET) == -1)
				err(1, fname);
			last_match = match;
			match = -1;

			/* go to next key */
			if (++optind >= argc)
				break;
		}
		if (level == optind) {
			if (substr(argv[optind], key)) {
				if (match != -1) {
					printf("Ambiguous topic.");
					goto list_topics;
				}
				if ((match = ftello(hp)) == -1)
					err(1, fname);
			}
		}
	}
	if (last_match == -1) {
		if (fseek(hp, 0L, SEEK_SET) == -1)
			err(1, fname);
	} else {
		if (fseeko(hp, last_match, SEEK_SET) == -1)
			err(1, fname);
	}

	for (;;) {
		if (fgets(line, sizeof(line), hp) == NULL) {
			if (ferror(hp))
				err(1, fname);
			break;
		}
		if (line[0] == '#')
			continue;
		if (line[0] == '^')
			break;
		printf("%s", line);
	}

	exit(0);

  not_found:
	printf("Topic not found.");

  list_topics:
	printf(" Use one of:\natmconfig");
	for (i = 0; i < optind; i++)
		printf(" %s", argv[i]);
	printf(" [");
	/* list all the keys at this level */
	if (last_match == -1) {
		if (fseek(hp, 0L, SEEK_SET) == -1)
			err(1, fname);
	} else {
		if (fseeko(hp, last_match, SEEK_SET) == -1)
			err(1, fname);
	}

	for (;;) {
		/* read next line */
		if (fgets(line, sizeof(line), hp) == NULL) {
			if (ferror(hp))
				err(1, fname);
			break;
		}
		if (line[0] == '#' || line[0] != '^')
			continue;

		if (sscanf(line + 1, "%d%99s", &level, key) != 2)
			errx(1, "error in help file '%s'", line);

		if (level < optind)
			break;
		if (level == optind)
			printf(" %s", key);
	}
	printf(" ]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int opt, i;
	const struct cmdtab *match, *cc, *tab;

	while ((opt = getopt(argc, argv, "htv")) != -1)
		switch (opt) {

		  case 'h':
			help_func(0, argv);

		  case 'v':
			verbose++;
			break;

		  case 't':
			notitle = 1;
			break;
		}

	if (argv[optind] == NULL)
		help_func(0, argv);

	argc -= optind;
	argv += optind;

	cc = main_tab;
	i = 0;
	for (;;) {
		/*
		 * Scan the table for a match
		 */
		tab = cc;
		match = NULL;
		while (cc->string != NULL) {
			if (substr(argv[i], cc->string)) {
				if (match != NULL) {
					printf("Ambiguous option '%s'",
					    argv[i]);
					cc = tab;
					goto subopts;
				}
				match = cc;
			}
			cc++;
		}
		if ((cc = match) == NULL) {
			printf("Unknown option '%s'", argv[i]);
			cc = tab;
			goto subopts;
		}

		/*
		 * Have a match. If there is no subtable, there must
		 * be either a handler or the command is only a help entry.
		 */
		if (cc->sub == NULL) {
			if (cc->func != NULL)
				break;
			printf("Unknown option '%s'", argv[i]);
			cc = tab;
			goto subopts;
		}

		/*
		 * Look at the next argument. If it doesn't exist or it
		 * looks like a switch, terminate the scan here.
		 */
		if (argv[i + 1] == NULL || argv[i + 1][0] == '-') {
			if (cc->func != NULL)
				break;
			printf("Need sub-option for '%s'", argv[i]);
			cc = cc->sub;
			goto subopts;
		}

		cc = cc->sub;
		i++;
	}

	argc -= i + 1;
	argv += i + 1;

	(*cc->func)(argc, argv);

	return (0);

  subopts:
	printf(". Select one of:\n");
	while (cc->string != NULL) {
		if (cc->func != NULL || cc->sub != NULL)
			printf("%s ", cc->string);
		cc++;
	}
	printf("\n");

	return (1);
}

void
verb(const char *fmt, ...)
{
	va_list ap;

	if (verbose) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
		va_end(ap);
	}
}

void
heading(const char *fmt, ...)
{
	va_list ap;

	if (need_heading) {
		need_heading = 0;
		if (!notitle) {
			va_start(ap, fmt);
			fprintf(stdout, fmt, ap);
			va_end(ap);
		}
	}
}

void
heading_init(void)
{
	need_heading = 1;
}

/*
 * stringify an enumerated value
 */
const char *
penum(int32_t value, const struct penum *strtab, char *buf)
{
	while (strtab->str != NULL) {
		if (strtab->value == value) {
			strcpy(buf, strtab->str);
			return (buf);
		}
		strtab++;
	}
	warnx("illegal value for enumerated variable '%d'", value);
	strcpy(buf, "?");
	return (buf);
}

/*
 * Parse command line options
 */
int
parse_options(int *pargc, char ***pargv, const struct option *opts)
{
	const struct option *o, *m;
	char *arg;
	u_long ularg, ularg1;
	long larg;
	char *end;

	if (*pargc == 0)
		return (-1);
	arg = (*pargv)[0];
	if (arg[0] != '-' || arg[1] == '\0')
		return (-1);
	if (arg[1] == '-' && arg[2] == '\0') {
		(*pargv)++;
		(*pargc)--;
		return (-1);
	}

	m = NULL;
	for (o = opts; o->optstr != NULL; o++) {
		if (strlen(arg + 1) <= strlen(o->optstr) &&
		    strncmp(arg + 1, o->optstr, strlen(arg + 1)) == 0) {
			if (m != NULL)
				errx(1, "ambiguous option '%s'", arg);
			m = o;
		}
	}
	if (m == NULL)
		errx(1, "unknown option '%s'", arg);
	
	(*pargv)++;
	(*pargc)--;

	if (m->opttype == OPT_NONE)
		return (m - opts);

	if (m->opttype == OPT_SIMPLE) {
		*(int *)m->optarg = 1;
		return (m - opts);
	}

	if (*pargc == 0)
		errx(1, "option requires argument '%s'", arg);
	optarg = *(*pargv)++;
	(*pargc)--;

	switch (m->opttype) {

	  case OPT_UINT:
		ularg = strtoul(optarg, &end, 0);
		if (*end != '\0')
			errx(1, "bad unsigned integer argument for '%s'", arg);
		if (ularg > UINT_MAX)
			errx(1, "argument to large for option '%s'", arg);
		*(u_int *)m->optarg = (u_int)ularg;
		break;

	  case OPT_INT:
		larg = strtol(optarg, &end, 0);
		if (*end != '\0')
			errx(1, "bad integer argument for '%s'", arg);
		if (larg > INT_MAX || larg < INT_MIN)
			errx(1, "argument out of range for option '%s'", arg);
		*(int *)m->optarg = (int)larg;
		break;

	  case OPT_UINT32:
		ularg = strtoul(optarg, &end, 0);
		if (*end != '\0')
			errx(1, "bad unsigned integer argument for '%s'", arg);
		if (ularg > UINT32_MAX)
			errx(1, "argument to large for option '%s'", arg);
		*(uint32_t *)m->optarg = (uint32_t)ularg;
		break;

	  case OPT_INT32:
		larg = strtol(optarg, &end, 0);
		if (*end != '\0')
			errx(1, "bad integer argument for '%s'", arg);
		if (larg > INT32_MAX || larg < INT32_MIN)
			errx(1, "argument out of range for option '%s'", arg);
		*(int32_t *)m->optarg = (int32_t)larg;
		break;

	  case OPT_UINT64:
		*(uint64_t *)m->optarg = strtoull(optarg, &end, 0);
		if (*end != '\0')
			errx(1, "bad unsigned integer argument for '%s'", arg);
		break;

	  case OPT_INT64:
		*(int64_t *)m->optarg = strtoll(optarg, &end, 0);
		if (*end != '\0')
			errx(1, "bad integer argument for '%s'", arg);
		break;

	  case OPT_FLAG:
		if (strcasecmp(optarg, "enable") == 0 ||
		    strcasecmp(optarg, "yes") == 0 ||
		    strcasecmp(optarg, "true") == 0 ||
		    strcasecmp(optarg, "on") == 0 ||
		    strcmp(optarg, "1") == 0)
			*(int *)m->optarg = 1;
		else if (strcasecmp(optarg, "disable") == 0 ||
		    strcasecmp(optarg, "no") == 0 ||
		    strcasecmp(optarg, "false") == 0 ||
		    strcasecmp(optarg, "off") == 0 ||
		    strcmp(optarg, "0") == 0)
			*(int *)m->optarg = 0;
		else
			errx(1, "bad boolean argument to '%s'", arg);
		break;

	  case OPT_VCI:
		ularg = strtoul(optarg, &end, 0);
		if (*end == '.') {
			ularg1 = strtoul(end + 1, &end, 0);
		} else {
			ularg1 = ularg;
			ularg = 0;
		}
		if (*end != '\0')
			errx(1, "bad VCI value for option '%s'", arg);
		if (ularg > 0xff)
			errx(1, "VPI value too large for option '%s'", arg);
		if (ularg1 > 0xffff)
			errx(1, "VCI value too large for option '%s'", arg);
		((u_int *)m->optarg)[0] = ularg;
		((u_int *)m->optarg)[1] = ularg1;
		break;

	  case OPT_STRING:
		if (m->optarg != NULL)
			*(const char **)m->optarg = optarg;
		break;

	  default:
		errx(1, "(internal) bad option type %u for '%s'",
		    m->opttype, arg);
	}
	return (m - opts);
}
