/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)from: sysctl.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD: src/sbin/sysctl/sysctl.c,v 1.25.2.1 2000/07/19 06:22:20 kbyanc Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/resource.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int	Aflag, aflag, bflag, nflag, wflag, Xflag;

static int	oidfmt(int *, int, char *, u_int *);
static void	parse(char *);
static int	show_var(int *, int);
static int	sysctl_all (int *oid, int len);
static int	name2oid(char *, int *);

static void
usage(void)
{

	(void)fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n",
		"usage: sysctl [-bn] variable ...",
		"       sysctl [-bn] -w variable=value ...",
		"       sysctl [-bn] -a",
		"       sysctl [-bn] -A",
		"       sysctl [-bn] -X");
	exit(1);
}

int
main(int argc, char **argv)
{
	int ch;
	setbuf(stdout,0);
	setbuf(stderr,0);

	while ((ch = getopt(argc, argv, "AabnwX")) != -1) {
		switch (ch) {
		case 'A': Aflag = 1; break;
		case 'a': aflag = 1; break;
		case 'b': bflag = 1; break;
		case 'n': nflag = 1; break;
		case 'w': wflag = 1; break;
		case 'X': Xflag = Aflag = 1; break;
		default: usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (wflag && (Aflag || aflag))
		usage();
	if (Aflag || aflag)
		exit (sysctl_all(0, 0));
	if (argc == 0)
		usage();
	while (argc-- > 0)
		parse(*argv++);
	exit(0);
}

/*
 * Parse a name into a MIB entry.
 * Lookup and print out the MIB entry if it exists.
 * Set a new value if requested.
 */
static void
parse(char *string)
{
	int len, i, j;
	void *newval = 0;
	int intval, newsize = 0;
	quad_t quadval;
	int mib[CTL_MAXNAME];
	char *cp, *bufp, buf[BUFSIZ];
	u_int kind;

	bufp = buf;
	snprintf(buf, BUFSIZ, "%s", string);
	if ((cp = strchr(string, '=')) != NULL) {
		if (!wflag)
			errx(2, "must specify -w to set variables");
		*strchr(buf, '=') = '\0';
		*cp++ = '\0';
		while (isspace(*cp))
			cp++;
		newval = cp;
		newsize = strlen(cp);
	} else {
		if (wflag)
			usage();
	}
	len = name2oid(bufp, mib);

	if (len < 0) 
		errx(1, "unknown oid '%s'", bufp);

	if (oidfmt(mib, len, 0, &kind))
		err(1, "couldn't find format of oid '%s'", bufp);

	if (!wflag) {
		if ((kind & CTLTYPE) == CTLTYPE_NODE) {
			sysctl_all(mib, len);
		} else {
			i = show_var(mib, len);
			if (!i && !bflag)
				putchar('\n');
		}
	} else {
		if ((kind & CTLTYPE) == CTLTYPE_NODE)
			errx(1, "oid '%s' isn't a leaf node", bufp);

		if (!(kind&CTLFLAG_WR))
			errx(1, "oid '%s' is read only", bufp);
	
		switch (kind & CTLTYPE) {
			case CTLTYPE_INT:
				intval = (int) strtol(newval, NULL, 0);
				newval = &intval;
				newsize = sizeof intval;
				break;
				break;
			case CTLTYPE_STRING:
				break;
			case CTLTYPE_QUAD:
				break;
				sscanf(newval, "%qd", &quadval);
				newval = &quadval;
				newsize = sizeof quadval;
				break;
			default:
				errx(1, "oid '%s' is type %d,"
					" cannot set that", bufp,
					kind & CTLTYPE);
		}

		i = show_var(mib, len);
		if (sysctl(mib, len, 0, 0, newval, newsize) == -1) {
			if (!i && !bflag)
				putchar('\n');
			switch (errno) {
			case EOPNOTSUPP:
				errx(1, "%s: value is not available", 
					string);
			case ENOTDIR:
				errx(1, "%s: specification is incomplete", 
					string);
			case ENOMEM:
				errx(1, "%s: type is unknown to this program", 
					string);
			default:
				warn("%s", string);
				return;
			}
		}
		if (!bflag)
			printf(" -> ");
		i = nflag;
		nflag = 1;
		j = show_var(mib, len);
		if (!j && !bflag)
			putchar('\n');
		nflag = i;
	}
}

/* These functions will dump out various interesting structures. */

static int
S_clockinfo(int l2, void *p)
{
	struct clockinfo *ci = (struct clockinfo*)p;
	if (l2 != sizeof *ci)
		err(1, "S_clockinfo %d != %d", l2, sizeof *ci);
	printf("{ hz = %d, tick = %d, tickadj = %d, profhz = %d, stathz = %d }",
		ci->hz, ci->tick, ci->tickadj, ci->profhz, ci->stathz);
	return (0);
}

static int
S_loadavg(int l2, void *p)
{
	struct loadavg *tv = (struct loadavg*)p;

	if (l2 != sizeof *tv)
		err(1, "S_loadavg %d != %d", l2, sizeof *tv);

	printf("{ %.2f %.2f %.2f }",
		(double)tv->ldavg[0]/(double)tv->fscale,
		(double)tv->ldavg[1]/(double)tv->fscale,
		(double)tv->ldavg[2]/(double)tv->fscale);
	return (0);
}

static int
S_timeval(int l2, void *p)
{
	struct timeval *tv = (struct timeval*)p;
	time_t tv_sec;
	char *p1, *p2;

	if (l2 != sizeof *tv)
		err(1, "S_timeval %d != %d", l2, sizeof *tv);
	printf("{ sec = %ld, usec = %ld } ",
		tv->tv_sec, tv->tv_usec);
	tv_sec = tv->tv_sec;
	p1 = strdup(ctime(&tv_sec));
	for (p2=p1; *p2 ; p2++)
		if (*p2 == '\n')
			*p2 = '\0';
	fputs(p1, stdout);
	return (0);
}

static int
T_dev_t(int l2, void *p)
{
	dev_t *d = (dev_t *)p;
	if (l2 != sizeof *d)
		err(1, "T_dev_T %d != %d", l2, sizeof *d);
	if ((int)(*d) != -1) {
		if (minor(*d) > 255 || minor(*d) < 0)
			printf("{ major = %d, minor = 0x%x }",
				major(*d), minor(*d));
		else
			printf("{ major = %d, minor = %d }",
				major(*d), minor(*d));
	}
	return (0);
}

/*
 * These functions uses a presently undocumented interface to the kernel
 * to walk the tree and get the type so it can print the value.
 * This interface is under work and consideration, and should probably
 * be killed with a big axe by the first person who can find the time.
 * (be aware though, that the proper interface isn't as obvious as it
 * may seem, there are various conflicting requirements.
 */

static int
name2oid(char *name, int *oidp)
{
	int oid[2];
	int i;
	size_t j;

	oid[0] = 0;
	oid[1] = 3;

	j = CTL_MAXNAME * sizeof (int);
	i = sysctl(oid, 2, oidp, &j, name, strlen(name));
	if (i < 0) 
		return i;
	j /= sizeof (int);
	return (j);
}

static int
oidfmt(int *oid, int len, char *fmt, u_int *kind)
{
	int qoid[CTL_MAXNAME+2];
	u_char buf[BUFSIZ];
	int i;
	size_t j;

	qoid[0] = 0;
	qoid[1] = 4;
	memcpy(qoid + 2, oid, len * sizeof(int));

	j = sizeof buf;
	i = sysctl(qoid, len + 2, buf, &j, 0, 0);
	if (i)
		err(1, "sysctl fmt %d %d %d", i, j, errno);

	if (kind)
		*kind = *(u_int *)buf;

	if (fmt)
		strcpy(fmt, (char *)(buf + sizeof(u_int)));
	return 0;
}

/*
 * This formats and outputs the value of one variable
 *
 * Returns zero if anything was actually output.
 * Returns one if didn't know what to do with this.
 * Return minus one if we had errors.
 */

static int
show_var(int *oid, int nlen)
{
	u_char buf[BUFSIZ], *val, *p;
	char name[BUFSIZ], descr[BUFSIZ], *fmt;
	int qoid[CTL_MAXNAME+2];
	int i;
	size_t j, len;
	u_int kind;
	int (*func)(int, void *) = 0;

	qoid[0] = 0;
	memcpy(qoid + 2, oid, nlen * sizeof(int));

	qoid[1] = 1;
	j = sizeof name;
	i = sysctl(qoid, nlen + 2, name, &j, 0, 0);
	if (i || !j)
		err(1, "sysctl name %d %d %d", i, j, errno);

	/* find an estimate of how much we need for this var */
	j = 0;
	i = sysctl(oid, nlen, 0, &j, 0, 0);
	j += j; /* we want to be sure :-) */

	val = alloca(j);
	len = j;
	i = sysctl(oid, nlen, val, &len, 0, 0);
	if (i || !len)
		return (1);

	if (bflag) {
		fwrite(val, 1, len, stdout);
		return (0);
	}

	qoid[1] = 4;
	j = sizeof buf;
	i = sysctl(qoid, nlen + 2, buf, &j, 0, 0);
	if (i || !j)
		err(1, "sysctl fmt %d %d %d", i, j, errno);

	kind = *(u_int *)buf;

	fmt = (char *)(buf + sizeof(u_int));

	p = val;
	switch (*fmt) {
	case 'A':
		if (!nflag)
			printf("%s: ", name);
		printf("%s", p);
		return (0);
		
	case 'I':
		if (!nflag)
			printf("%s: ", name);
		fmt++;
		val = "";
		while (len >= sizeof(int)) {
			if(*fmt == 'U')
				printf("%s%u", val, *(unsigned int *)p);
			else
				printf("%s%d", val, *(int *)p);
			val = " ";
			len -= sizeof (int);
			p += sizeof (int);
		}
		return (0);

	case 'L':
		if (!nflag)
			printf("%s: ", name);
		fmt++;
		val = "";
		while (len >= sizeof(long)) {
			if(*fmt == 'U')
				printf("%s%lu", val, *(unsigned long *)p);
			else
				printf("%s%ld", val, *(long *)p);
			val = " ";
			len -= sizeof (int);
			p += sizeof (int);
		}
		return (0);

	case 'P':
		if (!nflag)
			printf("%s: ", name);
		printf("%p", *(void **)p);
		return (0);

	case 'T':
	case 'S':
		i = 0;
		if (!strcmp(fmt, "S,clockinfo"))	func = S_clockinfo;
		else if (!strcmp(fmt, "S,timeval"))	func = S_timeval;
		else if (!strcmp(fmt, "S,loadavg"))	func = S_loadavg;
		else if (!strcmp(fmt, "T,dev_t"))	func = T_dev_t;
		if (func) {
			if (!nflag)
				printf("%s: ", name);
			return ((*func)(len, p));
		}
		/* FALL THROUGH */
	default:
		if (!Aflag)
			return (1);
		if (!nflag)
			printf("%s: ", name);
		printf("Format:%s Length:%d Dump:0x", fmt, len);
		while (len--) {
			printf("%02x", *p++);
			if (Xflag || p < val+16)
				continue;
			printf("...");
			break;
		}
		return (0);
	}
	return (1);
}

static int
sysctl_all (int *oid, int len)
{
	int name1[22], name2[22];
	int i, j;
	size_t l1, l2;

	name1[0] = 0;
	name1[1] = 2;
	l1 = 2;
	if (len) {
		memcpy(name1+2, oid, len*sizeof (int));
		l1 += len;
	} else {
		name1[2] = 1;
		l1++;
	}
	while (1) {
		l2 = sizeof name2;
		j = sysctl(name1, l1, name2, &l2, 0, 0);
		if (j < 0) {
			if (errno == ENOENT)
				return 0;
			else
				err(1, "sysctl(getnext) %d %d", j, l2);
		}

		l2 /= sizeof (int);

		if (l2 < len)
			return 0;

		for (i = 0; i < len; i++)
			if (name2[i] != oid[i])
				return 0;

		i = show_var(name2, l2);
		if (!i && !bflag)
			putchar('\n');

		memcpy(name1+2, name2, l2*sizeof (int));
		l1 = 2 + l2;
	}
}
