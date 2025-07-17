/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#ifdef JAIL
#include <sys/jail.h>
#endif
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <dev/evdev/input.h>

#ifdef __amd64__
#include <sys/efi.h>
#include <machine/metadata.h>
#endif

#if defined(__amd64__) || defined(__i386__)
#include <machine/pc/bios.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#ifdef JAIL
#include <jail.h>
#endif
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#ifdef JAIL
static const char *jailname;
#endif
static const char *conffile;

static int	aflag, bflag, Bflag, dflag, eflag, hflag, iflag;
static int	Nflag, nflag, oflag, qflag, tflag, Tflag, Wflag, xflag;
static bool	Fflag, Jflag, lflag, Vflag;

static void	attach_jail(void);
static int	oidfmt(int *, int, char *, u_int *);
static int	parsefile(FILE *);
static int	parse(const char *, int);
static int	show_var(int *, int, bool);
static int	sysctl_all(int *, int);
static int	name2oid(const char *, int *);

static int	strIKtoi(const char *, char **, const char *);

static int ctl_sign[CTLTYPE+1] = {
	[CTLTYPE_INT] = 1,
	[CTLTYPE_LONG] = 1,
	[CTLTYPE_S8] = 1,
	[CTLTYPE_S16] = 1,
	[CTLTYPE_S32] = 1,
	[CTLTYPE_S64] = 1,
};

static int ctl_size[CTLTYPE+1] = {
	[CTLTYPE_INT] = sizeof(int),
	[CTLTYPE_UINT] = sizeof(u_int),
	[CTLTYPE_LONG] = sizeof(long),
	[CTLTYPE_ULONG] = sizeof(u_long),
	[CTLTYPE_S8] = sizeof(int8_t),
	[CTLTYPE_S16] = sizeof(int16_t),
	[CTLTYPE_S32] = sizeof(int32_t),
	[CTLTYPE_S64] = sizeof(int64_t),
	[CTLTYPE_U8] = sizeof(uint8_t),
	[CTLTYPE_U16] = sizeof(uint16_t),
	[CTLTYPE_U32] = sizeof(uint32_t),
	[CTLTYPE_U64] = sizeof(uint64_t),
};

static const char *ctl_typename[CTLTYPE+1] = {
	[CTLTYPE_INT] = "integer",
	[CTLTYPE_UINT] = "unsigned integer",
	[CTLTYPE_LONG] = "long integer",
	[CTLTYPE_ULONG] = "unsigned long",
	[CTLTYPE_U8] = "uint8_t",
	[CTLTYPE_U16] = "uint16_t",
	[CTLTYPE_U32] = "uint32_t",
	[CTLTYPE_U64] = "uint64_t",
	[CTLTYPE_S8] = "int8_t",
	[CTLTYPE_S16] = "int16_t",
	[CTLTYPE_S32] = "int32_t",
	[CTLTYPE_S64] = "int64_t",
	[CTLTYPE_NODE] = "node",
	[CTLTYPE_STRING] = "string",
	[CTLTYPE_OPAQUE] = "opaque",
};

static void
usage(void)
{

	(void)fprintf(stderr, "%s\n%s\n",
	    "usage: sysctl [-j jail] [-bdeFhiJlNnoqTtVWx] [ -B <bufsize> ] [-f filename] name[=value] ...",
	    "       sysctl [-j jail] [-bdeFhJlNnoqTtVWx] [ -B <bufsize> ] -a");
	exit(1);
}

int
main(int argc, char **argv)
{
	int ch;
	int warncount = 0;
	FILE *file = NULL;

	setlocale(LC_NUMERIC, "");
	setbuf(stdout,0);
	setbuf(stderr,0);

	while ((ch = getopt(argc, argv, "AaB:bdeFf:hiJj:lNnoqTtVWwXx")) != -1) {
		switch (ch) {
		case 'A':
			/* compatibility */
			aflag = oflag = 1;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'B':
			Bflag = strtol(optarg, NULL, 0);
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
		case 'F':
			Fflag = true;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'J':
			Jflag = true;
			break;
		case 'j':
#ifdef JAIL
			if ((jailname = optarg) == NULL)
				usage();
#else
			errx(1, "not built with jail support");
#endif
			break;
		case 'l':
			lflag = true;
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
		case 'T':
			Tflag = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'V':
			Vflag = true;
			break;
		case 'W':
			Wflag = 1;
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

	/* Nflag is name only and doesn't make sense to combine with these */
	/* TODO: few other combinations do not make sense but come back later */
	if (Nflag && (lflag || nflag))
		usage();
	if (aflag && argc == 0) {
		attach_jail();
		exit(sysctl_all(NULL, 0));
	}
	if (argc == 0 && conffile == NULL)
		usage();

	if (conffile != NULL) {
		file = fopen(conffile, "r");
		if (file == NULL)
			err(EX_NOINPUT, "%s", conffile);
	}
	attach_jail();
	if (file != NULL) {
		warncount += parsefile(file);
		fclose(file);
	}

	while (argc-- > 0)
		warncount += parse(*argv++, 0);

	return (warncount);
}

static void
attach_jail(void)
{
#ifdef JAIL
	int jid;

	if (jailname == NULL)
		return;

	jid = jail_getid(jailname);
	if (jid == -1)
		errx(1, "jail not found");
	if (jail_attach(jid) != 0)
		errx(1, "cannot attach to jail");
#endif
}

/*
 * Parse a single numeric value, append it to 'newbuf', and update
 * 'newsize'.  Returns true if the value was parsed and false if the
 * value was invalid.  Non-numeric types (strings) are handled
 * directly in parse().
 */
static bool
parse_numeric(const char *newvalstr, const char *fmt, u_int kind,
    void **newbufp, size_t *newsizep)
{
	void *newbuf;
	const void *newval;
	int8_t i8val;
	uint8_t u8val;
	int16_t i16val;
	uint16_t u16val;
	int32_t i32val;
	uint32_t u32val;
	int intval;
	unsigned int uintval;
	long longval;
	unsigned long ulongval;
	int64_t i64val;
	uint64_t u64val;
	size_t valsize;
	char *endptr = NULL;
	
	errno = 0;

	switch (kind & CTLTYPE) {
	case CTLTYPE_INT:
		if (strncmp(fmt, "IK", 2) == 0)
			intval = strIKtoi(newvalstr, &endptr, fmt);
		else
			intval = (int)strtol(newvalstr, &endptr, 0);
		newval = &intval;
		valsize = sizeof(intval);
		break;
	case CTLTYPE_UINT:
		uintval = (int) strtoul(newvalstr, &endptr, 0);
		newval = &uintval;
		valsize = sizeof(uintval);
		break;
	case CTLTYPE_LONG:
		longval = strtol(newvalstr, &endptr, 0);
		newval = &longval;
		valsize = sizeof(longval);
		break;
	case CTLTYPE_ULONG:
		ulongval = strtoul(newvalstr, &endptr, 0);
		newval = &ulongval;
		valsize = sizeof(ulongval);
		break;
	case CTLTYPE_S8:
		i8val = (int8_t)strtol(newvalstr, &endptr, 0);
		newval = &i8val;
		valsize = sizeof(i8val);
		break;
	case CTLTYPE_S16:
		i16val = (int16_t)strtol(newvalstr, &endptr, 0);
		newval = &i16val;
		valsize = sizeof(i16val);
		break;
	case CTLTYPE_S32:
		i32val = (int32_t)strtol(newvalstr, &endptr, 0);
		newval = &i32val;
		valsize = sizeof(i32val);
		break;
	case CTLTYPE_S64:
		i64val = strtoimax(newvalstr, &endptr, 0);
		newval = &i64val;
		valsize = sizeof(i64val);
		break;
	case CTLTYPE_U8:
		u8val = (uint8_t)strtoul(newvalstr, &endptr, 0);
		newval = &u8val;
		valsize = sizeof(u8val);
		break;
	case CTLTYPE_U16:
		u16val = (uint16_t)strtoul(newvalstr, &endptr, 0);
		newval = &u16val;
		valsize = sizeof(u16val);
		break;
	case CTLTYPE_U32:
		u32val = (uint32_t)strtoul(newvalstr, &endptr, 0);
		newval = &u32val;
		valsize = sizeof(u32val);
		break;
	case CTLTYPE_U64:
		u64val = strtoumax(newvalstr, &endptr, 0);
		newval = &u64val;
		valsize = sizeof(u64val);
		break;
	default:
		/* NOTREACHED */
		abort();
	}
	
	if (errno != 0 || endptr == newvalstr ||
	    (endptr != NULL && *endptr != '\0'))
		return (false);

	newbuf = realloc(*newbufp, *newsizep + valsize);
	if (newbuf == NULL)
		err(1, "out of memory");
	memcpy((char *)newbuf + *newsizep, newval, valsize);
	*newbufp = newbuf;
	*newsizep += valsize;
	
	return (true);
}

/*
 * Parse a name into a MIB entry.
 * Lookup and print out the MIB entry if it exists.
 * Set a new value if requested.
 */
static int
parse(const char *string, int lineno)
{
	int len, i, j, save_errno;
	const void *newval;
	char *newvalstr = NULL;
	void *newbuf;
	size_t newsize = Bflag;
	int mib[CTL_MAXNAME];
	char *cp, *bufp, *buf, fmt[BUFSIZ], line[BUFSIZ];
	u_int kind;

	if (lineno)
		snprintf(line, sizeof(line), " at line %d", lineno);
	else
		line[0] = '\0';

	/*
	 * Split the string into name and value.
	 *
	 * Either = or : may be used as the delimiter.
	 * Whitespace surrounding the delimiter is trimmed.
	 * Quotes around the value are stripped.
	 */
	cp = buf = strdup(string);
	bufp = strsep(&cp, "=:");
	if (cp != NULL) {
		/* Tflag just lists tunables, do not allow assignment */
		if (Tflag || Wflag) {
			warnx("Can't set variables when using -T or -W");
			usage();
		}
		/* Trim whitespace before the value. */
		while (isspace(*cp))
			cp++;
		/* Strip a pair of " or ' if any. */
		switch (*cp) {
		case '\"':
		case '\'':
			if (cp[strlen(cp) - 1] == *cp)
				cp[strlen(cp) - 1] = '\0';
			cp++;
		}
		newvalstr = cp;
		newsize = strlen(cp);
	}
	/* Trim whitespace after the name. */
	cp = bufp + strlen(bufp) - 1;
	while (cp >= bufp && isspace((int)*cp)) {
		*cp = '\0';
		cp--;
	}

	/*
	 * Check the name is a useable oid.
	 */
	len = name2oid(bufp, mib);
	if (len < 0) {
		if (iflag) {
			free(buf);
			return (0);
		}
		if (!qflag) {
			if (errno == ENOENT) {
				warnx("unknown oid '%s'%s", bufp, line);
			} else {
				warn("unknown oid '%s'%s", bufp, line);
			}
		}
		free(buf);
		return (1);
	}

	if (oidfmt(mib, len, fmt, &kind)) {
		warn("couldn't find format of oid '%s'%s", bufp, line);
		free(buf);
		if (iflag)
			return (1);
		else
			exit(1);
	}

	/*
	 * We have a useable oid to work with.  If there is no value given,
	 * show the node and its children.  Otherwise, set the new value.
	 */
	if (newvalstr == NULL || dflag) {
		free(buf);
		if ((kind & CTLTYPE) == CTLTYPE_NODE) {
			if (dflag) {
				i = show_var(mib, len, false);
				if (!i && !bflag)
					putchar('\n');
			}
			sysctl_all(mib, len);
		} else {
			i = show_var(mib, len, false);
			if (!i && !bflag)
				putchar('\n');
		}
		return (0);
	}

	/*
	 * We have a new value to set.  Check its validity and parse if numeric.
	 */
	if ((kind & CTLTYPE) == CTLTYPE_NODE) {
		warnx("oid '%s' isn't a leaf node%s", bufp, line);
		free(buf);
		return (1);
	}

	if (!(kind & CTLFLAG_WR)) {
		if (kind & CTLFLAG_TUN) {
			warnx("oid '%s' is a read only tunable%s", bufp, line);
			warnx("Tunable values are set in /boot/loader.conf");
		} else
			warnx("oid '%s' is read only%s", bufp, line);
		free(buf);
		return (1);
	}

	switch (kind & CTLTYPE) {
	case CTLTYPE_INT:
	case CTLTYPE_UINT:
	case CTLTYPE_LONG:
	case CTLTYPE_ULONG:
	case CTLTYPE_S8:
	case CTLTYPE_S16:
	case CTLTYPE_S32:
	case CTLTYPE_S64:
	case CTLTYPE_U8:
	case CTLTYPE_U16:
	case CTLTYPE_U32:
	case CTLTYPE_U64:
		if (strlen(newvalstr) == 0) {
			warnx("empty numeric value");
			free(buf);
			return (1);
		}
		/* FALLTHROUGH */
	case CTLTYPE_STRING:
		break;
	default:
		warnx("oid '%s' is type %d, cannot set that%s",
		    bufp, kind & CTLTYPE, line);
		free(buf);
		return (1);
	}

	newbuf = NULL;

	switch (kind & CTLTYPE) {
	case CTLTYPE_STRING:
		newval = newvalstr;
		break;
	default:
		newsize = 0;
		while ((cp = strsep(&newvalstr, " ,")) != NULL) {
			if (*cp == '\0')
				continue;
			if (!parse_numeric(cp, fmt, kind, &newbuf, &newsize)) {
				warnx("invalid %s '%s'%s",
				    ctl_typename[kind & CTLTYPE], cp, line);
				free(newbuf);
				free(buf);
				return (1);
			}
		}
		newval = newbuf;
		break;
	}

	/*
	 * Show the current value, then set and show the new value.
	 */
	i = show_var(mib, len, false);
	if (sysctl(mib, len, 0, 0, newval, newsize) == -1) {
		save_errno = errno;
		free(newbuf);
		free(buf);
		if (!i && !bflag)
			putchar('\n');
		switch (save_errno) {
		case EOPNOTSUPP:
			warnx("%s: value is not available%s",
			    string, line);
			return (1);
		case ENOTDIR:
			warnx("%s: specification is incomplete%s",
			    string, line);
			return (1);
		case ENOMEM:
			warnx("%s: type is unknown to this program%s",
			    string, line);
			return (1);
		default:
			warnc(save_errno, "%s%s", string, line);
			return (1);
		}
	}
	free(newbuf);
	free(buf);
	if (!bflag)
		printf(" -> ");
	i = nflag;
	nflag = 1;
	j = show_var(mib, len, false);
	if (!j && !bflag)
		putchar('\n');
	nflag = i;

	return (0);
}

static int
parsefile(FILE *file)
{
	char line[BUFSIZ], *p, *pq, *pdq;
	int warncount = 0, lineno = 0;

	while (fgets(line, sizeof(line), file) != NULL) {
		lineno++;
		p = line;
		pq = strchr(line, '\'');
		pdq = strchr(line, '\"');
		/* Replace the first # with \0. */
		while((p = strchr(p, '#')) != NULL) {
			if (pq != NULL && p > pq) {
				if ((p = strchr(pq+1, '\'')) != NULL)
					*(++p) = '\0';
				break;
			} else if (pdq != NULL && p > pdq) {
				if ((p = strchr(pdq+1, '\"')) != NULL)
					*(++p) = '\0';
				break;
			} else if (p == line || *(p-1) != '\\') {
				*p = '\0';
				break;
			}
			p++;
		}
		/* Trim spaces */
		p = line + strlen(line) - 1;
		while (p >= line && isspace((int)*p)) {
			*p = '\0';
			p--;
		}
		p = line;
		while (isspace((int)*p))
			p++;
		if (*p == '\0')
			continue;
		else
			warncount += parse(p, lineno);
	}

	return (warncount);
}

/* These functions will dump out various interesting structures. */

static int
S_clockinfo(size_t l2, void *p)
{
	struct clockinfo *ci = (struct clockinfo*)p;

	if (l2 != sizeof(*ci)) {
		warnx("S_clockinfo %zu != %zu", l2, sizeof(*ci));
		return (1);
	}
	printf(hflag ? "{ hz = %'d, tick = %'d, profhz = %'d, stathz = %'d }" :
		"{ hz = %d, tick = %d, profhz = %d, stathz = %d }",
		ci->hz, ci->tick, ci->profhz, ci->stathz);
	return (0);
}

static int
S_loadavg(size_t l2, void *p)
{
	struct loadavg *tv = (struct loadavg*)p;

	if (l2 != sizeof(*tv)) {
		warnx("S_loadavg %zu != %zu", l2, sizeof(*tv));
		return (1);
	}
	printf(hflag ? "{ %'.2f %'.2f %'.2f }" : "{ %.2f %.2f %.2f }",
		(double)tv->ldavg[0]/(double)tv->fscale,
		(double)tv->ldavg[1]/(double)tv->fscale,
		(double)tv->ldavg[2]/(double)tv->fscale);
	return (0);
}

static int
S_timeval(size_t l2, void *p)
{
	struct timeval *tv = (struct timeval*)p;
	time_t tv_sec;
	char *p1, *p2;

	if (l2 != sizeof(*tv)) {
		warnx("S_timeval %zu != %zu", l2, sizeof(*tv));
		return (1);
	}
	printf(hflag ? "{ sec = %'jd, usec = %'ld } " :
		"{ sec = %jd, usec = %ld } ",
		(intmax_t)tv->tv_sec, tv->tv_usec);
	tv_sec = tv->tv_sec;
	p1 = strdup(ctime(&tv_sec));
	for (p2=p1; *p2 ; p2++)
		if (*p2 == '\n')
			*p2 = '\0';
	fputs(p1, stdout);
	free(p1);
	return (0);
}

static int
S_vmtotal(size_t l2, void *p)
{
	struct vmtotal *v;
	int pageKilo;

	if (l2 != sizeof(*v)) {
		warnx("S_vmtotal %zu != %zu", l2, sizeof(*v));
		return (1);
	}

	v = p;
	pageKilo = getpagesize() / 1024;

#define	pg2k(a)	((uintmax_t)(a) * pageKilo)
	printf("\nSystem wide totals computed every five seconds:"
	    " (values in kilobytes)\n");
	printf("===============================================\n");
	printf("Processes:\t\t(RUNQ: %d Disk Wait: %d Page Wait: "
	    "%d Sleep: %d)\n",
	    v->t_rq, v->t_dw, v->t_pw, v->t_sl);
	printf("Virtual Memory:\t\t(Total: %juK Active: %juK)\n",
	    pg2k(v->t_vm), pg2k(v->t_avm));
	printf("Real Memory:\t\t(Total: %juK Active: %juK)\n",
	    pg2k(v->t_rm), pg2k(v->t_arm));
	printf("Shared Virtual Memory:\t(Total: %juK Active: %juK)\n",
	    pg2k(v->t_vmshr), pg2k(v->t_avmshr));
	printf("Shared Real Memory:\t(Total: %juK Active: %juK)\n",
	    pg2k(v->t_rmshr), pg2k(v->t_armshr));
	printf("Free Memory:\t%juK", pg2k(v->t_free));
	return (0);
}

static int
S_input_id(size_t l2, void *p)
{
	struct input_id *id = p;

	if (l2 != sizeof(*id)) {
		warnx("S_input_id %zu != %zu", l2, sizeof(*id));
		return (1);
	}

	printf("{ bustype = 0x%04x, vendor = 0x%04x, "
	    "product = 0x%04x, version = 0x%04x }",
	    id->bustype, id->vendor, id->product, id->version);
	return (0);
}

static int
S_pagesizes(size_t l2, void *p)
{
	char buf[256];
	u_long *ps;
	size_t l;
	int i;

	l = snprintf(buf, sizeof(buf), "{ ");
	ps = p;
	for (i = 0; i * sizeof(*ps) < l2 && ps[i] != 0 && l < sizeof(buf);
	    i++) {
		l += snprintf(&buf[l], sizeof(buf) - l,
		    "%s%lu", i == 0 ? "" : ", ", ps[i]);
	}
	if (l < sizeof(buf))
		(void)snprintf(&buf[l], sizeof(buf) - l, " }");

	printf("%s", buf);

	return (0);
}

#ifdef __amd64__
static int
S_efi_map(size_t l2, void *p)
{
	struct efi_map_header *efihdr;
	struct efi_md *map;
	const char *type;
	size_t efisz;
	int ndesc, i;

	static const char * const types[] = {
		[EFI_MD_TYPE_NULL] =	"Reserved",
		[EFI_MD_TYPE_CODE] =	"LoaderCode",
		[EFI_MD_TYPE_DATA] =	"LoaderData",
		[EFI_MD_TYPE_BS_CODE] =	"BootServicesCode",
		[EFI_MD_TYPE_BS_DATA] =	"BootServicesData",
		[EFI_MD_TYPE_RT_CODE] =	"RuntimeServicesCode",
		[EFI_MD_TYPE_RT_DATA] =	"RuntimeServicesData",
		[EFI_MD_TYPE_FREE] =	"ConventionalMemory",
		[EFI_MD_TYPE_BAD] =	"UnusableMemory",
		[EFI_MD_TYPE_RECLAIM] =	"ACPIReclaimMemory",
		[EFI_MD_TYPE_FIRMWARE] = "ACPIMemoryNVS",
		[EFI_MD_TYPE_IOMEM] =	"MemoryMappedIO",
		[EFI_MD_TYPE_IOPORT] =	"MemoryMappedIOPortSpace",
		[EFI_MD_TYPE_PALCODE] =	"PalCode",
		[EFI_MD_TYPE_PERSISTENT] = "PersistentMemory",
	};

	/*
	 * Memory map data provided by UEFI via the GetMemoryMap
	 * Boot Services API.
	 */
	if (l2 < sizeof(*efihdr)) {
		warnx("S_efi_map length less than header");
		return (1);
	}
	efihdr = p;
	efisz = (sizeof(struct efi_map_header) + 0xf) & ~0xf;
	map = (struct efi_md *)((uint8_t *)efihdr + efisz);

	if (efihdr->descriptor_size == 0)
		return (0);
	if (l2 != efisz + efihdr->memory_size) {
		warnx("S_efi_map length mismatch %zu vs %zu", l2, efisz +
		    efihdr->memory_size);
		return (1);
	}
	ndesc = efihdr->memory_size / efihdr->descriptor_size;

	printf("\n%23s %12s %12s %8s %4s",
	    "Type", "Physical", "Virtual", "#Pages", "Attr");

	for (i = 0; i < ndesc; i++,
	    map = efi_next_descriptor(map, efihdr->descriptor_size)) {
		type = NULL;
		if (map->md_type < nitems(types))
			type = types[map->md_type];
		if (type == NULL)
			type = "<INVALID>";
		printf("\n%23s %012jx %012jx %08jx ", type,
		    (uintmax_t)map->md_phys, (uintmax_t)map->md_virt,
		    (uintmax_t)map->md_pages);
		if (map->md_attr & EFI_MD_ATTR_UC)
			printf("UC ");
		if (map->md_attr & EFI_MD_ATTR_WC)
			printf("WC ");
		if (map->md_attr & EFI_MD_ATTR_WT)
			printf("WT ");
		if (map->md_attr & EFI_MD_ATTR_WB)
			printf("WB ");
		if (map->md_attr & EFI_MD_ATTR_UCE)
			printf("UCE ");
		if (map->md_attr & EFI_MD_ATTR_WP)
			printf("WP ");
		if (map->md_attr & EFI_MD_ATTR_RP)
			printf("RP ");
		if (map->md_attr & EFI_MD_ATTR_XP)
			printf("XP ");
		if (map->md_attr & EFI_MD_ATTR_RT)
			printf("RUNTIME");
	}
	return (0);
}
#endif

#if defined(__amd64__) || defined(__i386__)
static int
S_bios_smap_xattr(size_t l2, void *p)
{
	struct bios_smap_xattr *smap, *end;

	if (l2 % sizeof(*smap) != 0) {
		warnx("S_bios_smap_xattr %zu is not a multiple of %zu", l2,
		    sizeof(*smap));
		return (1);
	}

	end = (struct bios_smap_xattr *)((char *)p + l2);
	for (smap = p; smap < end; smap++)
		printf("\nSMAP type=%02x, xattr=%02x, base=%016jx, len=%016jx",
		    smap->type, smap->xattr, (uintmax_t)smap->base,
		    (uintmax_t)smap->length);
	return (0);
}
#endif

static int
strIKtoi(const char *str, char **endptrp, const char *fmt)
{
	int kelv;
	float temp;
	size_t len;
	const char *p;
	int prec, i;

	assert(errno == 0);

	len = strlen(str);
	/* caller already checked this */
	assert(len > 0);

	/*
	 * A format of "IK" is in deciKelvin. A format of "IK3" is in
	 * milliKelvin. The single digit following IK is log10 of the
	 * multiplying factor to convert Kelvin into the untis of this sysctl,
	 * or the dividing factor to convert the sysctl value to Kelvin. Numbers
	 * larger than 6 will run into precision issues with 32-bit integers.
	 * Characters that aren't ASCII digits after the 'K' are ignored. No
	 * localization is present because this is an interface from the kernel
	 * to this program (eg not an end-user interface), so isdigit() isn't
	 * used here.
	 */
	if (fmt[2] != '\0' && fmt[2] >= '0' && fmt[2] <= '9')
		prec = fmt[2] - '0';
	else
		prec = 1;
	p = &str[len - 1];
	if (*p == 'C' || *p == 'F' || *p == 'K') {
		temp = strtof(str, endptrp);
		if (*endptrp != str && *endptrp == p && errno == 0) {
			if (*p == 'F')
				temp = (temp - 32) * 5 / 9;
			*endptrp = NULL;
			if (*p != 'K')
				temp += 273.15;
			for (i = 0; i < prec; i++)
				temp *= 10.0;
			return ((int)(temp + 0.5));
		}
	} else {
		/* No unit specified -> treat it as a raw number */
		kelv = (int)strtol(str, endptrp, 10);
		if (*endptrp != str && *endptrp == p && errno == 0) {
			*endptrp = NULL;
			return (kelv);
		}
	}

	errno = ERANGE;
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
name2oid(const char *name, int *oidp)
{
	int oid[2];
	int i;
	size_t j;

	oid[0] = CTL_SYSCTL;
	oid[1] = CTL_SYSCTL_NAME2OID;

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

	qoid[0] = CTL_SYSCTL;
	qoid[1] = CTL_SYSCTL_OIDFMT;
	memcpy(qoid + 2, oid, len * sizeof(int));

	j = sizeof(buf);
	i = sysctl(qoid, len + 2, buf, &j, 0, 0);
	if (i)
		err(1, "sysctl fmt %d %zu %d", i, j, errno);

	if (kind)
		*kind = *(u_int *)buf;

	if (fmt)
		strcpy(fmt, (char *)(buf + sizeof(u_int)));
	return (0);
}

/*
 * This displays a combination of name, type, format, and/or description.
 *
 * Returns zero if anything was actually output.
 * Returns one if there is an error.
 */
static int
show_info(char *name, const char *sep, int ctltype, char *fmt, int *qoid, int nlen)
{
	u_char buf[BUFSIZ];
	const char *prntype;
	int error = 0, i;
	size_t j;

	if (!nflag)
		printf("%s%s", name, sep);
	if (tflag) {
		if (ctl_typename[ctltype] != NULL)
			prntype = ctl_typename[ctltype];
		else {
			prntype = "unknown";
			error++;
		}
		if (Fflag || dflag)
			printf("%s%s", prntype, sep);
		else
			fputs(prntype, stdout);
	}
	if (Fflag) {
		if (!isprint(fmt[0])) /* Few codes doesn't have formats */
			fmt = "";
		if (dflag)
			printf("%s%s", fmt, sep);
		else
			fputs(fmt, stdout);
	}
	if (!dflag)
		return (error);

	qoid[1] = CTL_SYSCTL_OIDDESCR;
	bzero(buf, BUFSIZ);
	j = sizeof(buf);
	i = sysctl(qoid, nlen + 2, buf, &j, 0, 0);
	if (i < 0) {
		putchar('\n');
		return (1);
	}
	fputs(buf, stdout);
	return (error);
}

/*
 * This formats and outputs the value of one variable
 *
 * Returns zero if anything was actually output.
 * Returns one if didn't know what to do with this.
 * Return minus one if we had errors.
 */
static int
show_var(int *oid, int nlen, bool honor_skip)
{
	static int skip_len = 0, skip_oid[CTL_MAXNAME];
	u_char *val, *oval, *p;
	char name[BUFSIZ], fmt[BUFSIZ];
	const char *sep, *sep1;
	int qoid[CTL_MAXNAME+2];
	uintmax_t umv;
	intmax_t mv;
	int i, hexlen, sign, ctltype;
	size_t intlen;
	size_t j, len;
	u_int kind;
	float base;
	int (*func)(size_t, void *);
	int prec;

	/* Silence GCC. */
	umv = mv = intlen = 0;

	bzero(fmt, BUFSIZ);
	bzero(name, BUFSIZ);
	qoid[0] = CTL_SYSCTL;
	qoid[1] = CTL_SYSCTL_NAME;
	memcpy(qoid + 2, oid, nlen * sizeof(int));
	j = sizeof(name);
	i = sysctl(qoid, nlen + 2, name, &j, 0, 0);
	if (i || !j)
		err(1, "sysctl name %d %zu %d", i, j, errno);

	oidfmt(oid, nlen, fmt, &kind);
	/* if Wflag then only list sysctls that are writeable and not stats. */
	if (Wflag && ((kind & CTLFLAG_WR) == 0 || (kind & CTLFLAG_STATS) != 0))
		return (1);

	/* if Jflag then only list sysctls that are prison variables. */
	if (Jflag && (kind & CTLFLAG_PRISON) == 0)
		return (1);

	/* if Tflag then only list sysctls that are tuneables. */
	if (Tflag && (kind & CTLFLAG_TUN) == 0)
		return (1);

	/* if Vflag then only list sysctls that are vnet variables. */
	if (Vflag && (kind & CTLFLAG_VNET) == 0)
		return (1);

	if (Nflag) {
		printf("%s", name);
		return (0);
	}

	if (eflag)
		sep = "=";
	else
		sep = ": ";

	ctltype = (kind & CTLTYPE);
	if (tflag || Fflag || dflag)
		return show_info(name, sep, ctltype, fmt, qoid, nlen);

	/* keep track of encountered skip nodes, ignoring descendants */
	if ((skip_len == 0 || skip_len >= nlen * (int)sizeof(int)) &&
	    (kind & CTLFLAG_SKIP) != 0) {
		/* Save this oid so we can skip descendants. */
		skip_len = nlen * sizeof(int);
		memcpy(skip_oid, oid, skip_len);
	}

	/* bail before fetching the value if we're honoring skip */
	if (honor_skip) {
		if (0 < skip_len && skip_len <= nlen * (int)sizeof(int) &&
		    memcmp(skip_oid, oid, skip_len) == 0)
			return (1);
		/* Not a skip node or descendant of a skip node. */
		skip_len = 0;
	}

	/* don't fetch opaques that we don't know how to print */
	if (ctltype == CTLTYPE_OPAQUE) {
		if (strcmp(fmt, "S,clockinfo") == 0)
			func = S_clockinfo;
		else if (strcmp(fmt, "S,timeval") == 0)
			func = S_timeval;
		else if (strcmp(fmt, "S,loadavg") == 0)
			func = S_loadavg;
		else if (strcmp(fmt, "S,vmtotal") == 0)
			func = S_vmtotal;
		else if (strcmp(fmt, "S,input_id") == 0)
			func = S_input_id;
		else if (strcmp(fmt, "S,pagesizes") == 0)
			func = S_pagesizes;
#ifdef __amd64__
		else if (strcmp(fmt, "S,efi_map_header") == 0)
			func = S_efi_map;
#endif
#if defined(__amd64__) || defined(__i386__)
		else if (strcmp(fmt, "S,bios_smap_xattr") == 0)
			func = S_bios_smap_xattr;
#endif
		else {
			func = NULL;
			if (!bflag && !oflag && !xflag)
				return (1);
		}
	}

	/* find an estimate of how much we need for this var */
	if (Bflag)
		j = Bflag;
	else {
		j = 0;
		i = sysctl(oid, nlen, 0, &j, 0, 0);
		j += j; /* we want to be sure :-) */
	}

	val = oval = malloc(j + 1);
	if (val == NULL) {
		warnx("malloc failed");
		return (1);
	}
	len = j;
	i = sysctl(oid, nlen, val, &len, 0, 0);
	if (i != 0 || (len == 0 && ctltype != CTLTYPE_STRING)) {
		free(oval);
		return (1);
	}

	if (bflag) {
		fwrite(val, 1, len, stdout);
		free(oval);
		return (0);
	}
	val[len] = '\0';
	p = val;
	sign = ctl_sign[ctltype];
	intlen = ctl_size[ctltype];

	switch (ctltype) {
	case CTLTYPE_STRING:
		if (!nflag)
			printf("%s%s", name, sep);
		if (lflag)
			printf("%zd%s", len, sep);
		printf("%.*s", (int)len, p);
		free(oval);
		return (0);

	case CTLTYPE_INT:
	case CTLTYPE_UINT:
	case CTLTYPE_LONG:
	case CTLTYPE_ULONG:
	case CTLTYPE_S8:
	case CTLTYPE_S16:
	case CTLTYPE_S32:
	case CTLTYPE_S64:
	case CTLTYPE_U8:
	case CTLTYPE_U16:
	case CTLTYPE_U32:
	case CTLTYPE_U64:
		if (!nflag)
			printf("%s%s", name, sep);
		if (lflag)
			printf("%zd%s", len, sep);
		hexlen = 2 + (intlen * CHAR_BIT + 3) / 4;
		sep1 = "";
		while (len >= intlen) {
			switch (kind & CTLTYPE) {
			case CTLTYPE_INT:
			case CTLTYPE_UINT:
				umv = *(u_int *)p;
				mv = *(int *)p;
				break;
			case CTLTYPE_LONG:
			case CTLTYPE_ULONG:
				umv = *(u_long *)p;
				mv = *(long *)p;
				break;
			case CTLTYPE_S8:
			case CTLTYPE_U8:
				umv = *(uint8_t *)p;
				mv = *(int8_t *)p;
				break;
			case CTLTYPE_S16:
			case CTLTYPE_U16:
				umv = *(uint16_t *)p;
				mv = *(int16_t *)p;
				break;
			case CTLTYPE_S32:
			case CTLTYPE_U32:
				umv = *(uint32_t *)p;
				mv = *(int32_t *)p;
				break;
			case CTLTYPE_S64:
			case CTLTYPE_U64:
				umv = *(uint64_t *)p;
				mv = *(int64_t *)p;
				break;
			}
			fputs(sep1, stdout);
			if (xflag)
				printf("%#0*jx", hexlen, umv);
			else if (!sign)
				printf(hflag ? "%'ju" : "%ju", umv);
			else if (fmt[1] == 'K') {
				if (mv < 0)
					printf("%jd", mv);
				else {
					/*
					 * See strIKtoi for details on fmt.
					 */
					prec = 1;
					if (fmt[2] != '\0')
						prec = fmt[2] - '0';
					base = 1.0;
					for (int i = 0; i < prec; i++)
						base *= 10.0;
					printf("%.*fC", prec,
					    (float)mv / base - 273.15);
				}
			} else
				printf(hflag ? "%'jd" : "%jd", mv);
			sep1 = " ";
			len -= intlen;
			p += intlen;
		}
		free(oval);
		return (0);

	case CTLTYPE_OPAQUE:
		i = 0;
		if (func) {
			if (!nflag)
				printf("%s%s", name, sep);
			if (lflag)
				printf("%zd%s", len, sep);
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
		if (lflag)
			printf("%zd%s", len, sep);
		printf("Format:%s Length:%zu Dump:0x", fmt, len);
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

	name1[0] = CTL_SYSCTL;
	name1[1] = (oid != NULL || Nflag || dflag || tflag) ?
	    CTL_SYSCTL_NEXTNOSKIP : CTL_SYSCTL_NEXT;
	l1 = 2;
	if (len) {
		memcpy(name1 + 2, oid, len * sizeof(int));
		l1 += len;
	} else {
		name1[2] = CTL_KERN;
		l1++;
	}
	for (;;) {
		l2 = sizeof(name2);
		j = sysctl(name1, l1, name2, &l2, 0, 0);
		if (j < 0) {
			if (errno == ENOENT)
				return (0);
			else
				err(1, "sysctl(getnext) %d %zu", j, l2);
		}

		l2 /= sizeof(int);

		if (len < 0 || l2 < (unsigned int)len)
			return (0);

		if (memcmp(name2, oid, len * sizeof(int)) != 0)
			return (0);

		i = show_var(name2, l2, true);
		if (!i && !bflag)
			putchar('\n');

		memcpy(name1 + 2, name2, l2 * sizeof(int));
		l1 = 2 + l2;
	}
}
