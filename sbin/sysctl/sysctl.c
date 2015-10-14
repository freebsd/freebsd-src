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

#ifdef __amd64__
#include <sys/efi.h>
#include <machine/metadata.h>
#endif

#if defined(__amd64__) || defined(__i386__)
#include <machine/pc/bios.h>
#endif

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static const char *conffile;

static int	aflag, bflag, dflag, eflag, hflag, iflag;
static int	Nflag, nflag, oflag, qflag, Tflag, Wflag, xflag;

static int	oidfmt(int *, int, char *, u_int *);
static int	parsefile(const char *);
static int	parse(const char *, int);
static int	show_var(int *, int);
static int	sysctl_all(int *oid, int len);
static int	name2oid(const char *, int *);

static int	set_IK(const char *, int *);

static void
usage(void)
{

	(void)fprintf(stderr, "%s\n%s\n",
	    "usage: sysctl [-bdehiNnoqTWx] [-f filename] name[=value] ...",
	    "       sysctl [-bdehNnoqTWx] -a");
	exit(1);
}

int
main(int argc, char **argv)
{
	int ch;
	int warncount = 0;

	setlocale(LC_NUMERIC, "");
	setbuf(stdout,0);
	setbuf(stderr,0);

	while ((ch = getopt(argc, argv, "Aabdef:hiNnoqTwWxX")) != -1) {
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
		case 'f':
			conffile = optarg;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'i':
			iflag = 1;
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
		case 'w':
			/* compatibility */
			/* ignored */
			break;
		case 'W':
			Wflag = 1;
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
	if (argc == 0 && conffile == NULL)
		usage();

	warncount = 0;
	if (conffile != NULL)
		warncount += parsefile(conffile);

	while (argc-- > 0)
		warncount += parse(*argv++, 0);

	return (warncount);
}

/*
 * Parse a name into a MIB entry.
 * Lookup and print out the MIB entry if it exists.
 * Set a new value if requested.
 */
static int
parse(const char *string, int lineno)
{
	int len, i, j;
	void *newval = 0;
	int intval;
	unsigned int uintval;
	long longval;
	unsigned long ulongval;
	size_t newsize = 0;
	int64_t i64val;
	uint64_t u64val;
	int mib[CTL_MAXNAME];
	char *cp, *bufp, buf[BUFSIZ], *endptr, fmt[BUFSIZ], line[BUFSIZ];
	u_int kind;

	if (lineno)
		snprintf(line, sizeof(line), " at line %d", lineno);
	else
		line[0] = '\0';

	cp = buf;
	if (snprintf(buf, BUFSIZ, "%s", string) >= BUFSIZ) {
		warnx("oid too long: '%s'%s", string, line);
		return (1);
	}
	bufp = strsep(&cp, "=:");
	if (cp != NULL) {
		/* Tflag just lists tunables, do not allow assignment */
		if (Tflag || Wflag) {
			warnx("Can't set variables when using -T or -W");
			usage();
		}
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
		newval = cp;
		newsize = strlen(cp);
	}
	/* Trim spaces */
	cp = bufp + strlen(bufp) - 1;
	while (cp >= bufp && isspace((int)*cp)) {
		*cp = '\0';
		cp--;
	}
	len = name2oid(bufp, mib);

	if (len < 0) {
		if (iflag)
			return (0);
		if (qflag)
			return (1);
		else {
			if (errno == ENOENT) {
				warnx("unknown oid '%s'%s", bufp, line);
			} else {
				warn("unknown oid '%s'%s", bufp, line);
			}
			return (1);
		}
	}

	if (oidfmt(mib, len, fmt, &kind)) {
		warn("couldn't find format of oid '%s'%s", bufp, line);
		if (iflag)
			return (1);
		else
			exit(1);
	}

	if (newval == NULL || dflag) {
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
		if ((kind & CTLTYPE) == CTLTYPE_NODE) {
			warnx("oid '%s' isn't a leaf node%s", bufp, line);
			return (1);
		}

		if (!(kind & CTLFLAG_WR)) {
			if (kind & CTLFLAG_TUN) {
				warnx("oid '%s' is a read only tunable%s", bufp, line);
				warnx("Tunable values are set in /boot/loader.conf");
			} else
				warnx("oid '%s' is read only%s", bufp, line);
			return (1);
		}

		if ((kind & CTLTYPE) == CTLTYPE_INT ||
		    (kind & CTLTYPE) == CTLTYPE_UINT ||
		    (kind & CTLTYPE) == CTLTYPE_LONG ||
		    (kind & CTLTYPE) == CTLTYPE_ULONG ||
		    (kind & CTLTYPE) == CTLTYPE_S64 ||
		    (kind & CTLTYPE) == CTLTYPE_U64) {
			if (strlen(newval) == 0) {
				warnx("empty numeric value");
				return (1);
			}
		}

		switch (kind & CTLTYPE) {
			case CTLTYPE_INT:
				if (strcmp(fmt, "IK") == 0) {
					if (!set_IK(newval, &intval)) {
						warnx("invalid value '%s'%s",
						    (char *)newval, line);
						return (1);
					}
 				} else {
					intval = (int)strtol(newval, &endptr,
					    0);
					if (endptr == newval || *endptr != '\0') {
						warnx("invalid integer '%s'%s",
						    (char *)newval, line);
						return (1);
					}
				}
				newval = &intval;
				newsize = sizeof(intval);
				break;
			case CTLTYPE_UINT:
				uintval = (int) strtoul(newval, &endptr, 0);
				if (endptr == newval || *endptr != '\0') {
					warnx("invalid unsigned integer '%s'%s",
					    (char *)newval, line);
					return (1);
				}
				newval = &uintval;
				newsize = sizeof(uintval);
				break;
			case CTLTYPE_LONG:
				longval = strtol(newval, &endptr, 0);
				if (endptr == newval || *endptr != '\0') {
					warnx("invalid long integer '%s'%s",
					    (char *)newval, line);
					return (1);
				}
				newval = &longval;
				newsize = sizeof(longval);
				break;
			case CTLTYPE_ULONG:
				ulongval = strtoul(newval, &endptr, 0);
				if (endptr == newval || *endptr != '\0') {
					warnx("invalid unsigned long integer"
					    " '%s'%s", (char *)newval, line);
					return (1);
				}
				newval = &ulongval;
				newsize = sizeof(ulongval);
				break;
			case CTLTYPE_STRING:
				break;
			case CTLTYPE_S64:
				i64val = strtoimax(newval, &endptr, 0);
				if (endptr == newval || *endptr != '\0') {
					warnx("invalid int64_t '%s'%s",
					    (char *)newval, line);
					return (1);
				}
				newval = &i64val;
				newsize = sizeof(i64val);
				break;
			case CTLTYPE_U64:
				u64val = strtoumax(newval, &endptr, 0);
				if (endptr == newval || *endptr != '\0') {
					warnx("invalid uint64_t '%s'%s",
					    (char *)newval, line);
					return (1);
				}
				newval = &u64val;
				newsize = sizeof(u64val);
				break;
			case CTLTYPE_OPAQUE:
				/* FALLTHROUGH */
			default:
				warnx("oid '%s' is type %d,"
					" cannot set that%s", bufp,
					kind & CTLTYPE, line);
				return (1);
		}

		i = show_var(mib, len);
		if (sysctl(mib, len, 0, 0, newval, newsize) == -1) {
			if (!i && !bflag)
				putchar('\n');
			switch (errno) {
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
				warn("%s%s", string, line);
				return (1);
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

	return (0);
}

static int
parsefile(const char *filename)
{
	FILE *file;
	char line[BUFSIZ], *p, *pq, *pdq;
	int warncount = 0, lineno = 0;

	file = fopen(filename, "r");
	if (file == NULL)
		err(EX_NOINPUT, "%s", filename);
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
	fclose(file);

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
	struct vmtotal *v = (struct vmtotal *)p;
	int pageKilo = getpagesize() / 1024;

	if (l2 != sizeof(*v)) {
		warnx("S_vmtotal %zu != %zu", l2, sizeof(*v));
		return (1);
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
	    "Virtual Memory:\t\t(Total: %dK Active: %dK)\n",
	    v->t_vm * pageKilo, v->t_avm * pageKilo);
	printf("Real Memory:\t\t(Total: %dK Active: %dK)\n",
	    v->t_rm * pageKilo, v->t_arm * pageKilo);
	printf("Shared Virtual Memory:\t(Total: %dK Active: %dK)\n",
	    v->t_vmshr * pageKilo, v->t_avmshr * pageKilo);
	printf("Shared Real Memory:\t(Total: %dK Active: %dK)\n",
	    v->t_rmshr * pageKilo, v->t_armshr * pageKilo);
	printf("Free Memory:\t%dK", v->t_free * pageKilo);

	return (0);
}

#ifdef __amd64__
#define efi_next_descriptor(ptr, size) \
	((struct efi_md *)(((uint8_t *) ptr) + size))

static int
S_efi_map(size_t l2, void *p)
{
	struct efi_map_header *efihdr;
	struct efi_md *map;
	const char *type;
	size_t efisz;
	int ndesc, i;

	static const char *types[] = {
		"Reserved",
		"LoaderCode",
		"LoaderData",
		"BootServicesCode",
		"BootServicesData",
		"RuntimeServicesCode",
		"RuntimeServicesData",
		"ConventionalMemory",
		"UnusableMemory",
		"ACPIReclaimMemory",
		"ACPIMemoryNVS",
		"MemoryMappedIO",
		"MemoryMappedIOPortSpace",
		"PalCode"
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
		if (map->md_type <= EFI_MD_TYPE_PALCODE)
			type = types[map->md_type];
		else
			type = "<INVALID>";
		printf("\n%23s %012lx %12p %08lx ", type, map->md_phys,
		    map->md_virt, map->md_pages);
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
set_IK(const char *str, int *val)
{
	float temp;
	int len, kelv;
	const char *p;
	char *endptr;

	if ((len = strlen(str)) == 0)
		return (0);
	p = &str[len - 1];
	if (*p == 'C' || *p == 'F') {
		temp = strtof(str, &endptr);
		if (endptr == str || endptr != p)
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
name2oid(const char *name, int *oidp)
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
		err(1, "sysctl fmt %d %zu %d", i, j, errno);

	if (kind)
		*kind = *(u_int *)buf;

	if (fmt)
		strcpy(fmt, (char *)(buf + sizeof(u_int)));
	return (0);
}

static int ctl_sign[CTLTYPE+1] = {
	[CTLTYPE_INT] = 1,
	[CTLTYPE_LONG] = 1,
	[CTLTYPE_S64] = 1,
};

static int ctl_size[CTLTYPE+1] = {
	[CTLTYPE_INT] = sizeof(int),
	[CTLTYPE_UINT] = sizeof(u_int),
	[CTLTYPE_LONG] = sizeof(long),
	[CTLTYPE_ULONG] = sizeof(u_long),
	[CTLTYPE_S64] = sizeof(int64_t),
	[CTLTYPE_U64] = sizeof(int64_t),
};

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
	char name[BUFSIZ], fmt[BUFSIZ];
	const char *sep, *sep1;
	int qoid[CTL_MAXNAME+2];
	uintmax_t umv;
	intmax_t mv;
	int i, hexlen, sign, ctltype;
	size_t intlen;
	size_t j, len;
	u_int kind;
	int (*func)(size_t, void *);

	/* Silence GCC. */
	umv = mv = intlen = 0;

	bzero(buf, BUFSIZ);
	bzero(fmt, BUFSIZ);
	bzero(name, BUFSIZ);
	qoid[0] = 0;
	memcpy(qoid + 2, oid, nlen * sizeof(int));

	qoid[1] = 1;
	j = sizeof(name);
	i = sysctl(qoid, nlen + 2, name, &j, 0, 0);
	if (i || !j)
		err(1, "sysctl name %d %zu %d", i, j, errno);

	oidfmt(oid, nlen, fmt, &kind);
	/* if Wflag then only list sysctls that are writeable and not stats. */
	if (Wflag && ((kind & CTLFLAG_WR) == 0 || (kind & CTLFLAG_STATS) != 0))
		return 1;

	/* if Tflag then only list sysctls that are tuneables. */
	if (Tflag && (kind & CTLFLAG_TUN) == 0)
		return 1;

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
		return (1);
	}
	ctltype = (kind & CTLTYPE);
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
		printf("%.*s", (int)len, p);
		free(oval);
		return (0);

	case CTLTYPE_INT:
	case CTLTYPE_UINT:
	case CTLTYPE_LONG:
	case CTLTYPE_ULONG:
	case CTLTYPE_S64:
	case CTLTYPE_U64:
		if (!nflag)
			printf("%s%s", name, sep);
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
				else
					printf("%.1fC", (mv - 2732.0) / 10);
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
		if (strcmp(fmt, "S,clockinfo") == 0)
			func = S_clockinfo;
		else if (strcmp(fmt, "S,timeval") == 0)
			func = S_timeval;
		else if (strcmp(fmt, "S,loadavg") == 0)
			func = S_loadavg;
		else if (strcmp(fmt, "S,vmtotal") == 0)
			func = S_vmtotal;
#ifdef __amd64__
		else if (strcmp(fmt, "S,efi_map_header") == 0)
			func = S_efi_map;
#endif
#if defined(__amd64__) || defined(__i386__)
		else if (strcmp(fmt, "S,bios_smap_xattr") == 0)
			func = S_bios_smap_xattr;
#endif
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
				err(1, "sysctl(getnext) %d %zu", j, l2);
		}

		l2 /= sizeof(int);

		if (len < 0 || l2 < (unsigned int)len)
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
