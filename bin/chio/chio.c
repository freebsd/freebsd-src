/*	$NetBSD: chio.c,v 1.6 1998/01/04 23:53:58 thorpej Exp $ */
/*
 * Copyright (c) 1996 Jason R. Thorpe <thorpej@and.com>
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
 *    must display the following acknowledgements:
 *	This product includes software developed by Jason R. Thorpe
 *	for And Communications, http://www.and.com/
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
/*
 * Additional Copyright (c) 1997, by Matthew Jacob, for NASA/Ames Research Ctr.
 */

#ifndef lint
static const char copyright[] =
	"@(#) Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.";
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/chio.h> 
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"
#include "pathnames.h"

extern	char *__progname;	/* from crt0.o */

static	void usage __P((void));
static	void cleanup __P((void));
static	int parse_element_type __P((char *));
static	int parse_element_unit __P((char *));
static	const char * element_type_name __P((int et));
static	int parse_special __P((char *));
static	int is_special __P((char *));
static	const char *bits_to_string __P((int, const char *));

static	int do_move __P((char *, int, char **));
static	int do_exchange __P((char *, int, char **));
static	int do_position __P((char *, int, char **));
static	int do_params __P((char *, int, char **));
static	int do_getpicker __P((char *, int, char **));
static	int do_setpicker __P((char *, int, char **));
static	int do_status __P((char *, int, char **));
static	int do_ielem __P((char *, int, char **));
static	int do_voltag __P((char *, int, char **));

/* Valid changer element types. */
const struct element_type elements[] = {
	{ "drive",		CHET_DT },
	{ "picker",		CHET_MT },
	{ "portal",		CHET_IE },
	{ "slot",		CHET_ST },
	{ NULL,			0 },
};

/* Valid commands. */
const struct changer_command commands[] = {
	{ "exchange",		do_exchange },
	{ "getpicker",		do_getpicker },
	{ "ielem", 		do_ielem },
	{ "move",		do_move },
	{ "params",		do_params },
	{ "position",		do_position },
	{ "setpicker",		do_setpicker },
	{ "status",		do_status },
	{ "voltag",		do_voltag },
	{ NULL,			0 },
};

/* Valid special words. */
const struct special_word specials[] = {
	{ "inv",		SW_INVERT },
	{ "inv1",		SW_INVERT1 },
	{ "inv2",		SW_INVERT2 },
	{ NULL,			0 },
};

static	int changer_fd;
static	const char *changer_name;

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch, i;

	while ((ch = getopt(argc, argv, "f:")) != -1) {
		switch (ch) {
		case 'f':
			changer_name = optarg;
			break;

		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	/* Get the default changer if not already specified. */
	if (changer_name == NULL)
		if ((changer_name = getenv(CHANGER_ENV_VAR)) == NULL)
			changer_name = _PATH_CH;

	/* Open the changer device. */
	if ((changer_fd = open(changer_name, O_RDWR, 0600)) == -1)
		err(1, "%s: open", changer_name);

	/* Register cleanup function. */
	if (atexit(cleanup))
		err(1, "can't register cleanup function");

	/* Find the specified command. */
	for (i = 0; commands[i].cc_name != NULL; ++i)
		if (strcmp(*argv, commands[i].cc_name) == 0)
			break;
	if (commands[i].cc_name == NULL) {
		/* look for abbreviation */
		for (i = 0; commands[i].cc_name != NULL; ++i)
			if (strncmp(*argv, commands[i].cc_name,
			    strlen(*argv)) == 0)
				break;
	}

	if (commands[i].cc_name == NULL)
		errx(1, "unknown command: %s", *argv);

	exit ((*commands[i].cc_handler)(commands[i].cc_name, argc, argv));
	/* NOTREACHED */
}

static int
do_move(cname, argc, argv)
	char *cname;
	int argc;
	char **argv;
{
	struct changer_move cmd;
	int val;

	/*
	 * On a move command, we expect the following:
	 *
	 * <from ET> <from EU> <to ET> <to EU> [inv]
	 *
	 * where ET == element type and EU == element unit.
	 */

	++argv; --argc;

	if (argc < 4) {
		warnx("%s: too few arguments", cname);
		goto usage;
	} else if (argc > 5) {
		warnx("%s: too many arguments", cname);
		goto usage;
	}
	(void) memset(&cmd, 0, sizeof(cmd));

	/* <from ET>  */
	cmd.cm_fromtype = parse_element_type(*argv);
	++argv; --argc;

	/* <from EU> */
	cmd.cm_fromunit = parse_element_unit(*argv);
	++argv; --argc;

	/* <to ET> */
	cmd.cm_totype = parse_element_type(*argv);
	++argv; --argc;

	/* <to EU> */
	cmd.cm_tounit = parse_element_unit(*argv);
	++argv; --argc;

	/* Deal with optional command modifier. */
	if (argc) {
		val = parse_special(*argv);
		switch (val) {
		case SW_INVERT:
			cmd.cm_flags |= CM_INVERT;
			break;

		default:
			errx(1, "%s: inappropriate modifier `%s'",
			    cname, *argv);
			/* NOTREACHED */
		}
	}

	/* Send command to changer. */
	if (ioctl(changer_fd, CHIOMOVE, &cmd))
		err(1, "%s: CHIOMOVE", changer_name);

	return (0);

 usage:
	(void) fprintf(stderr, "usage: %s %s "
	    "<from ET> <from EU> <to ET> <to EU> [inv]\n", __progname, cname);
	return (1);
}

static int
do_exchange(cname, argc, argv)
	char *cname;
	int argc;
	char **argv;
{
	struct changer_exchange cmd;
	int val;

	/*
	 * On an exchange command, we expect the following:
	 *
  * <src ET> <src EU> <dst1 ET> <dst1 EU> [<dst2 ET> <dst2 EU>] [inv1] [inv2]
	 *
	 * where ET == element type and EU == element unit.
	 */

	++argv; --argc;

	if (argc < 4) {
		warnx("%s: too few arguments", cname);
		goto usage;
	} else if (argc > 8) {
		warnx("%s: too many arguments", cname);
		goto usage;
	}
	(void) memset(&cmd, 0, sizeof(cmd));

	/* <src ET>  */
	cmd.ce_srctype = parse_element_type(*argv);
	++argv; --argc;

	/* <src EU> */
	cmd.ce_srcunit = parse_element_unit(*argv);
	++argv; --argc;

	/* <dst1 ET> */
	cmd.ce_fdsttype = parse_element_type(*argv);
	++argv; --argc;

	/* <dst1 EU> */
	cmd.ce_fdstunit = parse_element_unit(*argv);
	++argv; --argc;

	/*
	 * If the next token is a special word or there are no more
	 * arguments, then this is a case of simple exchange.
	 * dst2 == src.
	 */
	if ((argc == 0) || is_special(*argv)) {
		cmd.ce_sdsttype = cmd.ce_srctype;
		cmd.ce_sdstunit = cmd.ce_srcunit;
		goto do_special;
	}

	/* <dst2 ET> */
	cmd.ce_sdsttype = parse_element_type(*argv);
	++argv; --argc;

	/* <dst2 EU> */
	cmd.ce_sdstunit = parse_element_unit(*argv);
	++argv; --argc;

 do_special:
	/* Deal with optional command modifiers. */
	while (argc) {
		val = parse_special(*argv);
		++argv; --argc;
		switch (val) {
		case SW_INVERT1:
			cmd.ce_flags |= CE_INVERT1;
			break;

		case SW_INVERT2:
			cmd.ce_flags |= CE_INVERT2;
			break;

		default:
			errx(1, "%s: inappropriate modifier `%s'",
			    cname, *argv);
			/* NOTREACHED */
		}
	}

	/* Send command to changer. */
	if (ioctl(changer_fd, CHIOEXCHANGE, &cmd))
		err(1, "%s: CHIOEXCHANGE", changer_name);

	return (0);

 usage:
	(void) fprintf(stderr,
	    "usage: %s %s <src ET> <src EU> <dst1 ET> <dst1 EU>\n"
	    "       [<dst2 ET> <dst2 EU>] [inv1] [inv2]\n",
	    __progname, cname);
	return (1);
}

static int
do_position(cname, argc, argv)
	char *cname;
	int argc;
	char **argv;
{
	struct changer_position cmd;
	int val;

	/*
	 * On a position command, we expect the following:
	 *
	 * <to ET> <to EU> [inv]
	 *
	 * where ET == element type and EU == element unit.
	 */

	++argv; --argc;

	if (argc < 2) {
		warnx("%s: too few arguments", cname);
		goto usage;
	} else if (argc > 3) {
		warnx("%s: too many arguments", cname);
		goto usage;
	}
	(void) memset(&cmd, 0, sizeof(cmd));

	/* <to ET>  */
	cmd.cp_type = parse_element_type(*argv);
	++argv; --argc;

	/* <to EU> */
	cmd.cp_unit = parse_element_unit(*argv);
	++argv; --argc;

	/* Deal with optional command modifier. */
	if (argc) {
		val = parse_special(*argv);
		switch (val) {
		case SW_INVERT:
			cmd.cp_flags |= CP_INVERT;
			break;

		default:
			errx(1, "%s: inappropriate modifier `%s'",
			    cname, *argv);
			/* NOTREACHED */
		}
	}

	/* Send command to changer. */
	if (ioctl(changer_fd, CHIOPOSITION, &cmd))
		err(1, "%s: CHIOPOSITION", changer_name);

	return (0);

 usage:
	(void) fprintf(stderr, "usage: %s %s <to ET> <to EU> [inv]\n",
	    __progname, cname);
	return (1);
}

/* ARGSUSED */
static int
do_params(cname, argc, argv)
	char *cname;
	int argc;
	char **argv;
{
	struct changer_params data;
	int picker;

	/* No arguments to this command. */

	++argv; --argc;

	if (argc) {
		warnx("%s: no arguments expected", cname);
		goto usage;
	}

	/* Get params from changer and display them. */
	(void) memset(&data, 0, sizeof(data));
	if (ioctl(changer_fd, CHIOGPARAMS, &data))
		err(1, "%s: CHIOGPARAMS", changer_name);

	(void) printf("%s: %d slot%s, %d drive%s, %d picker%s",
	    changer_name,
	    data.cp_nslots, (data.cp_nslots > 1) ? "s" : "",
	    data.cp_ndrives, (data.cp_ndrives > 1) ? "s" : "",
	    data.cp_npickers, (data.cp_npickers > 1) ? "s" : "");
	if (data.cp_nportals)
		(void) printf(", %d portal%s", data.cp_nportals,
		    (data.cp_nportals > 1) ? "s" : "");

	/* Get current picker from changer and display it. */
	if (ioctl(changer_fd, CHIOGPICKER, &picker))
		err(1, "%s: CHIOGPICKER", changer_name);

	(void) printf("\n%s: current picker: %d\n", changer_name, picker);

	return (0);

 usage:
	(void) fprintf(stderr, "usage: %s %s\n", __progname, cname);
	return (1);
}

/* ARGSUSED */
static int
do_getpicker(cname, argc, argv)
	char *cname;
	int argc;
	char **argv;
{
	int picker;

	/* No arguments to this command. */

	++argv; --argc;

	if (argc) {
		warnx("%s: no arguments expected", cname);
		goto usage;
	}

	/* Get current picker from changer and display it. */
	if (ioctl(changer_fd, CHIOGPICKER, &picker))
		err(1, "%s: CHIOGPICKER", changer_name);

	(void) printf("%s: current picker: %d\n", changer_name, picker);

	return (0);

 usage:
	(void) fprintf(stderr, "usage: %s %s\n", __progname, cname);
	return (1);
}

static int
do_setpicker(cname, argc, argv)
	char *cname;
	int argc;
	char **argv;
{
	int picker;

	++argv; --argc;

	if (argc < 1) {
		warnx("%s: too few arguments", cname);
		goto usage;
	} else if (argc > 1) {
		warnx("%s: too many arguments", cname);
		goto usage;
	}

	picker = parse_element_unit(*argv);

	/* Set the changer picker. */
	if (ioctl(changer_fd, CHIOSPICKER, &picker))
		err(1, "%s: CHIOSPICKER", changer_name);

	return (0);

 usage:
	(void) fprintf(stderr, "usage: %s %s <picker>\n", __progname, cname);
	return (1);
}

static int
do_status(cname, argc, argv)
	char *cname;
	int argc;
	char **argv;
{
	struct changer_params cp;
	struct changer_element_status_request cesr;
	int i, count, base, chet, schet, echet;
	char *description;
	int pvoltag = 0;
	int avoltag = 0;
	int sense = 0;
	int scsi = 0;
	int source = 0;
	int intaddr = 0;
	int c;

	count = 0;
	base = 0;
	description = NULL;

	optind = optreset = 1;
	while ((c = getopt(argc, argv, "vVsSbaI")) != -1) {
		switch (c) {
		case 'v':
			pvoltag = 1;
			break;
		case 'V':
			avoltag = 1;
			break;
		case 's':
			sense = 1;
			break;
		case 'S':
			source = 1;
			break;
		case 'b':
			scsi = 1;
			break;
		case 'I':
			intaddr = 1;
			break;
		case 'a':
			pvoltag = avoltag = source = sense = scsi = intaddr = 1;
			break;
		default:
			warnx("%s: bad option", cname);
			goto usage;
		}
	}

	argc -= optind;
	argv += optind;

	/*
	 * On a status command, we expect the following:
	 *
	 * [<ET> [<start> [<end>] ] ]
	 *
	 * where ET == element type, start == first element to report,
	 * end == number of elements to report
	 *
	 * If we get no arguments, we get the status of all
	 * known element types.
	 */
	if (argc > 3) {
		warnx("%s: too many arguments", cname);
		goto usage;
	}

	/*
	 * Get params from changer.  Specifically, we need the element
	 * counts.
	 */
	if (ioctl(changer_fd, CHIOGPARAMS, (char *)&cp))
		err(1, "%s: CHIOGPARAMS", changer_name);

	if (argc > 0)
		schet = echet = parse_element_type(argv[0]);
	else {
		schet = CHET_MT;
		echet = CHET_DT;
	}
	if (argc > 1) {
		base = atol(argv[1]);
		count = 1;
	}
	if (argc > 2)
		count = atol(argv[2]) - base + 1;

	if (base < 0 || count < 0)
		errx(1, "bad arguments");

	for (chet = schet; chet <= echet; ++chet) {
		switch (chet) {
		case CHET_MT:
			if (count == 0) 
				count = cp.cp_npickers;
			else if (count > cp.cp_npickers)
				errx(1, "not that many pickers in device");
			description = "picker";
			break;

		case CHET_ST:
			if (count == 0) 
				count = cp.cp_nslots;
			else if (count > cp.cp_nslots)
				errx(1, "not that many slots in device");
			description = "slot";
			break;

		case CHET_IE:
			if (count == 0) 
				count = cp.cp_nportals;
			else if (count > cp.cp_nportals)
				errx(1, "not that many portals in device");
			description = "portal";
			break;

		case CHET_DT:
			if (count == 0) 
				count = cp.cp_ndrives;
			else if (count > cp.cp_ndrives)
				errx(1, "not that many drives in device");
			description = "drive";
			break;
 
 		default:
 			/* To appease gcc -Wuninitialized. */
 			count = 0;
 			description = NULL;
		}

		if (count == 0) {
			if (argc == 0)
				continue;
			else {
				printf("%s: no %s elements\n",
				    changer_name, description);
				return (0);
			}
		}

		bzero(&cesr, sizeof(cesr));
		cesr.cesr_element_type = chet;
		cesr.cesr_element_base = base;
		cesr.cesr_element_count = count;
		/* Allocate storage for the status structures. */
		cesr.cesr_element_status
		  = (struct changer_element_status *) 
		  malloc(count * sizeof(struct changer_element_status));
		
		if (!cesr.cesr_element_status)
			errx(1, "can't allocate status storage");

		if (avoltag || pvoltag)
			cesr.cesr_flags |= CESR_VOLTAGS;

		if (ioctl(changer_fd, CHIOGSTATUS, (char *)&cesr)) {
			free(cesr.cesr_element_status);
			err(1, "%s: CHIOGSTATUS", changer_name);
		}

		/* Dump the status for each reported element. */
		for (i = 0; i < count; ++i) {
			struct changer_element_status *ces =
			         &(cesr.cesr_element_status[i]);
			printf("%s %d: %s", description, ces->ces_addr,
			    bits_to_string(ces->ces_flags,
					   CESTATUS_BITS));
			if (sense)
				printf(" sense: <0x%02x/0x%02x>",
				       ces->ces_sensecode, 
				       ces->ces_sensequal);
			if (pvoltag)
				printf(" voltag: <%s:%d>", 
				       ces->ces_pvoltag.cv_volid,
				       ces->ces_pvoltag.cv_serial);
			if (avoltag)
				printf(" avoltag: <%s:%d>", 
				       ces->ces_avoltag.cv_volid,
				       ces->ces_avoltag.cv_serial);
			if (source) {
				if (ces->ces_flags & CES_SOURCE_VALID)
					printf(" source: <%s %d>", 
					       element_type_name(
						       ces->ces_source_type),
					       ces->ces_source_addr);
				else
					printf(" source: <>");
			}
			if (intaddr)
				printf(" intaddr: <%d>", ces->ces_int_addr);
			if (scsi) {
				printf(" scsi: <");
				if (ces->ces_flags & CES_SCSIID_VALID)
					printf("%d", ces->ces_scsi_id);
				else
					putchar('?');
				putchar(':');
				if (ces->ces_flags & CES_LUN_VALID)
					printf("%d", ces->ces_scsi_lun);
				else
					putchar('?');
				putchar('>');
			}
			putchar('\n');
		}

		free(cesr.cesr_element_status);
		count = 0;
	}

	return (0);

 usage:
	(void) fprintf(stderr, "usage: %s %s [-vVsSbaA] [<element type> [<start-addr> [<end-addr>] ] ]\n",
		       __progname, cname);
	return (1);
}

static int
do_ielem(cname, argc, argv)
	char *cname;
	int argc;
	char **argv;
{
	int timeout = 0;

	if (argc == 2) {
		timeout = atol(argv[1]);
	} else if (argc > 1) {
		warnx("%s: too many arguments", cname);
		goto usage;
	}

	if (ioctl(changer_fd, CHIOIELEM, &timeout))
		err(1, "%s: CHIOIELEM", changer_name);

	return (0);

 usage:
	(void) fprintf(stderr, "usage: %s %s [<timeout>]\n",
		       __progname, cname);
	return (1);
}

static int
do_voltag(cname, argc, argv)
	char *cname;
	int argc;
	char **argv;
{
	int force = 0;
	int clear = 0;
	int alternate = 0;
	int c;
	struct changer_set_voltag_request csvr;

	bzero(&csvr, sizeof(csvr));

	optind = optreset = 1;
	while ((c = getopt(argc, argv, "fca")) != -1) {
		switch (c) {
		case 'f':
			force = 1;
			break;
		case 'c':
			clear = 1;
			break;
		case 'a':
			alternate = 1;
			break;
		default:
			warnx("%s: bad option", cname);
			goto usage;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 2) {
		warnx("%s: missing element specification", cname);
		goto usage;
	}

	csvr.csvr_type = parse_element_type(argv[0]);
	csvr.csvr_addr = atol(argv[1]);

	if (!clear) {
		if (argc < 3 || argc > 4) {
			warnx("%s: missing argument", cname);
			goto usage;
		}

		if (force)
			csvr.csvr_flags = CSVR_MODE_REPLACE;
		else
			csvr.csvr_flags = CSVR_MODE_SET;

		if (strlen(argv[2]) > sizeof(csvr.csvr_voltag.cv_volid)) {
			warnx("%s: volume label too long", cname);
			goto usage;
		}

		strncpy(csvr.csvr_voltag.cv_volid, argv[2],
		       sizeof(csvr.csvr_voltag.cv_volid));

		if (argc == 4) {
			csvr.csvr_voltag.cv_serial = atol(argv[3]);
		}
	} else {
		if (argc != 2) {
			warnx("%s: unexpected argument", cname);
			goto usage;
		}
		csvr.csvr_flags = CSVR_MODE_CLEAR;
	}

	if (alternate) {
		csvr.csvr_flags |= CSVR_ALTERNATE;
	}

	if (ioctl(changer_fd, CHIOSETVOLTAG, &csvr))
		err(1, "%s: CHIOSETVOLTAG", changer_name);

	return 0;
 usage:
	(void) fprintf(stderr, 
		       "usage: %s %s [-fca] <element> [<voltag> [<vsn>] ]\n",
		       __progname, cname);
	return 1;
}

static int
parse_element_type(cp)
	char *cp;
{
	int i;

	for (i = 0; elements[i].et_name != NULL; ++i)
		if (strcmp(elements[i].et_name, cp) == 0)
			return (elements[i].et_type);

	errx(1, "invalid element type `%s'", cp);
	/* NOTREACHED */
}

static const char *
element_type_name(et)
	int et;
{
	int i;

	for (i = 0; elements[i].et_name != NULL; i++)
		if (elements[i].et_type == et)
			return elements[i].et_name;

	return "unknown";
}

static int
parse_element_unit(cp)
	char *cp;
{
	int i;
	char *p;

	i = (int)strtol(cp, &p, 10);
	if ((i < 0) || (*p != '\0'))
		errx(1, "invalid unit number `%s'", cp);

	return (i);
}

static int
parse_special(cp)
	char *cp;
{
	int val;

	val = is_special(cp);
	if (val)
		return (val);

	errx(1, "invalid modifier `%s'", cp);
	/* NOTREACHED */
}

static int
is_special(cp)
	char *cp;
{
	int i;

	for (i = 0; specials[i].sw_name != NULL; ++i)
		if (strcmp(specials[i].sw_name, cp) == 0)
			return (specials[i].sw_value);

	return (0);
}

static const char *
bits_to_string(v, cp)
	int v;
	const char *cp;
{
	const char *np;
	char f, sep, *bp;
	static char buf[128];

	bp = buf;
	(void) memset(buf, 0, sizeof(buf));

	for (sep = '<'; (f = *cp++) != 0; cp = np) {
		for (np = cp; *np >= ' ';)
			np++;
		if ((v & (1 << (f - 1))) == 0)
			continue;
		(void) snprintf(bp, sizeof(buf) - (bp - &buf[0]),
			"%c%.*s", sep, (int)(long)(np - cp), cp);
		bp += strlen(bp);
		sep = ',';
	}
	if (sep != '<')
		*bp = '>';

	return (buf);
}

static void
cleanup()
{
	/* Simple enough... */
	(void)close(changer_fd);
}

static void
usage()
{
	(void) fprintf(stderr, "usage: %s [-f changer] command [-<flags>] "
		"arg1 arg2 [arg3 [...]]\n", __progname);
	exit(1);
}
