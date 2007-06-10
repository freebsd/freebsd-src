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

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int	aflag, bflag, dflag, eflag, hflag, Nflag, nflag, oflag;
static int	qflag, xflag;

static int	oidfmt(int *, int, char *, u_int *);
static void	parse(char *);
static int	show_var(int *, int);
static int	sysctl_all(int *oid, int len);
static int	name2oid(char *, int *);

static void	set_T_dev_t(char *, void **, size_t *);
static int	set_IK(char *, int *);

static void
usage(void)
{

	(void)fprintf(stderr, "%s\n%s\n",
	    "usage: sysctl [-bdehNnoqx] name[=value] ...",
	    "       sysctl [-bdehNnoqx] -a");
	exit(1);
}

int
main(int argc, char **argv)
{
	int ch;

	setlocale(LC_NUMERIC, "");
	setbuf(stdout,0);
	setbuf(stderr,0);

	while ((ch = getopt(argc, argv, "AabdehNnoqwxX")) != -1) {
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
		case 'h':
			hflag = 1;
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
		case 'q':
			qflag = 1;
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
	char *cp, *bufp, buf[BUFSIZ], *endptr, fmt[BUFSIZ];
	u_int kind;

	bufp = buf;
	if (snprintf(buf, BUFSIZ, "%s", string) >= BUFSIZ)
		errx(1, "oid too long: '%s'", string);
	if ((cp = strchr(string, '=')) != NULL) {
		*strchr(buf, '=') = '\0';
		*cp++ = '\0';
		while (isspace(*cp))
			cp++;
		newval = cp;
		newsize = strlen(cp);
	}
	len = name2oid(bufp, mib);

	if (len < 0) {
		if (qflag)
			exit(1);
		else
			errx(1, "unknown oid '%s'", bufp);
	}

	if (oidfmt(mib, len, fmt, &kind))
		err(1, "couldn't find format of oid '%s'", bufp);

	if (newval == NULL) {
		if ((kind & CTLTYPE) == CTLTYPE_NODE) {
			if (dflag) {
				i = show_var(mib, len);
				if (!i && !bflag)
					putchar('\n');
			}
			sysctl_all(mib, len);
		} else {
			i = show_var(mib, len);
			if (!i && !bflag)
				putchar('\n');
		}
	} else {
		if ((kind & CTLTYPE) == CTLTYPE_NODE)
			errx(1, "oid '%s' isn't a leaf node", bufp);

		if (!(kind & CTLFLAG_WR)) {
			if (kind & CTLFLAG_TUN) {
				warnx("oid '%s' is a read only tunable", bufp);
				errx(1, "Tunable values are set in /boot/loader.conf");
			} else {
				errx(1, "oid '%s' is read only", bufp);
			}
		}

		if ((kind & CTLTYPE) == CTLTYPE_INT ||
		    (kind & CTLTYPE) == CTLTYPE_UINT ||
		    (kind & CTLTYPE) == CTLTYPE_LONG ||
		    (kind & CTLTYPE) == CTLTYPE_ULONG ||
		    (kind & CTLTYPE) == CTLTYPE_QUAD) {
			if (strlen(newval) == 0)
				errx(1, "empty numeric value");
		}

		switch (kind & CTLTYPE) {
			case CTLTYPE_INT:
				if (strcmp(fmt, "IK") == 0) {
					if (!set_IK(newval, &intval))
						errx(1, "invalid value '%s'",
						    newval);
 				} else {
					intval = (int)strtol(newval, &endptr,
					    0);
					if (endptr == newval || *endptr != '\0')
						errx(1, "invalid integer '%s'",
						    newval);
				}
				newval = &intval;
				newsize = sizeof(intval);
				break;
			case CTLTYPE_UINT:
				uintval = (int) strtoul(newval, &endptr, 0);
				if (endptr == newval || *endptr != '\0')
					errx(1, "invalid unsigned integer '%s'",
					    newval);
				newval = &uintval;
				newsize = sizeof(uintval);
				break;
			case CTLTYPE_LONG:
				longval = strtol(newval, &endptr, 0);
				if (endptr == newval || *endptr != '\0')
					errx(1, "invalid long integer '%s'",
					    newval);
				newval = &longval;
				newsize = sizeof(longval);
				break;
			case CTLTYPE_ULONG:
				ulongval = strtoul(newval, &endptr, 0);
				if (endptr == newval || *endptr != '\0')
					errx(1, "invalid unsigned long integer"
					    " '%s'", newval);
				newval = &ulongval;
				newsize = sizeof(ulongval);
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
					set_T_dev_t (newval, &newval, &newsize);
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

	if (l2 != sizeof(*ci)) {
		warnx("S_clockinfo %d != %d", l2, sizeof(*ci));
		return (0);
	}
	printf(hflag ? "{ hz = %'d, tick = %'d, profhz = %'d, stathz = %'d }" :
		"{ hz = %d, tick = %d, profhz = %d, stathz = %d }",
		ci->hz, ci->tick, ci->profhz, ci->stathz);
	return (0);
}

static int
S_loadavg(int l2, void *p)
{
	struct loadavg *tv = (struct loadavg*)p;

	if (l2 != sizeof(*tv)) {
		warnx("S_loadavg %d != %d", l2, sizeof(*tv));
		return (0);
	}
	printf(hflag ? "{ %'.2f %'.2f %'.2f }" : "{ %.2f %.2f %.2f }",
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

	if (l2 != sizeof(*tv)) {
		warnx("S_timeval %d != %d", l2, sizeof(*tv));
		return (0);
	}
	printf(hflag ? "{ sec = %'ld, usec = %'ld } " :
		"{ sec = %ld, usec = %ld } ",
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
S_vmtotal(int l2, void *p)
{
	struct vmtotal *v = (struct vmtotal *)p;
	int pageKilo = getpagesize() / 1024;

	if (l2 != sizeof(*v)) {
		warnx("S_vmtotal %d != %d", l2, sizeof(*v));
		return (0);
	}

	printf(
	    "\nSystem wide totals computed every five seconds:"
	    " (values in kilobytes)\n");
	printf("===============================================\n");
	printf(
	    "Processes:\t\t(RUNQ: %hd Disk Wait: %hd Page Wait: "
	    "%hd Sleep: %hd)\n",
	    v->t_rq, v->t_dw, v->t_pw, v->t_sl);
	printf(
	    "Virtual Memory:\t\t(Total: %dK, Active %dK)\n",
	    v->t_vm * pageKilo, v->t_avm * pageKilo);
	printf("Real Memory:\t\t(Total: %dK Active %dK)\n",
	    v->t_rm * pageKilo, v->t_arm * pageKilo);
	printf("Shared Virtual Memory:\t(Total: %dK Active: %dK)\n",
	    v->t_vmshr * pageKilo, v->t_avmshr * pageKilo);
	printf("Shared Real Memory:\t(Total: %dK Active: %dK)\n",
	    v->t_rmshr * pageKilo, v->t_armshr * pageKilo);
	printf("Free Memory Pages:\t%dK\n", v->t_free * pageKilo);

	return (0);
}

static int
T_dev_t(int l2, void *p)
{
	dev_t *d = (dev_t *)p;

	if (l2 != sizeof(*d)) {
		warnx("T_dev_T %d != %d", l2, sizeof(*d));
		return (0);
	}
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
set_T_dev_t(char *path, void **val, size_t *size)
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
	*val = (void *) &statb.st_rdev;
	*size = sizeof(statb.st_rdev);
}

static int
set_IK(char *str, int *val)
{
	float temp;
	int len, kelv;
	char *p, *endptr;

	if ((len = strlen(str)) == 0)
		return (0);
	p = &str[len - 1];
	if (*p == 'C' || *p == 'F') {
		*p = '\0';
		temp = strtof(str, &endptr);
		if (endptr == str || *endptr != '\0')
			return (0);
		if (*p == 'F')
			temp = (temp - 32) * 5 / 9;
		kelv = temp * 10 + 2732;
	} else {
		kelv = (int)strtol(str, &endptr, 10);
		if (endptr == str || *endptr != '\0')
			return (0);
	}
	*val = kelv;
	return (1);
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
		return (i);
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
	return (0);
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
	u_char buf[BUFSIZ], *val, *oval, *p;
	char name[BUFSIZ], *fmt, *sep;
	int qoid[CTL_MAXNAME+2];
	uintmax_t umv;
	intmax_t mv;
	u_quad_t uqv;
	quad_t qv;
	u_long ulv;
	long lv;
	u_int uiv;
	int i, flen, iv;
	size_t intlen;
	size_t j, len;
	u_int kind;
	int (*func)(int, void *);

	bzero(buf, BUFSIZ);
	bzero(name, BUFSIZ);
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

	val = oval = malloc(j + 1);
	if (val == NULL) {
		warnx("malloc failed");
		return (-1);
	}
	len = j;
	i = sysctl(oid, nlen, val, &len, 0, 0);
	if (i || !len) {
		free(oval);
		return (1);
	}

	if (bflag) {
		fwrite(val, 1, len, stdout);
		free(oval);
		return (0);
	}
	val[len] = '\0';
	fmt = buf;
	oidfmt(oid, nlen, fmt, &kind);
	p = val;
	switch (*fmt) {
	case 'A':
		if (!nflag)
			printf("%s%s", name, sep);
		printf("%.*s", len, p);
		free(oval);
		return (0);

	case 'I':
	case 'L':
	case 'Q':
		if (!nflag)
			printf("%s%s", name, sep);
		switch (*fmt) {
		case 'I': intlen = sizeof(int); flen = 10; break;
		case 'L': intlen = sizeof(long); flen = 18; break;
		case 'Q': intlen = sizeof(quad_t); flen = 18; break;
		}
		val = "";
		while (len >= intlen) {
			switch (*fmt) {
			case 'I':
				memcpy(&uiv, p, intlen); umv = uiv;
				memcpy(&iv, p, intlen); mv = iv;
				break;
			case 'L':
				memcpy(&ulv, p, intlen); umv = ulv;
				memcpy(&lv, p, intlen); mv = lv;
				break;
			case 'Q':
				memcpy(&uqv, p, intlen); umv = uqv;
				memcpy(&qv, p, intlen); mv = qv;
				break;
			}
			fputs(val, stdout);
			if (fmt[1] == 'U')
				printf(hflag ? "%'ju" : "%ju", umv);
			else if (fmt[1] == 'X')
				printf(hflag ? "%'#0*jx" : "%#0*jx", flen, umv);
			else if (fmt[1] == 'K') {
				if (*(int *)p < 0)
					printf("%jd", mv);
				else
					printf("%.1fC", (mv - 2732.0) / 10);
			} else
				printf(hflag ? "%'d" : "%d", mv);
			val = " ";
			len -= intlen;
			p += intlen;
		}
		free(oval);
		return (0);

	case 'P':
		if (!nflag)
			printf("%s%s", name, sep);
		printf("%p", *(void **)p);
		free(oval);
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
		else if (strcmp(fmt, "S,vmtotal") == 0)
			func = S_vmtotal;
		else if (strcmp(fmt, "T,dev_t") == 0)
			func = T_dev_t;
		else
			func = NULL;
		if (func) {
			if (!nflag)
				printf("%s%s", name, sep);
			i = (*func)(len, p);
			free(oval);
			return (i);
		}
		/* FALLTHROUGH */
	default:
		if (!oflag && !xflag) {
			free(oval);
			return (1);
		}
		if (!nflag)
			printf("%s%s", name, sep);
		printf("Format:%s Length:%d Dump:0x", fmt, len);
		while (len-- && (xflag || p < val + 16))
			printf("%02x", *p++);
		if (!xflag && len > 16)
			printf("...");
		free(oval);
		return (0);
	}
	free(oval);
	return (1);
}

static int
sysctl_all(int *oid, int len)
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
				return (0);
			else
				err(1, "sysctl(getnext) %d %d", j, l2);
		}

		l2 /= sizeof(int);

		if (l2 < len)
			return (0);

		for (i = 0; i < len; i++)
			if (name2[i] != oid[i])
				return (0);

		i = show_var(name2, l2);
		if (!i && !bflag)
			putchar('\n');

		memcpy(name1+2, name2, l2 * sizeof(int));
		l1 = 2 + l2;
	}
}
