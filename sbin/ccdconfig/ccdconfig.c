/*	$NetBSD: ccdconfig.c,v 1.2.2.1 1995/11/11 02:43:35 thorpej Exp $	*/

/*
 * Copyright (c) 1995 Jason R. Thorpe.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project
 *	by Jason R. Thorpe.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/linker.h>
#include <sys/disklabel.h>
#include <sys/stat.h>
#include <sys/module.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/devicestat.h>
#include <sys/ccdvar.h>

#include "pathnames.h"

static	int lineno = 0;
static	int verbose = 0;
static	const char *ccdconf = _PATH_CCDCONF;

struct	flagval {
	const char	*fv_flag;
	int	fv_val;
} flagvaltab[] = {
	{ "CCDF_UNIFORM",	CCDF_UNIFORM },
	{ "CCDF_MIRROR",	CCDF_MIRROR },
	{ NULL,			0 },
};

#define CCD_CONFIG		0	/* configure a device */
#define CCD_CONFIGALL		1	/* configure all devices */
#define CCD_UNCONFIG		2	/* unconfigure a device */
#define CCD_UNCONFIGALL		3	/* unconfigure all devices */
#define CCD_DUMP		4	/* dump a ccd's configuration */

static	int checkdev(char *);
static	int do_io(int, u_long, struct ccd_ioctl *);
static	int do_single(int, char **, int);
static	int do_all(int);
static	int dump_ccd(int, char **);
static	int flags_to_val(char *);
static	void print_ccd_info(struct ccd_s *);
static	int resolve_ccdname(char *);
static	void usage(void);

int
main(int argc, char *argv[])
{
	int ch, options = 0, action = CCD_CONFIG;

	while ((ch = getopt(argc, argv, "cCf:guUv")) != -1) {
		switch (ch) {
		case 'c':
			action = CCD_CONFIG;
			++options;
			break;

		case 'C':
			action = CCD_CONFIGALL;
			++options;
			break;

		case 'f':
			ccdconf = optarg;
			break;

		case 'g':
			action = CCD_DUMP;
			break;

		case 'u':
			action = CCD_UNCONFIG;
			++options;
			break;

		case 'U':
			action = CCD_UNCONFIGALL;
			++options;
			break;

		case 'v':
			verbose = 1;
			break;

		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (options > 1)
		usage();

	if (modfind("ccd") < 0) {
		/* Not present in kernel, try loading it */
		if (kldload("ccd") < 0 || modfind("ccd") < 0)
			warn("ccd module not available!");
	}

	switch (action) {
		case CCD_CONFIG:
		case CCD_UNCONFIG:
			exit(do_single(argc, argv, action));
			/* NOTREACHED */

		case CCD_CONFIGALL:
		case CCD_UNCONFIGALL:
			exit(do_all(action));
			/* NOTREACHED */

		case CCD_DUMP:
			exit(dump_ccd(argc, argv));
			/* NOTREACHED */
	}
	/* NOTREACHED */
	return (0);
}

static int
do_single(int argc, char **argv, int action)
{
	struct ccd_ioctl ccio;
	char *cp, *cp2, **disks;
	int ccd, noflags = 0, i, ileave, flags = 0, j;
	u_int u;

	bzero(&ccio, sizeof(ccio));

	/*
	 * If unconfiguring, all arguments are treated as ccds.
	 */
	if (action == CCD_UNCONFIG || action == CCD_UNCONFIGALL) {
		for (i = 0; argc != 0; ) {
			cp = *argv++; --argc;
			if ((ccd = resolve_ccdname(cp)) == NULL) {
				warnx("invalid ccd name: %s", cp);
				i = 1;
				continue;
			}
			ccio.ccio_size = ccd;
			if (do_io(ccd, CCDIOCCLR, &ccio))
				i = 1;
			else
				if (verbose)
					printf("%s unconfigured\n", cp);
		}
		return (i);
	}

	/* Make sure there are enough arguments. */
	if (argc < 4) {
		if (argc == 3) {
			/* Assume that no flags are specified. */
			noflags = 1;
		} else {
			if (action == CCD_CONFIGALL) {
				warnx("%s: bad line: %d", ccdconf, lineno);
				return (1);
			} else
				usage();
		}
	}

	/* First argument is the ccd to configure. */
	cp = *argv++; --argc;
	if ((ccd = resolve_ccdname(cp)) == NULL) {
		warnx("invalid ccd name: %s", cp);
		return (1);
	}

	/* Next argument is the interleave factor. */
	cp = *argv++; --argc;
	errno = 0;	/* to check for ERANGE */
	ileave = (int)strtol(cp, &cp2, 10);
	if ((errno == ERANGE) || (ileave < 0) || (*cp2 != '\0')) {
		warnx("invalid interleave factor: %s", cp);
		return (1);
	}

	if (noflags == 0) {
		/* Next argument is the ccd configuration flags. */
		cp = *argv++; --argc;
		if ((flags = flags_to_val(cp)) < 0) {
			warnx("invalid flags argument: %s", cp);
			return (1);
		}
	}

	/* Next is the list of disks to make the ccd from. */
	disks = malloc(argc * sizeof(char *));
	if (disks == NULL) {
		warnx("no memory to configure ccd");
		return (1);
	}
	for (i = 0; argc != 0; ) {
		cp = *argv++; --argc;
		if ((j = checkdev(cp)) == 0)
			disks[i++] = cp;
		else {
			warnx("%s: %s", cp, strerror(j));
			return (1);
		}
	}

	/* Fill in the ccio. */
	ccio.ccio_disks = disks;
	ccio.ccio_ndisks = i;
	ccio.ccio_ileave = ileave;
	ccio.ccio_flags = flags;
	ccio.ccio_size = ccd;

	if (do_io(ccd, CCDIOCSET, &ccio)) {
		free(disks);
		return (1);
	}

	if (verbose) {
		printf("ccd%d: %d components ", ccio.ccio_unit,
		    ccio.ccio_ndisks);
		for (u = 0; u < ccio.ccio_ndisks; ++u) {
			if ((cp2 = strrchr(disks[u], '/')) != NULL)
				++cp2;
			else
				cp2 = disks[u];
			printf("%c%s%c",
			    u == 0 ? '(' : ' ', cp2,
			    u == ccio.ccio_ndisks - 1 ? ')' : ',');
		}
		printf(", %lu blocks ", (u_long)ccio.ccio_size);
		if (ccio.ccio_ileave != 0)
			printf("interleaved at %d blocks\n", ccio.ccio_ileave);
		else
			printf("concatenated\n");
	}

	free(disks);
	return (0);
}

static int
do_all(int action)
{
	FILE *f;
	char line[_POSIX2_LINE_MAX];
	char *cp, **argv;
	int argc, rval;
	gid_t egid;

	rval = 0;
	egid = getegid();
	setegid(getgid());
	if ((f = fopen(ccdconf, "r")) == NULL) {
		setegid(egid);
		warn("fopen: %s", ccdconf);
		return (1);
	}
	setegid(egid);

	while (fgets(line, sizeof(line), f) != NULL) {
		argc = 0;
		argv = NULL;
		++lineno;
		if ((cp = strrchr(line, '\n')) != NULL)
			*cp = '\0';

		/* Break up the line and pass it's contents to do_single(). */
		if (line[0] == '\0')
			goto end_of_line;
		for (cp = line; (cp = strtok(cp, " \t")) != NULL; cp = NULL) {
			if (*cp == '#')
				break;
			if ((argv = realloc(argv,
			    sizeof(char *) * ++argc)) == NULL) {
				warnx("no memory to configure ccds");
				return (1);
			}
			argv[argc - 1] = cp;
			/*
			 * If our action is to unconfigure all, then pass
			 * just the first token to do_single() and ignore
			 * the rest.  Since this will be encountered on
			 * our first pass through the line, the Right
			 * Thing will happen.
			 */
			if (action == CCD_UNCONFIGALL) {
				if (do_single(argc, argv, action))
					rval = 1;
				goto end_of_line;
			}
		}
		if (argc != 0)
			if (do_single(argc, argv, action))
				rval = 1;

 end_of_line:
		if (argv != NULL)
			free(argv);
	}

	(void)fclose(f);
	return (rval);
}

static int
checkdev(char *path)
{
	struct stat st;

	if (stat(path, &st) != 0)
		return (errno);

	if (!S_ISBLK(st.st_mode) && !S_ISCHR(st.st_mode))
		return (EINVAL);

	return (0);
}

static int
resolve_ccdname(char *name)
{

	if (!strncmp(name, _PATH_DEV, strlen(_PATH_DEV)))
		name += strlen(_PATH_DEV);
	if (strncmp(name, "ccd", 3))
		return -1;
	name += 3;
	if (!isdigit(*name))
		return -1;
	return (strtoul(name, NULL, 10));
}

static int
do_io(int unit, u_long cmd, struct ccd_ioctl *cciop)
{
	int fd;
	char *cp;
	char *path;

	asprintf(&path, "%s%s", _PATH_DEV, _PATH_CCDCTL);

	if ((fd = open(path, O_RDWR, 0640)) < 0) {
		asprintf(&path, "%sccd%dc", _PATH_DEV, unit);
		if ((fd = open(path, O_RDWR, 0640)) < 0) {
			warn("open: %s", path);
			return (1);
		}
		fprintf(stderr,
		    "***WARNING***: Kernel older than ccdconfig(8), please upgrade it.\n");
		fprintf(stderr,
		    "***WARNING***: Continuing in 30 seconds\n");
		sleep(30);
	}

	if (ioctl(fd, cmd, cciop) < 0) {
		switch (cmd) {
		case CCDIOCSET:
			cp = "CCDIOCSET";
			break;

		case CCDIOCCLR:
			cp = "CCDIOCCLR";
			break;

		case CCDCONFINFO:
			cp = "CCDCONFINFO";
			break;

		case CCDCPPINFO:
			cp = "CCDCPPINFO";
			break;

		default:
			cp = "unknown";
		}
		warn("ioctl (%s): %s", cp, path);
		return (1);
	}

	return (0);
}

static int
dump_ccd(int argc, char **argv)
{
	char *cp;
	int i, error, numccd, numconfiged = 0;
	struct ccdconf conf;
	int ccd;

	/*
	 * Read the ccd configuration data from the kernel and dump
	 * it to stdout.
	 */
	if ((ccd = resolve_ccdname("ccd0")) < 0) {		/* XXX */
		warnx("invalid ccd name: %s", cp);
		return (1);
	}
	conf.size = 0;
	if (do_io(ccd, CCDCONFINFO, (struct ccd_ioctl *) &conf))
		return (1);
	if (conf.size == 0) {
		printf("no concatenated disks configured\n");
		return (0);
	}
	/* Allocate space for the configuration data. */
	conf.buffer = alloca(conf.size);
	if (conf.buffer == NULL) {
		warnx("no memory for configuration data");
		return (1);
	}
	if (do_io(ccd, CCDCONFINFO, (struct ccd_ioctl *) &conf))
		return (1);

	numconfiged = conf.size / sizeof(struct ccd_s);

	if (argc == 0) {
		for (i = 0; i < numconfiged; i++)
			print_ccd_info(&(conf.buffer[i]));
	} else {
		while (argc) {
			cp = *argv++; --argc;
			if ((ccd = resolve_ccdname(cp)) < 0) {
				warnx("invalid ccd name: %s", cp);
				continue;
			}
			error = 1;
			for (i = 0; i < numconfiged; i++) {
				if (conf.buffer[i].sc_unit == ccd) {
					print_ccd_info(&(conf.buffer[i]));
					error = 0;
					break;
				}
			}
			if (error) {
				warnx("ccd%d not configured", numccd);
				continue;
			}
		}
	}	

	return (0);
}

static void
print_ccd_info(struct ccd_s *cs)
{
	char *cp;
	static int header_printed = 0;
	struct ccdcpps cpps;
	int ccd;

	/* Print out header if necessary*/
	if (header_printed == 0 && verbose) {
		printf("# ccd\t\tileave\tflags\tcompnent devices\n");
		header_printed = 1;
	}

	/* Dump out softc information. */
	printf("ccd%d\t\t%d\t%d\t", cs->sc_unit, cs->sc_ileave,
	    cs->sc_cflags & CCDF_USERMASK);
	fflush(stdout);

	/* Read in the component info. */
	asprintf(&cp, "%s%d", cs->device_stats.device_name,
	    cs->device_stats.unit_number);
	if (cp == NULL) {
		printf("\n");
		warn("ccd%d: can't allocate memory",
		    cs->sc_unit);
		return;
	}

	if ((ccd = resolve_ccdname(cp)) < 0) {
		printf("\n");
		warnx("can't read component info: invalid ccd name: %s", cp);
		return;
	}
	cpps.size = 1024;
	cpps.buffer = alloca(cpps.size);
	memcpy(cpps.buffer, &ccd, sizeof ccd);
	if (do_io(ccd, CCDCPPINFO, (struct ccd_ioctl *) &cpps)) {
		printf("\n");
		warnx("can't read component info");
		return;
	}

	/* Display component info. */
	for (cp = cpps.buffer; *cp && cp - cpps.buffer < cpps.size; cp += strlen(cp) + 1) {
		printf((cp + strlen(cp) + 1) < (cpps.buffer + cpps.size) ?
		    "%s " : "%s", cp);
	}
	printf("\n");
	return;
}

static int
flags_to_val(char *flags)
{
	char *cp, *tok;
	int i, tmp, val = ~CCDF_USERMASK;
	size_t flagslen;

	/*
	 * The most common case is that of NIL flags, so check for
	 * those first.
	 */
	if (strcmp("none", flags) == 0 || strcmp("0x0", flags) == 0 ||
	    strcmp("0", flags) == 0)
		return (0);

	flagslen = strlen(flags);

	/* Check for values represented by strings. */
	if ((cp = strdup(flags)) == NULL)
		err(1, "no memory to parse flags");
	tmp = 0;
	for (tok = cp; (tok = strtok(tok, ",")) != NULL; tok = NULL) {
		for (i = 0; flagvaltab[i].fv_flag != NULL; ++i)
			if (strcmp(tok, flagvaltab[i].fv_flag) == 0)
				break;
		if (flagvaltab[i].fv_flag == NULL) {
			free(cp);
			goto bad_string;
		}
		tmp |= flagvaltab[i].fv_val;
	}

	/* If we get here, the string was ok. */
	free(cp);
	val = tmp;
	goto out;

 bad_string:

	/* Check for values represented in hex. */
	if (flagslen > 2 && flags[0] == '0' && flags[1] == 'x') {
		errno = 0;	/* to check for ERANGE */
		val = (int)strtol(&flags[2], &cp, 16);
		if ((errno == ERANGE) || (*cp != '\0'))
			return (-1);
		goto out;
	}

	/* Check for values represented in decimal. */
	errno = 0;	/* to check for ERANGE */
	val = (int)strtol(flags, &cp, 10);
	if ((errno == ERANGE) || (*cp != '\0'))
		return (-1);

 out:
	return (((val & ~CCDF_USERMASK) == 0) ? val : -1);
}

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n",
		"usage: ccdconfig [-cv] ccd ileave [flags] dev [...]",
		"       ccdconfig -C [-v] [-f config_file]",
		"       ccdconfig -u [-v] ccd [...]",
		"       ccdconfig -U [-v] [-f config_file]",
		"       ccdconfig -g [ccd [...]]");
	exit(1);
}

/* Local Variables: */
/* c-argdecl-indent: 8 */
/* c-indent-level: 8 */
/* End: */
