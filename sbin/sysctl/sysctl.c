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
  "$FreeBSD$";
#endif /* not lint */

#include <sys/diskslice.h>	/* used for bootdev parsing */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/reboot.h>		/* used for bootdev parsing */
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int	aflag, bflag, dflag, eflag, Nflag, nflag, oflag, xflag;

static int	oidfmt(int *, int, char *, u_int *);
static void	parse(char *);
static int	show_var(int *, int);
static int	sysctl_all (int *oid, int len);
static int	name2oid(char *, int *);

static void	set_T_dev_t (char *, void **, int *);

static void
usage(void)
{

	(void)fprintf(stderr, "%s\n%s\n",
	    "usage: sysctl [-bdeNnox] variable[=value] ...",
	    "       sysctl [-bdeNnox] -a");
	exit(1);
}

int
main(int argc, char **argv)
{
	int ch;
	setbuf(stdout,0);
	setbuf(stderr,0);

	while ((ch = getopt(argc, argv, "AabdeNnowxX")) != -1) {
		switch (ch) {
		case 'A':
			/* compatibility */
			aflag = oflag = 1;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'b':
			bflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'e':
			eflag = 1;
			break;
		case 'N':
			Nflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'o':
			oflag = 1;
			break;
		case 'w':
			/* compatibility */
			/* ignored */
			break;
		case 'X':
			/* compatibility */
			aflag = xflag = 1;
			break;
		case 'x':
			xflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (Nflag && nflag)
		usage();
	if (aflag && argc == 0)
		exit(sysctl_all(0, 0));
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
	int intval;
	unsigned int uintval;
	long longval;
	unsigned long ulongval;
	size_t newsize = 0;
	quad_t quadval;
	int mib[CTL_MAXNAME];
	char *cp, *bufp, buf[BUFSIZ], fmt[BUFSIZ];
	u_int kind;

	bufp = buf;
	snprintf(buf, BUFSIZ, "%s", string);
	if ((cp = strchr(string, '=')) != NULL) {
		*strchr(buf, '=') = '\0';
		*cp++ = '\0';
		while (isspace(*cp))
			cp++;
		newval = cp;
		newsize = strlen(cp);
	}
	len = name2oid(bufp, mib);

	if (len < 0) 
		errx(1, "unknown oid '%s'", bufp);

	if (oidfmt(mib, len, fmt, &kind))
		err(1, "couldn't find format of oid '%s'", bufp);

	if (newval == NULL) {
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
				newsize = sizeof(intval);
				break;
			case CTLTYPE_UINT:
				uintval = (int) strtoul(newval, NULL, 0);
				newval = &uintval;
				newsize = sizeof uintval;
				break;
			case CTLTYPE_LONG:
				longval = strtol(newval, NULL, 0);
				newval = &longval;
				newsize = sizeof longval;
				break;
			case CTLTYPE_ULONG:
				ulongval = strtoul(newval, NULL, 0);
				newval = &ulongval;
				newsize = sizeof ulongval;
				break;
			case CTLTYPE_STRING:
				break;
			case CTLTYPE_QUAD:
				sscanf(newval, "%qd", &quadval);
				newval = &quadval;
				newsize = sizeof(quadval);
				break;
			case CTLTYPE_OPAQUE:
				if (strcmp(fmt, "T,dev_t") == 0) {
					set_T_dev_t ((char*)newval, &newval, &newsize);
					break;
				}
				/* FALLTHROUGH */
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
	if (l2 != sizeof(*ci))
		err(1, "S_clockinfo %d != %d", l2, sizeof(*ci));
	printf("{ hz = %d, tick = %d, tickadj = %d, profhz = %d, stathz = %d }",
		ci->hz, ci->tick, ci->tickadj, ci->profhz, ci->stathz);
	return (0);
}

static int
S_loadavg(int l2, void *p)
{
	struct loadavg *tv = (struct loadavg*)p;

	if (l2 != sizeof(*tv))
		err(1, "S_loadavg %d != %d", l2, sizeof(*tv));

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

	if (l2 != sizeof(*tv))
		err(1, "S_timeval %d != %d", l2, sizeof(*tv));
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
	if (l2 != sizeof(*d))
		err(1, "T_dev_T %d != %d", l2, sizeof(*d));
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

static void
set_T_dev_t (char *path, void **val, int *size)
{
	static struct stat statb;

	if (strcmp(path, "none") && strcmp(path, "off")) {
		int rc = stat (path, &statb);
		if (rc) {
			err(1, "cannot stat %s", path);
		}

		if (!S_ISCHR(statb.st_mode)) {
			errx(1, "must specify a device special file.");
		}
	} else {
		statb.st_rdev = NODEV;
	}
	*val = (char*) &statb.st_rdev;
	*size = sizeof statb.st_rdev;
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

	j = CTL_MAXNAME * sizeof(int);
	i = sysctl(oid, 2, oidp, &j, name, strlen(name));
	if (i < 0) 
		return i;
	j /= sizeof(int);
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

	j = sizeof(buf);
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
 * Code to map a bootdev major number into a suitable device name.
 * Major numbers are mapped into names as in boot2.c
 */
struct _foo {
	int majdev;
	char *name;
} maj2name[] = {
	30,	"ad",
	0,	"wd",
	1,	"wfd",
	2,	"fd",
	4,	"da",
	-1,	NULL	/* terminator */
};

static int
machdep_bootdev(u_long value)
{
	int majdev, unit, slice, part;
	struct _foo *p;

	if (value & B_MAGICMASK != B_DEVMAGIC) {
		printf("invalid (0x%08x)", value);
		return 0;
	}
	majdev = B_TYPE(value);
	unit = B_UNIT(value);
	slice = B_SLICE(value);
	part = B_PARTITION(value);
	if (majdev == 2) {	/* floppy, as known to the boot block... */
		printf("/dev/fd%d", unit);
		return 0;
	}
	for (p = maj2name; p->name != NULL && p->majdev != majdev ; p++) ;
	if (p->name != NULL) {	/* found */
		if (slice == WHOLE_DISK_SLICE)
			printf("/dev/%s%d%c", p->name, unit, part);
		else
			printf("/dev/%s%ds%d%c",
			    p->name, unit, slice - BASE_SLICE + 1, part + 'a');
	} else
		printf("unknown (major %d unit %d slice %d part %d)",
			majdev, unit, slice, part);
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
	char name[BUFSIZ], *fmt, *sep;
	int qoid[CTL_MAXNAME+2];
	int i;
	size_t j, len;
	u_int kind;
	int (*func)(int, void *);

	qoid[0] = 0;
	memcpy(qoid + 2, oid, nlen * sizeof(int));

	qoid[1] = 1;
	j = sizeof(name);
	i = sysctl(qoid, nlen + 2, name, &j, 0, 0);
	if (i || !j)
		err(1, "sysctl name %d %d %d", i, j, errno);

	if (Nflag) {
		printf("%s", name);
		return (0);
	}

	if (eflag)
		sep = "=";
	else
		sep = ": ";

	if (dflag) {	/* just print description */
		qoid[1] = 5;
		j = sizeof(buf);
		i = sysctl(qoid, nlen + 2, buf, &j, 0, 0);
		if (!nflag)
			printf("%s%s", name, sep);
		printf("%s", buf);
		return (0);
	}
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

	fmt = buf;
	oidfmt(oid, nlen, fmt, &kind);
	p = val;
	switch (*fmt) {
	case 'A':
		if (!nflag)
			printf("%s%s", name, sep);
		printf("%s", p);
		return (0);
		
	case 'I':
		if (!nflag)
			printf("%s%s", name, sep);
		fmt++;
		val = "";
		while (len >= sizeof(int)) {
			if(*fmt == 'U')
				printf("%s%u", val, *(unsigned int *)p);
			else
				printf("%s%d", val, *(int *)p);
			val = " ";
			len -= sizeof(int);
			p += sizeof(int);
		}
		return (0);

	case 'L':
		if (!nflag)
			printf("%s%s", name, sep);
		fmt++;
		if (!strcmp(name, "machdep.guessed_bootdev"))
			return machdep_bootdev(*(unsigned long *)p);
		val = "";
		while (len >= sizeof(long)) {
			if(*fmt == 'U')
				printf("%s%lu", val, *(unsigned long *)p);
			else
				printf("%s%ld", val, *(long *)p);
			val = " ";
			len -= sizeof(long);
			p += sizeof(long);
		}
		return (0);

	case 'P':
		if (!nflag)
			printf("%s%s", name, sep);
		printf("%p", *(void **)p);
		return (0);

	case 'T':
	case 'S':
		i = 0;
		if (strcmp(fmt, "S,clockinfo") == 0)
			func = S_clockinfo;
		else if (strcmp(fmt, "S,timeval") == 0)
			func = S_timeval;
		else if (strcmp(fmt, "S,loadavg") == 0)
			func = S_loadavg;
		else if (strcmp(fmt, "T,dev_t") == 0)
			func = T_dev_t;
		else
			func = NULL;
		if (func) {
			if (!nflag)
				printf("%s%s", name, sep);
			return ((*func)(len, p));
		}
		/* FALL THROUGH */
	default:
		if (!oflag && !xflag)
			return (1);
		if (!nflag)
			printf("%s%s", name, sep);
		printf("Format:%s Length:%d Dump:0x", fmt, len);
		while (len-- && (xflag || p < val + 16))
			printf("%02x", *p++);
		if (!xflag && len > 16)
			printf("...");
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
		memcpy(name1+2, oid, len * sizeof(int));
		l1 += len;
	} else {
		name1[2] = 1;
		l1++;
	}
	for (;;) {
		l2 = sizeof(name2);
		j = sysctl(name1, l1, name2, &l2, 0, 0);
		if (j < 0) {
			if (errno == ENOENT)
				return 0;
			else
				err(1, "sysctl(getnext) %d %d", j, l2);
		}

		l2 /= sizeof(int);

		if (l2 < len)
			return 0;

		for (i = 0; i < len; i++)
			if (name2[i] != oid[i])
				return 0;

		i = show_var(name2, l2);
		if (!i && !bflag)
			putchar('\n');

		memcpy(name1+2, name2, l2 * sizeof(int));
		l1 = 2 + l2;
	}
}
