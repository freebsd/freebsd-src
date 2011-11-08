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
#include <stdint.h>
#include <fnmatch.h>
#include <dirent.h>
#ifndef RESCUE
#include <bsnmp/asn1.h>
#include <bsnmp/snmp.h>
#include <bsnmp/snmpclient.h>
#endif

#include "atmconfig.h"
#include "private.h"

/* verbosity level */
static int verbose;

/* notitle option */
static int notitle;

/* need to put heading before next output */
static int need_heading;

/*
 * TOP LEVEL commands
 */
static void help_func(int argc, char *argv[]) __dead2;

static const struct cmdtab static_main_tab[] = {
	{ "help",	NULL,		help_func },
	{ "options",	NULL,		NULL },
	{ "commands",	NULL,		NULL },
	{ "diag",	diag_tab,	NULL },
	{ "natm",	natm_tab,	NULL },
	{ NULL,		NULL,		NULL }
};

static struct cmdtab *main_tab = NULL;
static size_t main_tab_size = sizeof(static_main_tab) /
	sizeof(static_main_tab[0]);

static int
substr(const char *s1, const char *s2)
{
	return (strlen(s1) <= strlen(s2) && strncmp(s1, s2, strlen(s1)) == 0);
}

/*
 * Current help file state
 */
struct help_file {
	int	file_state;	/* 0:looking for main file, 1:found, 2:other */
	const char *p_start;	/* current path pointer */
	const char *p_end;	/* end of current path in path */
	char	*dirname;	/* directory name */
	DIR	*dir;		/* open directory */
	char	*fname;		/* current filename */
	FILE	*fp;		/* open file */
	char	line[LINE_MAX];	/* current line */
	u_int	fcnt;		/* count of files found */
};

struct help_pos {
	off_t	pos;		/* file position */
	u_int	fcnt;		/* number of file */
	char	*fname;		/* name of file */
	const char *p_start;	/* current path pointer */
	const char *p_end;	/* end of current path in path */
};

static int
help_next_file(struct help_file *hp)
{
	const char *fpat;
	struct dirent *ent;

	if (hp->file_state == 3)
		return (-1);

	if (hp->file_state == 0)
		fpat = FILE_HELP;
	else
		fpat = FILE_HELP_OTHERS;

	if (hp->file_state == 0 || hp->file_state == 1) {
		/* start from beginning */
		hp->p_start = PATH_HELP;
		hp->file_state++;
	}

  try_file:
	if (hp->dir != NULL) {
		/* directory open (must be state 2) */
		while ((ent = readdir(hp->dir)) != NULL) {
			if (fnmatch(fpat, ent->d_name, FNM_NOESCAPE) != 0)
				continue;
			if (asprintf(&hp->fname, "%s/%s", hp->dirname,
			    ent->d_name) == -1)
				err(1, NULL);
			if ((hp->fp = fopen(hp->fname, "r")) != NULL) {
				hp->fcnt++;
				return (0);
			}
			free(hp->fname);
		}
		/* end of directory */
		closedir(hp->dir);
		hp->dir = NULL;
		free(hp->dirname);
		goto next_path;
	}

	/* nothing open - advanc to new path element */
  try_path:
	for (hp->p_end = hp->p_start; *hp->p_end != '\0' &&
	    *hp->p_end != ':'; hp->p_end++)
		;

	if (asprintf(&hp->dirname, "%.*s", (int)(hp->p_end - hp->p_start),
	    hp->p_start) == -1)
		err(1, NULL);

	if (hp->file_state == 1) {
		/* just try to open */
		if (asprintf(&hp->fname, "%s/%s", hp->dirname, fpat) == -1)
			err(1, NULL);
		if ((hp->fp = fopen(hp->fname, "r")) != NULL) {
			hp->fcnt++;
			return (0);
		}
		free(hp->fname);

		goto next_path;
	}

	/* open directory */
	if ((hp->dir = opendir(hp->dirname)) != NULL)
		goto try_file;

	free(hp->dirname);

  next_path:
	hp->p_start = hp->p_end;
	if (*hp->p_start == '\0') {
		/* end of path */
		if (hp->file_state == 1)
			errx(1, "help file not found");
		return (-1);
	}
	hp->p_start++;
	goto try_path;

}

/*
 * Save current file position
 */
static void
help_file_tell(struct help_file *hp, struct help_pos *pos)
{
	if (pos->fname != NULL)
		free(pos->fname);
	if ((pos->fname = strdup(hp->fname)) == NULL)
		err(1, NULL);
	pos->fcnt = hp->fcnt;
	pos->p_start = hp->p_start;
	pos->p_end = hp->p_end;
	if ((pos->pos = ftello(hp->fp)) == -1)
		err(1, "%s", pos->fname);
}

/*
 * Go to that position
 *
 * We can go either to the original help file or back in the current file.
 */
static void
help_file_seek(struct help_file *hp, struct help_pos *pos)
{
	hp->p_start = pos->p_start;
	hp->p_end = pos->p_end;
	hp->fcnt = pos->fcnt;

	if (hp->dir != NULL) {
		free(hp->dirname);
		closedir(hp->dir);
		hp->dir = NULL;
	}

	if (hp->fp != NULL &&strcmp(hp->fname, pos->fname) != 0) {
		free(hp->fname);
		fclose(hp->fp);
		hp->fp = NULL;
	}
	if (hp->fp == NULL) {
		if ((hp->fname = strdup(pos->fname)) == NULL)
			err(1, NULL);
		if ((hp->fp = fopen(hp->fname, "r")) == NULL)
			err(1, "reopen %s", hp->fname);
	}
	if (fseeko(hp->fp, pos->pos, SEEK_SET) == -1)
		err(1, "seek %s", hp->fname);

	if (pos->fcnt == 1)
		/* go back to state 1 */
		hp->file_state = 1;
	else
		/* lock */
		hp->file_state = 3;
}

/*
 * Rewind to position 0
 */
static void
help_file_rewind(struct help_file *hp)
{

	if (hp->file_state == 1) {
		if (fseeko(hp->fp, (off_t)0, SEEK_SET) == -1)
			err(1, "rewind help file");
		return;
	}

	if (hp->dir != NULL) {
		free(hp->dirname);
		closedir(hp->dir);
		hp->dir = NULL;
	}

	if (hp->fp != NULL) {
		free(hp->fname);
		fclose(hp->fp);
		hp->fp = NULL;
	}
	memset(hp, 0, sizeof(*hp));
}

/*
 * Get next line from a help file
 */
static const char *
help_next_line(struct help_file *hp)
{
	for (;;) {
		if (hp->fp != NULL) {
			if (fgets(hp->line, sizeof(hp->line), hp->fp) != NULL)
				return (hp->line);
			if (ferror(hp->fp))
				err(1, "%s", hp->fname);
			free(hp->fname);

			fclose(hp->fp);
			hp->fp = NULL;
		}
		if (help_next_file(hp) == -1)
			return (NULL);
	}
	
}

/*
 * This function prints the available 0-level help topics from all
 * other help files by scanning the files. It assumes, that this is called
 * only from the main help file.
 */
static void
help_get_0topics(struct help_file *hp)
{
	struct help_pos save;
	const char *line;

	memset(&save, 0, sizeof(save));
	help_file_tell(hp, &save);

	help_file_rewind(hp);
	while ((line = help_next_line(hp)) != NULL) {
		if (line[0] == '^' && line[1] == '^')
			printf("%s", line + 2);
	}
	help_file_seek(hp, &save);
}

/*
 * Function to print help. The help argument is in argv[0] here.
 */
static void
help_func(int argc, char *argv[])
{
	struct help_file hfile;
	struct help_pos match, last_match;
	const char *line;
	char key[100];
	int level;
	int i, has_sub_topics;

	memset(&hfile, 0, sizeof(hfile));
	memset(&match, 0, sizeof(match));
	memset(&last_match, 0, sizeof(last_match));

	if (argc == 0) {
		/* only 'help' - show intro */
		if ((argv[0] = strdup("intro")) == NULL)
			err(1, NULL);
		argc = 1;
	}

	optind = 0;
	match.pos = -1;
	last_match.pos = -1;
	for (;;) {
		/* read next line */
		if ((line = help_next_line(&hfile)) == NULL) {
			/* EOF */
			level = 999;
			goto stop;
		}
		if (line[0] != '^' || line[1] == '^')
			continue;

		if (sscanf(line + 1, "%d%99s", &level, key) != 2)
			errx(1, "error in help file '%s'", line);

		if (level < optind) {
  stop:
			/* next higher level entry - stop this level */
			if (match.pos == -1) {
				/* not found */
				goto not_found;
			}
			/* go back to the match */
			help_file_seek(&hfile, &match);
			last_match = match;
			memset(&match, 0, sizeof(match));
			match.pos = -1;

			/* go to next key */
			if (++optind >= argc)
				break;
		}
		if (level == optind) {
			if (substr(argv[optind], key)) {
				if (match.pos != -1) {
					printf("Ambiguous topic.");
					goto list_topics;
				}
				help_file_tell(&hfile, &match);
			}
		}
	}

	/* before breaking above we have seeked back to the matching point */
	for (;;) {
		if ((line = help_next_line(&hfile)) == NULL)
			break;

		if (line[0] == '#')
			continue;
		if (line[0] == '^') {
			if (line[1] == '^')
				continue;
			break;
		}
		if (strncmp(line, "$MAIN", 5) == 0) {
			help_get_0topics(&hfile);
			continue;
		}
		printf("%s", line);
	}

	exit(0);

  not_found:
	printf("Topic not found.");

  list_topics:
	printf(" Use one of:\natmconfig help");
	for (i = 0; i < optind; i++)
		printf(" %s", argv[i]);

	printf(" [");

	/* list all the keys at this level */
	if (last_match.pos == -1)
		/* go back to start of help */
		help_file_rewind(&hfile);
	else
		help_file_seek(&hfile, &last_match);

	has_sub_topics = 0;
	while ((line = help_next_line(&hfile)) != NULL) {
		if (line[0] == '#' || line[0] != '^' || line[1] == '^')
			continue;

		if (sscanf(line + 1, "%d%99s", &level, key) != 2)
			errx(1, "error in help file '%s'", line);

		if (level < optind)
			break;
		if (level == optind) {
			has_sub_topics = 1;
			printf(" %s", key);
		}
	}
	printf(" ].");
	if (!has_sub_topics)
		printf(" No sub-topics found.");
	printf("\n");
	exit(1);
}

#ifndef RESCUE
/*
 * Parse a server specification
 *
 * syntax is [trans::][community@][server][:port]
 */
static void
parse_server(char *name)
{
	char *p, *s = name;

	/* look for a double colon */
	for (p = s; *p != '\0'; p++) {
		if (*p == '\\' && p[1] != '\0') {
			p++;
			continue;
		}
		if (*p == ':' && p[1] == ':')
			break;
	}
	if (*p != '\0') {
		if (p > s) {
			if (p - s == 3 && strncmp(s, "udp", 3) == 0)
				snmp_client.trans = SNMP_TRANS_UDP;
			else if (p - s == 6 && strncmp(s, "stream", 6) == 0)
				snmp_client.trans = SNMP_TRANS_LOC_STREAM;
			else if (p - s == 5 && strncmp(s, "dgram", 5) == 0)
				snmp_client.trans = SNMP_TRANS_LOC_DGRAM;
			else
				errx(1, "unknown SNMP transport '%.*s'",
				    (int)(p - s), s);
		}
		s = p + 2;
	}

	/* look for a @ */
	for (p = s; *p != '\0'; p++) {
		if (*p == '\\' && p[1] != '\0') {
			p++;
			continue;
		}
		if (*p == '@')
			break;
	}

	if (*p != '\0') {
		if (p - s > SNMP_COMMUNITY_MAXLEN)
			err(1, "community string too long");
		strncpy(snmp_client.read_community, s, p - s);
		snmp_client.read_community[p - s] = '\0';
		strncpy(snmp_client.write_community, s, p - s);
		snmp_client.write_community[p - s] = '\0';
		s = p + 1;
	}

	/* look for a colon */
	for (p = s; *p != '\0'; p++) {
		if (*p == '\\' && p[1] != '\0') {
			p++;
			continue;
		}
		if (*p == ':')
			break;
	}

	if (*p == ':') {
		if (p > s) {
			*p = '\0';
			snmp_client_set_host(&snmp_client, s);
			*p = ':';
		}
		snmp_client_set_port(&snmp_client, p + 1);
	} else if (p > s)
		snmp_client_set_host(&snmp_client, s);
}
#endif

int
main(int argc, char *argv[])
{
	int opt, i;
	const struct cmdtab *match, *cc, *tab;

#ifndef RESCUE
	snmp_client_init(&snmp_client);
	snmp_client.trans = SNMP_TRANS_LOC_STREAM;
	snmp_client_set_host(&snmp_client, PATH_ILMI_SOCK);
#endif

#ifdef RESCUE
#define OPTSTR	"htv"
#else
#define	OPTSTR	"htvs:"
#endif

	while ((opt = getopt(argc, argv, OPTSTR)) != -1)
		switch (opt) {

		  case 'h':
			help_func(0, argv);

#ifndef RESCUE
		  case 's':
			parse_server(optarg);
			break;
#endif

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

	if ((main_tab = malloc(sizeof(static_main_tab))) == NULL)
		err(1, NULL);
	memcpy(main_tab, static_main_tab, sizeof(static_main_tab));

#ifndef RESCUE
	/* XXX while this is compiled in */
	device_register();
#endif

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
 * And the other way 'round
 */
int
pparse(int32_t *val, const struct penum *tab, const char *str)
{

	while (tab->str != NULL) {
		if (strcmp(tab->str, str) == 0) {
			*val = tab->value;
			return (0);
		}
		tab++;
	}
	return (-1);
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

/*
 * for compiled-in modules
 */
void
register_module(const struct amodule *mod)
{
	main_tab_size++;
	if ((main_tab = realloc(main_tab, main_tab_size * sizeof(main_tab[0])))
	    == NULL)
		err(1, NULL);
	main_tab[main_tab_size - 2] = *mod->cmd;
	memset(&main_tab[main_tab_size - 1], 0, sizeof(main_tab[0]));
}
