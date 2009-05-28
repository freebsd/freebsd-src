/*-
 * Copyright (c) 2003 Mike Barcroft <mike@FreeBSD.org>
 * Copyright (c) 2008 Bjoern A. Zeeb <bz@FreeBSD.org>
 * Copyright (c) 2009 James Gritton <jamie@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	SJPARAM		"security.jail.param"
#define	ARRAY_SLOP	5

#define	CTLTYPE_BOOL	(CTLTYPE + 1)
#define	CTLTYPE_NOBOOL	(CTLTYPE + 2)
#define	CTLTYPE_IPADDR	(CTLTYPE + 3)
#define	CTLTYPE_IP6ADDR	(CTLTYPE + 4)

#define	PARAM_KEY	0x01
#define	PARAM_USER	0x02
#define	PARAM_ARRAY	0x04
#define	PARAM_OPT	0x08
#define	PARAM_WR	0x10

#define	PRINT_DEFAULT	0x01
#define	PRINT_HEADER	0x02
#define	PRINT_NAMEVAL	0x04
#define	PRINT_QUOTED	0x08
#define	PRINT_SKIP	0x10
#define	PRINT_VERBOSE	0x20

struct param {
	char	*name;
	void	*value;
	size_t	 size;
	int	 type;
	unsigned flags;
	int	 noparent;
};

struct iovec2 {
	struct iovec	name;
	struct iovec	value;
};

static struct param *params;
static int nparams;
static char errmsg[256];

static int add_param(const char *name, void *value, unsigned flags);
static int get_param(const char *name, struct param *param);
static int sort_param(const void *a, const void *b);
static char *noname(const char *name);
static char *nononame(const char *name);
static int print_jail(int pflags, int jflags);
static void quoted_print(char *str, int len);

int
main(int argc, char **argv)
{
	char *dot, *ep, *jname, *nname;
	int c, i, jflags, jid, lastjid, pflags, spc;

	jname = NULL;
	pflags = jflags = jid = 0;
	while ((c = getopt(argc, argv, "adj:hnqsv")) >= 0)
		switch (c) {
		case 'a':
		case 'd':
			jflags |= JAIL_DYING;
			break;
		case 'j':
			jid = strtoul(optarg, &ep, 10);
			if (!*optarg || *ep)
				jname = optarg;
			break;
		case 'h':
			pflags = (pflags & ~PRINT_SKIP) | PRINT_HEADER;
			break;
		case 'n':
			pflags = (pflags & ~PRINT_VERBOSE) | PRINT_NAMEVAL;
			break;
		case 'q':
			pflags |= PRINT_QUOTED;
			break;
		case 's':
			pflags = (pflags & ~(PRINT_HEADER | PRINT_VERBOSE)) |
			    PRINT_NAMEVAL | PRINT_QUOTED | PRINT_SKIP;
			break;
		case 'v':
			pflags = (pflags & ~(PRINT_NAMEVAL | PRINT_SKIP)) |
			    PRINT_VERBOSE;
			break;
		default:
			errx(1, "usage: jls [-dhnqv] [-j jail] [param ...]");
		}

	/* Add the parameters to print. */
	if (optind == argc) {
		if (pflags & PRINT_VERBOSE) {
			add_param("jid", NULL, PARAM_USER);
			add_param("host.hostname", NULL, PARAM_USER);
			add_param("path", NULL, PARAM_USER);
			add_param("name", NULL, PARAM_USER);
			add_param("dying", NULL, PARAM_USER);
			add_param("cpuset", NULL, PARAM_USER);
			add_param("ip4.addr", NULL, PARAM_USER);
			add_param("ip6.addr", NULL, PARAM_USER | PARAM_OPT);
		} else {
			pflags = (pflags &
			    ~(PRINT_NAMEVAL | PRINT_SKIP | PRINT_VERBOSE)) |
			    PRINT_DEFAULT;
			add_param("jid", NULL, PARAM_USER);
			add_param("ip4.addr", NULL, PARAM_USER);
			add_param("host.hostname", NULL, PARAM_USER);
			add_param("path", NULL, PARAM_USER);
		}
	} else
		while (optind < argc)
			add_param(argv[optind++], NULL, PARAM_USER);

	if (pflags & PRINT_SKIP) {
		/* Check for parameters with boolean parents. */
		for (i = 0; i < nparams; i++) {
			if ((params[i].flags & PARAM_USER) &&
			    (dot = strchr(params[i].name, '.'))) {
				*dot = 0;
				nname = noname(params[i].name);
				*dot = '.';
				params[i].noparent =
				    add_param(nname, NULL, PARAM_OPT);
				free(nname);
			}
		}
	}

	/* Add the index key and errmsg parameters. */
	if (jid != 0)
		add_param("jid", &jid, PARAM_KEY);
	else if (jname != NULL)
		add_param("name", jname, PARAM_KEY);
	else
		add_param("lastjid", &lastjid, PARAM_KEY);
	add_param("errmsg", errmsg, PARAM_KEY);

	/* Print a header line if requested. */
	if (pflags & PRINT_VERBOSE)
		printf("   JID  Hostname                      Path\n"
		       "        Name                          State\n"
		       "        CPUSetID\n"
		       "        IP Address(es)\n");
	else if (pflags & PRINT_DEFAULT)
		printf("   JID  IP Address      "
		       "Hostname                      Path\n");
	else if (pflags & PRINT_HEADER) {
		for (i = spc = 0; i < nparams; i++)
			if (params[i].flags & PARAM_USER) {
				if (spc)
					putchar(' ');
				else
					spc = 1;
				fputs(params[i].name, stdout);
			}
		putchar('\n');
	}

	/* Fetch the jail(s) and print the paramters. */
	if (jid != 0 || jname != NULL) {
		if (print_jail(pflags, jflags) < 0) {
			if (errmsg[0])
				errx(1, "%s", errmsg);
			err(1, "jail_get");
		}
	} else {
		for (lastjid = 0;
		     (lastjid = print_jail(pflags, jflags)) >= 0; )
			;
		if (errno != 0 && errno != ENOENT) {
			if (errmsg[0])
				errx(1, "%s", errmsg);
			err(1, "jail_get");
		}
	}

	return (0);
}

static int
add_param(const char *name, void *value, unsigned flags)
{
	struct param *param;
	char *nname;
	size_t mlen1, mlen2, buflen;
	int mib1[CTL_MAXNAME], mib2[CTL_MAXNAME - 2];
	int i, tnparams;
	char buf[MAXPATHLEN];

	static int paramlistsize;

	/* The pseudo-parameter "all" scans the list of available parameters. */
	if (!strcmp(name, "all")) {
		tnparams = nparams;
		mib1[0] = 0;
		mib1[1] = 2;
		mlen1 = CTL_MAXNAME - 2;
		if (sysctlnametomib(SJPARAM, mib1 + 2, &mlen1) < 0)
			err(1, "sysctlnametomib(" SJPARAM ")");
		for (;;) {
			/* Get the next parameter. */
			mlen2 = sizeof(mib2);
			if (sysctl(mib1, mlen1 + 2, mib2, &mlen2, NULL, 0) < 0)
				err(1, "sysctl(0.2)");
			if (mib2[0] != mib1[2] || mib2[1] != mib1[3] ||
			    mib2[2] != mib1[4])
				break;
			/* Convert it to an ascii name. */
			memcpy(mib1 + 2, mib2, mlen2);
			mlen1 = mlen2 / sizeof(int);
			mib1[1] = 1;
			buflen = sizeof(buf);
			if (sysctl(mib1, mlen1 + 2, buf, &buflen, NULL, 0) < 0)
				err(1, "sysctl(0.1)");
			add_param(buf + sizeof(SJPARAM), NULL, flags);
			/*
			 * Convert nobool parameters to bool if their
			 * counterpart is a node, ortherwise discard them.
			 */
			param = &params[nparams - 1];
			if (param->type == CTLTYPE_NOBOOL) {
				nname = nononame(param->name);
				if (get_param(nname, param) >= 0 &&
				    param->type != CTLTYPE_NODE) {
					free(nname);
					nparams--;
				} else {
					free(param->name);
					param->name = nname;
					param->type = CTLTYPE_BOOL;
					param->size = sizeof(int);
					param->value = NULL;
				}
			}
			mib1[1] = 2;
		}

		qsort(params + tnparams, (size_t)(nparams - tnparams),
		    sizeof(struct param), sort_param);
		return -1;
	}

	/* Check for repeat parameters. */
	for (i = 0; i < nparams; i++)
		if (!strcmp(name, params[i].name)) {
			params[i].value = value;
			params[i].flags |= flags;
			return i;
		}

	/* Make sure there is room for the new param record. */
	if (!nparams) {
		paramlistsize = 32;
		params = malloc(paramlistsize * sizeof(*params));
		if (params == NULL)
			err(1, "malloc");
	} else if (nparams >= paramlistsize) {
		paramlistsize *= 2;
		params = realloc(params, paramlistsize * sizeof(*params));
		if (params == NULL)
			err(1, "realloc");
	}

	/* Look up the parameter. */
	param = params + nparams++;
	memset(param, 0, sizeof *param);
	param->name = strdup(name);
	if (param->name == NULL)
		err(1, "strdup");
	param->flags = flags;
	param->noparent = -1;
	/* We have to know about pseudo-parameters without asking. */
	if (!strcmp(param->name, "lastjid")) {
		param->type = CTLTYPE_INT;
		param->size = sizeof(int);
		goto got_type;
	}
	if (!strcmp(param->name, "errmsg")) {
		param->type = CTLTYPE_STRING;
		param->size = sizeof(errmsg);
		goto got_type;
	}
	if (get_param(name, param) < 0) {
		if (errno != ENOENT)
			err(1, "sysctl(0.3.%s)", name);
		/* See if this the "no" part of an existing boolean. */
		if ((nname = nononame(name))) {
			i = get_param(nname, param);
			free(nname);
			if (i >= 0 && param->type == CTLTYPE_BOOL) {
				param->type = CTLTYPE_NOBOOL;
				goto got_type;
			}
		}
		if (flags & PARAM_OPT) {
			nparams--;
			return -1;
		}
		errx(1, "unknown parameter: %s", name);
	}
	if (param->type == CTLTYPE_NODE) {
		/*
		 * A node isn't normally a parameter, but may be a boolean
		 * if its "no" counterpart exists.
		 */
		nname = noname(name);
		i = get_param(nname, param);
		free(nname);
		if (i >= 0 && param->type == CTLTYPE_NOBOOL) {
			param->type = CTLTYPE_BOOL;
			goto got_type;
		}
		errx(1, "unknown parameter: %s", name);
	}

 got_type:
	param->value = value;
	return param - params;
}

static int
get_param(const char *name, struct param *param)
{
	char *p;
	size_t buflen, mlen;
	int mib[CTL_MAXNAME];
	struct {
		int i;
		char s[MAXPATHLEN];
	} buf;

	/* Look up the MIB. */
	mib[0] = 0;
	mib[1] = 3;
	snprintf(buf.s, sizeof(buf.s), SJPARAM ".%s", name);
	mlen = sizeof(mib) - 2 * sizeof(int);
	if (sysctl(mib, 2, mib + 2, &mlen, buf.s, strlen(buf.s)) < 0)
		return (-1);
	/* Get the type and size. */
	mib[1] = 4;
	buflen = sizeof(buf);
	if (sysctl(mib, (mlen / sizeof(int)) + 2, &buf, &buflen, NULL, 0) < 0)
		err(1, "sysctl(0.4.%s)", name);
	param->type = buf.i & CTLTYPE;
	if (buf.i & (CTLFLAG_WR | CTLFLAG_TUN))
		param->flags |= PARAM_WR;
	p = strchr(buf.s, '\0');
	if (p - 2 >= buf.s && !strcmp(p - 2, ",a")) {
		p[-2] = 0;
		param->flags |= PARAM_ARRAY;
	}
	switch (param->type) {
	case CTLTYPE_INT:
		/* An integer parameter might be a boolean. */
		if (buf.s[0] == 'B')
			param->type = buf.s[1] == 'N'
			    ? CTLTYPE_NOBOOL : CTLTYPE_BOOL;
	case CTLTYPE_UINT:
		param->size = sizeof(int);
		break;
	case CTLTYPE_LONG:
	case CTLTYPE_ULONG:
		param->size = sizeof(long);
		break;
	case CTLTYPE_STRUCT:
		if (!strcmp(buf.s, "S,in_addr")) {
			param->type = CTLTYPE_IPADDR;
			param->size = sizeof(struct in_addr);
		} else if (!strcmp(buf.s, "S,in6_addr")) {
			param->type = CTLTYPE_IP6ADDR;
			param->size = sizeof(struct in6_addr);
		}
		break;
	case CTLTYPE_STRING:
		buf.s[0] = 0;
		sysctl(mib + 2, mlen / sizeof(int), buf.s, &buflen, NULL, 0);
		param->size = strtoul(buf.s, NULL, 10);
		if (param->size == 0)
			param->size = BUFSIZ;
	}
	return (0);
}

static int
sort_param(const void *a, const void *b)
{
	const struct param *parama, *paramb;
	char *ap, *bp;

	/* Put top-level parameters first. */
	parama = a;
	paramb = b;
	ap = strchr(parama->name, '.');
	bp = strchr(paramb->name, '.');
	if (ap && !bp)
		return (1);
	if (bp && !ap)
		return (-1);
	return (strcmp(parama->name, paramb->name));
}

static char *
noname(const char *name)
{
	char *nname, *p;

	nname = malloc(strlen(name) + 3);
	if (nname == NULL)
		err(1, "malloc");
	p = strrchr(name, '.');
	if (p != NULL)
		sprintf(nname, "%.*s.no%s", (int)(p - name), name, p + 1);
	else
		sprintf(nname, "no%s", name);
	return nname;
}

static char *
nononame(const char *name)
{
	char *nname, *p;

	p = strrchr(name, '.');
	if (strncmp(p ? p + 1 : name, "no", 2))
		return NULL;
	nname = malloc(strlen(name) - 1);
	if (nname == NULL)
		err(1, "malloc");
	if (p != NULL)
		sprintf(nname, "%.*s.%s", (int)(p - name), name, p + 3);
	else
		strcpy(nname, name + 2);
	return nname;
}

static int
print_jail(int pflags, int jflags)
{
	char *nname;
	int i, ai, jid, count, sanity, spc;
	char ipbuf[INET6_ADDRSTRLEN];

	static struct iovec2 *iov, *aiov;
	static int narray, nkey;

	/* Set up the parameter list(s) the first time around. */
	if (iov == NULL) {
		iov = malloc(nparams * sizeof(struct iovec2));
		if (iov == NULL)
			err(1, "malloc");
		for (i = narray = 0; i < nparams; i++) {
			iov[i].name.iov_base = params[i].name;
			iov[i].name.iov_len = strlen(params[i].name) + 1;
			iov[i].value.iov_base = params[i].value;
			iov[i].value.iov_len =
			    params[i].type == CTLTYPE_STRING &&
			    params[i].value != NULL &&
			    ((char *)params[i].value)[0] != '\0'
			    ? strlen(params[i].value) + 1 : params[i].size;
			if (params[i].flags & (PARAM_KEY | PARAM_ARRAY)) {
				narray++;
				if (params[i].flags & PARAM_KEY)
					nkey++;
			}
		}
		if (narray > nkey) {
			aiov = malloc(narray * sizeof(struct iovec2));
			if (aiov == NULL)
				err(1, "malloc");
			for (i = ai = 0; i < nparams; i++)
				if (params[i].flags &
				    (PARAM_KEY | PARAM_ARRAY))
					aiov[ai++] = iov[i];
		}
	}
	/* If there are array parameters, find their sizes. */
	if (aiov != NULL) {
		for (ai = 0; ai < narray; ai++)
			if (aiov[ai].value.iov_base == NULL)
				aiov[ai].value.iov_len = 0;
		if (jail_get((struct iovec *)aiov, 2 * narray, jflags) < 0)
			return (-1);
	}
	/* Allocate storage for all parameters. */
	for (i = ai = 0; i < nparams; i++) {
		if (params[i].flags & (PARAM_KEY | PARAM_ARRAY)) {
			if (params[i].flags & PARAM_ARRAY) {
				iov[i].value.iov_len = aiov[ai].value.iov_len +
				    ARRAY_SLOP * params[i].size;
				iov[i].value.iov_base =
				    malloc(iov[i].value.iov_len);
			}
			ai++;
		} else
			iov[i].value.iov_base = malloc(params[i].size);
		if (iov[i].value.iov_base == NULL)
			err(1, "malloc");
		if (params[i].value == NULL)
			memset(iov[i].value.iov_base, 0, iov[i].value.iov_len);
	}
	/*
	 * Get the actual prison.  If there are array elements, retry a few
	 * times in case the size changed from under us.
	 */
	if ((jid = jail_get((struct iovec *)iov, 2 * nparams, jflags)) < 0) {
		if (errno != EINVAL || aiov == NULL || errmsg[0])
			return (-1);
		for (sanity = 0;; sanity++) {
			if (sanity == 10)
				return (-1);
			for (ai = 0; ai < narray; ai++)
				if (params[i].flags & PARAM_ARRAY)
					aiov[ai].value.iov_len = 0;
			if (jail_get((struct iovec *)iov, 2 * narray, jflags) <
			    0)
				return (-1);
			for (i = ai = 0; i < nparams; i++) {
				if (!(params[i].flags &
				    (PARAM_KEY | PARAM_ARRAY)))
					continue;
				if (params[i].flags & PARAM_ARRAY) {
					iov[i].value.iov_len =
					    aiov[ai].value.iov_len +
					    ARRAY_SLOP * params[i].size;
					iov[i].value.iov_base =
					    realloc(iov[i].value.iov_base,
					    iov[i].value.iov_len);
					if (iov[i].value.iov_base == NULL)
						err(1, "malloc");
				}
				ai++;
			}
		}
	}
	if (pflags & PRINT_VERBOSE) {
		printf("%6d  %-29.29s %.74s\n"
		       "%6s  %-29.29s %.74s\n"
		       "%6s  %-6d\n",
		    *(int *)iov[0].value.iov_base,
		    (char *)iov[1].value.iov_base,
		    (char *)iov[2].value.iov_base,
		    "",
		    (char *)iov[3].value.iov_base,
		    *(int *)iov[4].value.iov_base ? "DYING" : "ACTIVE",
		    "",
		    *(int *)iov[5].value.iov_base);
		count = iov[6].value.iov_len / sizeof(struct in_addr);
		for (ai = 0; ai < count; ai++)
			if (inet_ntop(AF_INET,
			    &((struct in_addr *)iov[6].value.iov_base)[ai],
			    ipbuf, sizeof(ipbuf)) == NULL)
				err(1, "inet_ntop");
			else
				printf("%6s  %-15.15s\n", "", ipbuf);
		if (!strcmp(params[7].name, "ip6.addr")) {
			count = iov[7].value.iov_len / sizeof(struct in6_addr);
			for (ai = 0; ai < count; ai++)
				if (inet_ntop(AF_INET6, &((struct in_addr *)
				    iov[7].value.iov_base)[ai],
				    ipbuf, sizeof(ipbuf)) == NULL)
					err(1, "inet_ntop");
				else
					printf("%6s  %-15.15s\n", "", ipbuf);
		}
	} else if (pflags & PRINT_DEFAULT)
		printf("%6d  %-15.15s %-29.29s %.74s\n",
		    *(int *)iov[0].value.iov_base,
		    iov[1].value.iov_len == 0 ? "-"
		    : inet_ntoa(*(struct in_addr *)iov[1].value.iov_base),
		    (char *)iov[2].value.iov_base,
		    (char *)iov[3].value.iov_base);
	else {
		for (i = spc = 0; i < nparams; i++) {
			if (!(params[i].flags & PARAM_USER))
				continue;
			if ((pflags & PRINT_SKIP) &&
			    ((!(params[i].flags & PARAM_WR)) ||
			     (params[i].noparent >= 0 &&
			      *(int *)iov[params[i].noparent].value.iov_base)))
				continue;
			if (spc)
				putchar(' ');
			else
				spc = 1;
			if (pflags & PRINT_NAMEVAL) {
				/*
				 * Generally "name=value", but for booleans
				 * either "name" or "noname".
				 */
				switch (params[i].type) {
				case CTLTYPE_BOOL:
					if (*(int *)iov[i].value.iov_base)
						printf("%s", params[i].name);
					else {
						nname = noname(params[i].name);
						printf("%s", nname);
						free(nname);
					}
					break;
				case CTLTYPE_NOBOOL:
					if (*(int *)iov[i].value.iov_base)
						printf("%s", params[i].name);
					else {
						nname =
						    nononame(params[i].name);
						printf("%s", nname);
						free(nname);
					}
					break;
				default:
					printf("%s=", params[i].name);
				}
			}
			count = params[i].flags & PARAM_ARRAY
			    ? iov[i].value.iov_len / params[i].size : 1;
			if (count == 0) {
				if (pflags & PRINT_QUOTED)
					printf("\"\"");
				else if (!(pflags & PRINT_NAMEVAL))
					putchar('-');
			}
			for (ai = 0; ai < count; ai++) {
				if (ai > 0)
					putchar(',');
				switch (params[i].type) {
				case CTLTYPE_INT:
					printf("%d", ((int *)
					    iov[i].value.iov_base)[ai]);
					break;
				case CTLTYPE_UINT:
					printf("%u", ((int *)
					    iov[i].value.iov_base)[ai]);
					break;
				case CTLTYPE_IPADDR:
					if (inet_ntop(AF_INET,
					    &((struct in_addr *)
					    iov[i].value.iov_base)[ai],
					    ipbuf, sizeof(ipbuf)) == NULL)
						err(1, "inet_ntop");
					else
						printf("%s", ipbuf);
					break;
				case CTLTYPE_IP6ADDR:
					if (inet_ntop(AF_INET6,
					    &((struct in6_addr *)
					    iov[i].value.iov_base)[ai],
					    ipbuf, sizeof(ipbuf)) == NULL)
						err(1, "inet_ntop");
					else
						printf("%s", ipbuf);
					break;
				case CTLTYPE_LONG:
					printf("%ld", ((long *)
					    iov[i].value.iov_base)[ai]);
				case CTLTYPE_ULONG:
					printf("%lu", ((long *)
					    iov[i].value.iov_base)[ai]);
					break;
				case CTLTYPE_STRING:
					if (pflags & PRINT_QUOTED)
						quoted_print((char *)
						    iov[i].value.iov_base,
						    params[i].size);
					else
						printf("%.*s",
						    (int)params[i].size,
						    (char *)
						    iov[i].value.iov_base);
					break;
				case CTLTYPE_BOOL:
				case CTLTYPE_NOBOOL:
					if (!(pflags & PRINT_NAMEVAL))
						printf(((int *)
						    iov[i].value.iov_base)[ai]
						    ? "true" : "false");
				}
			}
		}
		putchar('\n');
	}
	for (i = 0; i < nparams; i++)
		if (params[i].value == NULL)
			free(iov[i].value.iov_base);
	return (jid);
}

static void
quoted_print(char *str, int len)
{
	int c, qc;
	char *p = str;
	char *ep = str + len;

	/* An empty string needs quoting. */
	if (!*p) {
		fputs("\"\"", stdout);
		return;
	}

	/*
	 * The value will be surrounded by quotes if it contains spaces
	 * or quotes.
	 */
	qc = strchr(p, '\'') ? '"'
	    : strchr(p, '"') ? '\''
	    : strchr(p, ' ') || strchr(p, '\t') ? '"'
	    : 0;
	if (qc)
		putchar(qc);
	while (p < ep && (c = *p++)) {
		if (c == '\\' || c == qc)
			putchar('\\');
		putchar(c);
	}
	if (qc)
		putchar(qc);
}
