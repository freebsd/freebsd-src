/*-
 * Copyright (c) 2010-2011
 * 	Swinburne University of Technology, Melbourne, Australia.
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Sebastian Zander, made
 * possible in part by a gift from The Cisco University Research Program Fund, a
 * corporate advised fund of Silicon Valley Community Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Description:
 * Bidirectional inter-arrival times feature.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_diffuse.h>

#include <netinet/ipfw/diffuse_feature.h>
#include <netinet/ipfw/diffuse_feature_iat_common.h>
#include <netinet/ipfw/diffuse_feature_iatbd.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <libutil.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "diffuse_ui.h"
#include "ipfw2.h"

enum feature_iatbd_tokens {
	TOK_DI_WINDOW = TOK_DI_FEATURE_MOD_START,
	TOK_DI_PARTIAL_WINDOWS,
	TOK_DI_ACCURATE_TIME,
	TOK_DI_JUMP_WINDOWS,
	TOK_DI_PRECISION
};

static struct di_option feature_iatbd_params[] = {
	/* Min window of 2 because we need n packets for n-1 iats. */
	{ "window",		DI_OPTION_ARG_UINT,	2,	1000,
	    TOK_DI_WINDOW },
	{ "partial-windows",	DI_OPTION_ARG_NOARG,	0,	0,
	    TOK_DI_PARTIAL_WINDOWS },
	{ "accurate-time",	DI_OPTION_ARG_NOARG,	0,	0,
	    TOK_DI_ACCURATE_TIME },
	{ "jump-windows",	DI_OPTION_ARG_NOARG,	0,	0,
	    TOK_DI_JUMP_WINDOWS },
	/* Configurable precision from 1us to 1ms. */
	{ "precision",		DI_OPTION_ARG_UINT,	1,	10000,
	    TOK_DI_PRECISION },
	{ NULL, 0, 0 }  /* Terminator. */
};

int
iatbd_get_conf_size(void)
{

	return (sizeof(struct di_feature_iatbd_config));
}

int
iatbd_get_opts(struct di_option **opts)
{

	*opts = feature_iatbd_params;

	return (sizeof(feature_iatbd_params));
}

int
iatbd_parse_opts(int token, char *arg_val, struct di_oid *buf)
{
	static struct di_feature_iatbd_config *conf = NULL;
	char *end;

	if (conf == NULL) {
		conf = (struct di_feature_iatbd_config *)buf;
		conf->iat_window = -1;
		conf->iat_partial_window = -1;
		conf->iat_ts_acc = -1;
		conf->iat_jump_window = -1;
		conf->iat_prec = -1;
	}

	switch(token) {
	case TOK_DI_OPTS_INIT:
		break;

	case TOK_DI_WINDOW:
		end = NULL;
		conf->iat_window = strtoul(arg_val, &end, 0);
		if (*end == 'K' || *end == 'k')
			conf->iat_window *= 1024;
		break;

	case TOK_DI_PARTIAL_WINDOWS:
		conf->iat_partial_window = 1;
		break;

	case TOK_DI_ACCURATE_TIME:
		conf->iat_ts_acc = 1;
		break;

	case TOK_DI_JUMP_WINDOWS:
		conf->iat_jump_window = 1;
		break;

	case TOK_DI_PRECISION:
		conf->iat_prec = strtoul(arg_val, &end, 0);
		break;

	default:
		/* This should never happen. */
		errx(EX_DATAERR, "invalid option, fix source");
	}

	return (0);
}

void
iatbd_print_opts(struct di_oid *opts)
{
	struct di_feature_iatbd_config *conf;

	conf = (struct di_feature_iatbd_config *)opts;

	printf("  window: %d\n", conf->iat_window);
	printf("  partial windows: %s\n",
	    (conf->iat_partial_window == 1) ? "yes" : "no");
	printf("  accurate time: %s\n", (conf->iat_ts_acc == 1) ? "yes" : "no");
	printf("  jump window: %s\n",
	    (conf->iat_jump_window == 1) ? "yes" : "no");
	printf("  precision: %d\n", conf->iat_prec);
}

void
iatbd_print_usage()
{

	printf("module iatbd [window <packets>] [partial-windows] "
	    "[jump-windows] [accurate-time] [precision <us>]\n");
}

DI_IATBD_STAT_NAMES; /* Stat name array in diffuse_feature_iatbd.h. */
char *
iatbd_get_stat_name(int i)
{

	return (di_iatbd_stat_names[i]);
}

static struct di_feature_module iatbd_feature_module = {
	.name =			DI_IATBD_NAME,
	.type =			DI_IATBD_TYPE,
	.get_conf_size =	iatbd_get_conf_size,
	.get_opts =		iatbd_get_opts,
	.parse_opts =		iatbd_parse_opts,
	.print_opts =		iatbd_print_opts,
	.print_usage =		iatbd_print_usage,
	.get_stat_name =	iatbd_get_stat_name
};

struct di_feature_module *iatbd_module(void)
{

	return (&iatbd_feature_module);
}
