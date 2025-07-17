/*
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 * Phil Shafer, July 2015
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "xo.h"
#include "xo_encoder.h"

int
main (int argc, char **argv)
{
    struct item {
	const char *i_title;
	int i_count;
    };
    struct item list[] = {
	{ "gum", 1412 },
	{ "rope", 85 },
	{ "ladder", 0 },
	{ "bolt", 4123 },
	{ "water", 17 },
	{ NULL, 0 }
    };
    struct item *ip;
    int i;
    
    argc = xo_parse_args(argc, argv);
    if (argc < 0)
	return 1;

    for (argc = 1; argv[argc]; argc++) {
	if (xo_streq(argv[argc], "xml"))
	    xo_set_style(NULL, XO_STYLE_XML);
	else if (xo_streq(argv[argc], "json"))
	    xo_set_style(NULL, XO_STYLE_JSON);
	else if (xo_streq(argv[argc], "text"))
	    xo_set_style(NULL, XO_STYLE_TEXT);
	else if (xo_streq(argv[argc], "html"))
	    xo_set_style(NULL, XO_STYLE_HTML);
	else if (xo_streq(argv[argc], "pretty"))
	    xo_set_flags(NULL, XOF_PRETTY);
	else if (xo_streq(argv[argc], "xpath"))
	    xo_set_flags(NULL, XOF_XPATH);
	else if (xo_streq(argv[argc], "info"))
	    xo_set_flags(NULL, XOF_INFO);
        else if (xo_streq(argv[argc], "error")) {
            close(-1);
            xo_err(1, "error detected");
        }
    }

    xo_set_flags(NULL, XOF_KEYS);
    xo_set_program("test");

    xo_open_container_h(NULL, "top");

    xo_open_container("data");
    xo_open_container("contents");
    xo_open_list("item");

    xo_emit("{T:Item/%-10s}{T:Count/%12s}\n");

    for (ip = list; ip->i_title; ip++) {
	xo_open_instance("item");

	xo_emit("{k:name/%-10s/%s}{n:count/%12u/%u}\n",
		ip->i_title, ip->i_count);

	xo_close_instance("item");
    }

    xo_close_list("item");
    xo_close_container("contents");
    xo_close_container("data");

    xo_emit("\n\n");

    xo_open_container("data2");
    xo_open_container("contents");

    xo_emit("{T:Item/%-10s}{T:Count/%12s}\n");

    for (ip = list; ip->i_title; ip++) {
	xo_open_instance("item");

	xo_emit("{k:name/%-10s/%s}{n:count/%12u/%u}\n",
		ip->i_title, ip->i_count);
    }

    xo_close_container("data2");

    xo_emit("\n\n");

    xo_open_container("data3");
    xo_open_marker("m1");
    xo_open_container("contents");

    xo_emit("{T:Item/%-10s}{T:Count/%12s}\n");

    for (ip = list; ip->i_title; ip++) {
	xo_open_instance("item");

	xo_emit("{k:name/%-10s/%s}{n:count/%12u/%u}\n",
		ip->i_title, ip->i_count);
    }

    xo_close_container("data3");	/* warn: fails at marker 'm1' */
    xo_emit("{:test}", "one");

    xo_close_marker("m1");
    xo_close_container("data3");	/* this one works, post-marker */

    xo_emit("\n\n");

    xo_open_container("data4");
    xo_open_marker("m1");
    xo_open_container("contents");

    xo_emit("{T:Item/%-10s}{T:Count/%12s}\n");

    for (ip = list; ip->i_title; ip++) {
	xo_open_instance("item");

	xo_emit("{k:name/%-10s/%s}{n:count/%12u/%u}\n",
		ip->i_title, ip->i_count);
	
	xo_open_marker("m2");
	for (i = 0; i < 3; i++) {
	    xo_open_instance("sub");
	    xo_emit("{Lwc:/Name}{:name/%d} + 1 = {:next/%d}\n", i, i + 1);
	    xo_close_container("data4"); /* warn: fails at marker 'm2' */
	}
	xo_close_marker("m2");
	xo_emit("{Lwc:/Last}{:last/%d}\n", i);
    }

    xo_close_container("data4");	/* warn: fails at marker 'm1' */
    xo_emit("{:test}", "one");

    xo_emit("\n\n");

    xo_close_container_h(NULL, "top");

    xo_finish();

    return 0;
}
