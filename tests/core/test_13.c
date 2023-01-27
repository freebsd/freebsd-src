/*
 * Copyright (c) 2022, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 * Phil Shafer, Feb 2022
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <langinfo.h>
#include <unistd.h>
#include <wchar.h>
#include <fcntl.h>

#include "xo_config.h"
#include "xo.h"
#include "xo_encoder.h"

static size_t padding_for_month[12];
static size_t month_max_size = 0;

static const char *
get_abmon(int mon)
{

	switch (mon) {
	case 0: return (nl_langinfo(ABMON_1));
	case 1: return (nl_langinfo(ABMON_2));
	case 2: return (nl_langinfo(ABMON_3));
	case 3: return (nl_langinfo(ABMON_4));
	case 4: return (nl_langinfo(ABMON_5));
	case 5: return (nl_langinfo(ABMON_6));
	case 6: return (nl_langinfo(ABMON_7));
	case 7: return (nl_langinfo(ABMON_8));
	case 8: return (nl_langinfo(ABMON_9));
	case 9: return (nl_langinfo(ABMON_10));
	case 10: return (nl_langinfo(ABMON_11));
	case 11: return (nl_langinfo(ABMON_12));
	}

	/* should never happen */
	abort();
}

static void
compute_abbreviated_month_size(void)
{
	int i;
	size_t width;
	size_t months_width[12];

	for (i = 0; i < 12; i++) {
		width = strlen(get_abmon(i));
		if (width == (size_t)-1) {
			month_max_size = -1;
			return;
		}
		months_width[i] = width;
		if (width > month_max_size)
			month_max_size = width;
	}

	for (i = 0; i < 12; i++)
		padding_for_month[i] = month_max_size - months_width[i];
}

static void
printsize(const char *field, size_t width, off_t bytes)
{
	char fmt[BUFSIZ];
	
		/* This format assignment needed to work round gcc bug. */
		snprintf(fmt, sizeof(fmt), "{:%s/%%%dj%sd} ",
		     field, (int) width, "");
		xo_emit_f(XOEF_NO_RETAIN, fmt, (intmax_t) bytes);
}

static size_t
ls_strftime(char *str, size_t len, const char *fmt, const struct tm *tm)
{
	char *posb, nfmt[BUFSIZ];
	const char *format = fmt;
	size_t ret;

	if ((posb = strstr(fmt, "%b")) != NULL) {
		if (month_max_size > 0) {
			snprintf(nfmt, sizeof(nfmt),  "%.*s%s%*s%s",
			    (int)(posb - fmt), fmt,
			    get_abmon(tm->tm_mon),
			    (int)padding_for_month[tm->tm_mon],
			    "",
			    posb + 2);
			format = nfmt;
		}
	}
	ret = strftime(str, len, format, tm);
	return (ret);
}

static void
printtime(const char *field, time_t ftime)
{
	char longstring[80];
	char fmt[BUFSIZ];
	static time_t now = 0;
	const char *format;
	static int d_first = -1;

	if (d_first < 0)
	    d_first = 1;
	if (now == 0)
	    now = time(NULL);

#define	SIXMONTHS	((365 / 2) * 86400)
	if (1)
		/* mmm dd hh:mm || dd mmm hh:mm */
		format = d_first ? "%e %b %R" : "%b %e %R";
	else
		/* mmm dd  yyyy || dd mmm  yyyy */
		format = d_first ? "%e %b  %Y" : "%b %e  %Y";
	ls_strftime(longstring, sizeof(longstring), format, localtime(&ftime));

	snprintf(fmt, sizeof(fmt), "{d:%s/%%hs} ", field);
	xo_attr("value", "%ld", (long) ftime);
	xo_emit_f(XOEF_NO_RETAIN, fmt, longstring);
	snprintf(fmt, sizeof(fmt), "{en:%s/%%ld}", field);
	xo_emit_f(XOEF_NO_RETAIN, fmt, (long) ftime);
}


int
main (int argc, char **argv)
{
    int i, count = 10;
    int mon = 0;
    xo_emit_flags_t flags = XOF_RETAIN_ALL;
    int opt_color = 1;

    xo_set_program("test_13");

    argc = xo_parse_args(argc, argv);
    if (argc < 0)
	return 1;

    compute_abbreviated_month_size();

    for (argc = 1; argv[argc]; argc++) {
	if (xo_streq(argv[argc], "xml"))
	    xo_set_style(NULL, XO_STYLE_XML);
	else if (xo_streq(argv[argc], "json"))
	    xo_set_style(NULL, XO_STYLE_JSON);
	else if (xo_streq(argv[argc], "text"))
	    xo_set_style(NULL, XO_STYLE_TEXT);
	else if (xo_streq(argv[argc], "html"))
	    xo_set_style(NULL, XO_STYLE_HTML);
	else if (xo_streq(argv[argc], "no-color"))
	    opt_color = 0;
	else if (xo_streq(argv[argc], "pretty"))
	    xo_set_flags(NULL, XOF_PRETTY);
	else if (xo_streq(argv[argc], "xpath"))
	    xo_set_flags(NULL, XOF_XPATH);
	else if (xo_streq(argv[argc], "info"))
	    xo_set_flags(NULL, XOF_INFO);
	else if (xo_streq(argv[argc], "no-retain"))
	    flags &= ~XOF_RETAIN_ALL;
	else if (xo_streq(argv[argc], "big")) {
	    if (argv[argc + 1]) {
		const char *cp = argv[++argc];
		char *ep;
		count = strtoul(cp, &ep, 0);
		if (ep && *ep) {
		    const char suff[] = "kmgt";
		    unsigned long mult[]
			= { 1000, 1000000, 1000000000, 1000000000000 };
		    char *sp = strchr(suff, *ep);
		    if (sp) {
			count *= mult[sp - suff];
		    }
		}
	    }
	} else if (xo_streq(argv[argc], "null")) {
	    int fd = open("/dev/null", O_WRONLY);
	    if (fd >= 0) {
		close(1);
		dup2(fd, 1);
	    }
	}
    }

    xo_set_flags(NULL, XOF_UNITS); /* Always test w/ this */
    if (opt_color)
	xo_set_flags(NULL, XOF_COLOR); /* Force color output */
    xo_set_file(stdout);

    xo_open_container("top");
    xo_open_container("data");

    if (flags != 0)
	xo_set_flags(NULL, flags);

    xo_open_list("entry");

    for (i = 0; i < count; i++) {
	xo_open_instance("entry");

	char name[80];
	snprintf(name, sizeof(name), "xx-%08u", i);

	xo_emitr("{ke:name/%hs}", name);

	xo_emitr("{t:inode/%*ju} ", 3, 12);
	xo_emitr("{t:blocks/%*jd} ", 4, 1234);

	xo_emitr("{t:mode/%s}{e:mode_octal/%03o} {t:links/%*ju} {t:user/%-*s}  {t:group/%-*s}  ",
		"mode", 0660, 2, (uintmax_t) 12,
		4, "phil", 4, "phil");


	xo_emitr("{e:type/%s}", "regular");

	xo_emitr("{:flags/%-*s} ", 3, "123");
	xo_emitr("{t:label/%-*s} ", 4, "1234");
	printsize("size", 5, 12345);
	printtime("modify-time", 1644355825);

	xo_emitr("{dk:name/%hs}", name);

	xo_close_instance("entry");
	xo_emit("\n");
    }

    xo_close_list("entry");

    xo_emit("{Lwc:hits}{:hits/%ld}\n", xo_retain_get_hits());

    xo_close_container("data");
    xo_close_container_h(NULL, "top");

    xo_finish();

    return 0;
}
