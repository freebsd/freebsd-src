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

#ifndef lint
static const char rcsid[] =
	"$Id: chio.c,v 1.5 1998/05/06 06:49:56 charnier Exp $";
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

static	void usage __P((void));
static	void cleanup __P((void));
static	int parse_element_type __P((char *));
static	int parse_element_unit __P((char *));
static	int parse_special __P((char *));
static	int is_special __P((char *));
static	char *bits_to_string __P((int, const char *));

static	int do_move __P((char *, int, char **));
static	int do_exchange __P((char *, int, char **));
static	int do_position __P((char *, int, char **));
static	int do_params __P((char *, int, char **));
static	int do_getpicker __P((char *, int, char **));
static	int do_setpicker __P((char *, int, char **));
static	int do_status __P((char *, int, char **));

/* Valid changer element types. */
const struct element_type elements[] = {
	{ "picker",		CHET_MT },
	{ "slot",		CHET_ST },
	{ "portal",		CHET_IE },
	{ "drive",		CHET_DT },
	{ NULL,			0 },
};

/* Valid commands. */
const struct changer_command commands[] = {
	{ "move",		do_move },
	{ "exchange",		do_exchange },
	{ "position",		do_position },
	{ "params",		do_params },
	{ "getpicker",		do_getpicker },
	{ "setpicker",		do_setpicker },
	{ "status",		do_status },
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
static	char *changer_name;

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
	if (commands[i].cc_name == NULL)
		errx(1, "unknown command: %s", *argv);

	/* Skip over the command name and call handler. */
	++argv; --argc;
	exit ((*commands[i].cc_handler)(commands[i].cc_name, argc, argv));
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
	if (argc < 4) {
		warnx("%s: too few arguments", cname);
		goto usage;
	} else if (argc > 5) {
		warnx("%s: too many arguments", cname);
		goto usage;
	}
	bzero(&cmd, sizeof(cmd));

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
	if (ioctl(changer_fd, CHIOMOVE, (char *)&cmd))
		err(1, "%s: CHIOMOVE", changer_name);

	return (0);

 usage:
	fprintf(stderr, "usage: chio %s "
	    "<from ET> <from EU> <to ET> <to EU> [inv]\n", cname);
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
	if (argc < 4) {
		warnx("%s: too few arguments", cname);
		goto usage;
	} else if (argc > 8) {
		warnx("%s: too many arguments", cname);
		goto usage;
	}
	bzero(&cmd, sizeof(cmd));

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
	if (ioctl(changer_fd, CHIOEXCHANGE, (char *)&cmd))
		err(1, "%s: CHIOEXCHANGE", changer_name);

	return (0);

 usage:
	fprintf(stderr,
		"usage: chio %s <src ET> <src EU> <dst1 ET> <dst1 EU>\n"
		"       [<dst2 ET> <dst2 EU>] [inv1] [inv2]\n", cname);
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
	if (argc < 2) {
		warnx("%s: too few arguments", cname);
		goto usage;
	} else if (argc > 3) {
		warnx("%s: too many arguments", cname);
		goto usage;
	}
	bzero(&cmd, sizeof(cmd));

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
	if (ioctl(changer_fd, CHIOPOSITION, (char *)&cmd))
		err(1, "%s: CHIOPOSITION", changer_name);

	return (0);

 usage:
	fprintf(stderr, "usage: chio %s <to ET> <to EU> [inv]\n", cname);
	return (1);
}

static int
do_params(cname, argc, argv)
	char *cname;
	int argc;
	char **argv;
{
	struct changer_params data;

	/* No arguments to this command. */
	if (argc) {
		warnx("%s: no arguments expected", cname);
		goto usage;
	}

	/* Get params from changer and display them. */
	bzero(&data, sizeof(data));
	if (ioctl(changer_fd, CHIOGPARAMS, (char *)&data))
		err(1, "%s: CHIOGPARAMS", changer_name);

	printf("%s: %d slot%s, %d drive%s, %d picker%s",
	    changer_name,
	    data.cp_nslots, (data.cp_nslots > 1) ? "s" : "",
	    data.cp_ndrives, (data.cp_ndrives > 1) ? "s" : "",
	    data.cp_npickers, (data.cp_npickers > 1) ? "s" : "");
	if (data.cp_nportals)
		printf(", %d portal%s", data.cp_nportals,
		    (data.cp_nportals > 1) ? "s" : "");
	printf("\n%s: current picker: %d\n", changer_name, data.cp_curpicker);

	return (0);

 usage:
	fprintf(stderr, "usage: chio %s\n", cname);
	return (1);
}

static int
do_getpicker(cname, argc, argv)
	char *cname;
	int argc;
	char **argv;
{
	int picker;

	/* No arguments to this command. */
	if (argc) {
		warnx("%s: no arguments expected", cname);
		goto usage;
	}

	/* Get current picker from changer and display it. */
	if (ioctl(changer_fd, CHIOGPICKER, (char *)&picker))
		err(1, "%s: CHIOGPICKER", changer_name);

	printf("%s: current picker: %d\n", changer_name, picker);

	return (0);

 usage:
	fprintf(stderr, "usage: chio %s\n", cname);
	return (1);
}

static int
do_setpicker(cname, argc, argv)
	char *cname;
	int argc;
	char **argv;
{
	int picker;

	if (argc < 1) {
		warnx("%s: too few arguments", cname);
		goto usage;
	} else if (argc > 1) {
		warnx("%s: too many arguments", cname);
		goto usage;
	}

	picker = parse_element_unit(*argv);

	/* Set the changer picker. */
	if (ioctl(changer_fd, CHIOSPICKER, (char *)&picker))
		err(1, "%s: CHIOSPICKER", changer_name);

	return (0);

 usage:
	fprintf(stderr, "usage: chio %s <picker>\n", cname);
	return (1);
}

static int
do_status(cname, argc, argv)
	char *cname;
	int argc;
	char **argv;
{
	struct changer_element_status cmd;
	struct changer_params data;
	u_int8_t *statusp;
	int i, count, chet, schet, echet;
	char *description;

	count = 0;
	description = NULL;

	/*
	 * On a status command, we expect the following:
	 *
	 * [<ET>]
	 *
	 * where ET == element type.
	 *
	 * If we get no arguments, we get the status of all
	 * known element types.
	 */
	if (argc > 1) {
		warnx("%s: too many arguments", cname);
		goto usage;
	}

	/*
	 * Get params from changer.  Specifically, we need the element
	 * counts.
	 */
	bzero(&data, sizeof(data));
	if (ioctl(changer_fd, CHIOGPARAMS, (char *)&data))
		err(1, "%s: CHIOGPARAMS", changer_name);

	if (argc)
		schet = echet = parse_element_type(*argv);
	else {
		schet = CHET_MT;
		echet = CHET_DT;
	}

	for (chet = schet; chet <= echet; ++chet) {
		switch (chet) {
		case CHET_MT:
			count = data.cp_npickers;
			description = "picker";
			break;

		case CHET_ST:
			count = data.cp_nslots;
			description = "slot";
			break;

		case CHET_IE:
			count = data.cp_nportals;
			description = "portal";
			break;

		case CHET_DT:
			count = data.cp_ndrives;
			description = "drive";
			break;
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

		/* Allocate storage for the status bytes. */
		if ((statusp = (u_int8_t *)malloc(count)) == NULL)
			errx(1, "can't allocate status storage");

		bzero(statusp, count);
		bzero(&cmd, sizeof(cmd));

		cmd.ces_type = chet;
		cmd.ces_data = statusp;

		if (ioctl(changer_fd, CHIOGSTATUS, (char *)&cmd)) {
			free(statusp);
			err(1, "%s: CHIOGSTATUS", changer_name);
		}

		/* Dump the status for each element of this type. */
		for (i = 0; i < count; ++i) {
			printf("%s %d: %s\n", description, i,
			    bits_to_string(statusp[i], CESTATUS_BITS));
		}

		free(statusp);
	}

	return (0);

 usage:
	fprintf(stderr, "usage: chio %s [<element type>]\n", cname);
	return (1);
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

static char *
bits_to_string(v, cp)
	int v;
	const char *cp;
{
	const char *np;
	char f, sep, *bp;
	static char buf[128];

	bp = buf;
	bzero(buf, sizeof(buf));

	for (sep = '<'; (f = *cp++) != 0; cp = np) {
		for (np = cp; *np >= ' ';)
			np++;
		if ((v & (1 << (f - 1))) == 0)
			continue;
		bp += snprintf(bp, sizeof(buf) - (bp - &buf[0]),
						"%c%.*s", sep, np - cp, cp);
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
	int i;

	fprintf(stderr, "usage: chio [-f changer] command [args ...]\n");
	fprintf(stderr, "commands:");
	for (i = 0; commands[i].cc_name; i++)
		fprintf(stderr, " %s", commands[i].cc_name);
	fprintf(stderr, "\n");
	exit(1);
}
