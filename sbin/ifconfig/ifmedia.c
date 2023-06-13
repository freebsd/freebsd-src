/*	$NetBSD: ifconfig.c,v 1.34 1997/04/21 01:17:58 lukem Exp $	*/
/* $FreeBSD$ */

/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 1997 Jason R. Thorpe.
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
 *      This product includes software developed for the NetBSD Project
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

/*
 * Copyright (c) 1983, 1993
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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/route.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libifconfig.h>

#include "ifconfig.h"

static void domediaopt(if_ctx *, const char *, bool);
static ifmedia_t get_media_subtype(ifmedia_t, const char *);
static ifmedia_t get_media_mode(ifmedia_t, const char *);
static ifmedia_t get_media_options(ifmedia_t, const char *);
static void print_media(ifmedia_t, bool);
static void print_media_ifconfig(ifmedia_t);

static void
media_status(if_ctx *ctx)
{
	struct ifmediareq *ifmr;

	if (ifconfig_media_get_mediareq(lifh, ctx->ifname, &ifmr) == -1)
		return;

	if (ifmr->ifm_count == 0) {
		warnx("%s: no media types?", ctx->ifname);
		goto free;
	}

	printf("\tmedia: ");
	print_media(ifmr->ifm_current, true);
	if (ifmr->ifm_active != ifmr->ifm_current) {
		putchar(' ');
		putchar('(');
		print_media(ifmr->ifm_active, false);
		putchar(')');
	}

	putchar('\n');

	if (ifmr->ifm_status & IFM_AVALID) {
		struct ifdownreason ifdr;
		const char *status;

		status = ifconfig_media_get_status(ifmr);
		printf("\tstatus: %s", status);
		if (strcmp(status, "no carrier") == 0 &&
		    ifconfig_media_get_downreason(lifh, ctx->ifname, &ifdr) == 0) {
			switch (ifdr.ifdr_reason) {
			case IFDR_REASON_MSG:
				printf(" (%s)", ifdr.ifdr_msg);
				break;
			case IFDR_REASON_VENDOR:
				printf(" (vendor code %d)",
				    ifdr.ifdr_vendor);
				break;
			default:
				break;
			}
		}
		putchar('\n');
	}

	if (ctx->args->supmedia) {
		printf("\tsupported media:\n");
		for (int i = 0; i < ifmr->ifm_count; ++i) {
			printf("\t\t");
			print_media_ifconfig(ifmr->ifm_ulist[i]);
			putchar('\n');
		}
	}
free:
	free(ifmr);
}

struct ifmediareq *
ifmedia_getstate(if_ctx *ctx)
{
	static struct ifmediareq *ifmr = NULL;

	if (ifmr != NULL)
		return (ifmr);

	if (ifconfig_media_get_mediareq(lifh, ctx->ifname, &ifmr) == -1)
		errc(1, ifconfig_err_errno(lifh),
		    "%s: ifconfig_media_get_mediareq", ctx->ifname);

	if (ifmr->ifm_count == 0)
		errx(1, "%s: no media types?", ctx->ifname);

	return (ifmr);
}

static void
setifmediacallback(if_ctx *ctx, void *arg)
{
	struct ifmediareq *ifmr = (struct ifmediareq *)arg;
	static bool did_it = false;

	if (!did_it) {
		ifr.ifr_media = ifmr->ifm_current;
		if (ioctl_ctx(ctx, SIOCSIFMEDIA, (caddr_t)&ifr) < 0)
			err(1, "SIOCSIFMEDIA (media)");
		free(ifmr);
		did_it = true;
	}
}

static void
setmedia(if_ctx *ctx, const char *val, int d __unused)
{
	struct ifmediareq *ifmr;
	int subtype;

	ifmr = ifmedia_getstate(ctx);

	/*
	 * We are primarily concerned with the top-level type.
	 * However, "current" may be only IFM_NONE, so we just look
	 * for the top-level type in the first "supported type"
	 * entry.
	 *
	 * (I'm assuming that all supported media types for a given
	 * interface will be the same top-level type..)
	 */
	subtype = get_media_subtype(ifmr->ifm_ulist[0], val);

	strlcpy(ifr.ifr_name, ctx->ifname, sizeof(ifr.ifr_name));
	ifr.ifr_media = (ifmr->ifm_current & IFM_IMASK) |
	    IFM_TYPE(ifmr->ifm_ulist[0]) | subtype;

	ifmr->ifm_current = ifr.ifr_media;
	callback_register(setifmediacallback, (void *)ifmr);
}

static void
setmediaopt(if_ctx *ctx, const char *val, int d __unused)
{

	domediaopt(ctx, val, false);
}

static void
unsetmediaopt(if_ctx *ctx, const char *val, int d __unused)
{

	domediaopt(ctx, val, true);
}

static void
domediaopt(if_ctx *ctx, const char *val, bool clear)
{
	struct ifmediareq *ifmr;
	ifmedia_t options;

	ifmr = ifmedia_getstate(ctx);

	options = get_media_options(ifmr->ifm_ulist[0], val);

	strlcpy(ifr.ifr_name, ctx->ifname, sizeof(ifr.ifr_name));
	ifr.ifr_media = ifmr->ifm_current;
	if (clear)
		ifr.ifr_media &= ~options;
	else {
		if (options & IFM_HDX) {
			ifr.ifr_media &= ~IFM_FDX;
			options &= ~IFM_HDX;
		}
		ifr.ifr_media |= options;
	}
	ifmr->ifm_current = ifr.ifr_media;
	callback_register(setifmediacallback, (void *)ifmr);
}

static void
setmediainst(if_ctx *ctx, const char *val, int d __unused)
{
	struct ifmediareq *ifmr;
	int inst;

	ifmr = ifmedia_getstate(ctx);

	inst = atoi(val);
	if (inst < 0 || inst > (int)IFM_INST_MAX)
		errx(1, "invalid media instance: %s", val);

	strlcpy(ifr.ifr_name, ctx->ifname, sizeof(ifr.ifr_name));
	ifr.ifr_media = (ifmr->ifm_current & ~IFM_IMASK) | inst << IFM_ISHIFT;

	ifmr->ifm_current = ifr.ifr_media;
	callback_register(setifmediacallback, (void *)ifmr);
}

static void
setmediamode(if_ctx *ctx, const char *val, int d __unused)
{
	struct ifmediareq *ifmr;
	int mode;

	ifmr = ifmedia_getstate(ctx);

	mode = get_media_mode(ifmr->ifm_ulist[0], val);

	strlcpy(ifr.ifr_name, ctx->ifname, sizeof(ifr.ifr_name));
	ifr.ifr_media = (ifmr->ifm_current & ~IFM_MMASK) | mode;

	ifmr->ifm_current = ifr.ifr_media;
	callback_register(setifmediacallback, (void *)ifmr);
}

static ifmedia_t
get_media_subtype(ifmedia_t media, const char *val)
{
	ifmedia_t subtype;

	subtype = ifconfig_media_lookup_subtype(media, val);
	if (subtype != INVALID_IFMEDIA)
		return (subtype);
	switch (errno) {
	case EINVAL:
		errx(EXIT_FAILURE, "unknown media type 0x%x", media);
	case ENOENT:
		errx(EXIT_FAILURE, "unknown media subtype: %s", val);
	default:
		err(EXIT_FAILURE, "ifconfig_media_lookup_subtype");
	}
	/*NOTREACHED*/
}

static ifmedia_t
get_media_mode(ifmedia_t media, const char *val)
{
	ifmedia_t mode;

	mode = ifconfig_media_lookup_mode(media, val);
	if (mode != INVALID_IFMEDIA)
		return (mode);
	switch (errno) {
	case EINVAL:
		errx(EXIT_FAILURE, "unknown media type 0x%x", media);
	case ENOENT:
		return (INVALID_IFMEDIA);
	default:
		err(EXIT_FAILURE, "ifconfig_media_lookup_subtype");
	}
	/*NOTREACHED*/
}

static ifmedia_t
get_media_options(ifmedia_t media, const char *val)
{
	ifmedia_t *options;
	const char **optnames;
	char *opts, *opt;
	size_t nopts;
	int rval;

	/*
	 * We muck with the string, so copy it.
	 */
	opts = strdup(val);
	if (opts == NULL)
		err(EXIT_FAILURE, "strdup");

	/*
	 * Split the comma-delimited list into separate strings.
	 */
	nopts = 0;
	for (opt = opts; (opt = strtok(opt, ",")) != NULL; opt = NULL)
		++nopts;
	if (nopts == 0) {
		free(opts);
		return (0);
	}
	optnames = calloc(nopts, sizeof(*optnames));
	if (optnames == NULL)
		err(EXIT_FAILURE, "calloc");
	opt = opts;
	for (size_t i = 0; i < nopts; ++i) {
		optnames[i] = opt;
		opt = strchr(opt, '\0') + 1;
	}

	/*
	 * Look up the options in the user-provided list.
	 */
	options = ifconfig_media_lookup_options(media, optnames, nopts);
	if (options == NULL)
		err(EXIT_FAILURE, "ifconfig_media_lookup_options");
	rval = 0;
	for (size_t i = 0; i < nopts; ++i) {
		if (options[i] == INVALID_IFMEDIA)
			errx(EXIT_FAILURE, "unknown option: %s", optnames[i]);
		rval |= options[i];
	}
	free(options);
	free(optnames);
	free(opts);
	return (rval);
}

static void
print_media(ifmedia_t media, bool print_toptype)
{
	const char *val, **options;

	val = ifconfig_media_get_type(media);
	if (val == NULL) {
		printf("<unknown type>");
		return;
	} else if (print_toptype) {
		printf("%s", val);
	}

	val = ifconfig_media_get_subtype(media);
	if (val == NULL) {
		printf("<unknown subtype>");
		return;
	}

	if (print_toptype)
		putchar(' ');

	printf("%s", val);

	if (print_toptype) {
		val = ifconfig_media_get_mode(media);
		if (val != NULL && strcasecmp("autoselect", val) != 0)
			printf(" mode %s", val);
	}

	options = ifconfig_media_get_options(media);
	if (options != NULL && options[0] != NULL) {
		printf(" <%s", options[0]);
		for (size_t i = 1; options[i] != NULL; ++i)
			printf(",%s", options[i]);
		printf(">");
	}
	free(options);

	if (print_toptype && IFM_INST(media) != 0)
		printf(" instance %d", IFM_INST(media));
}

static void
print_media_ifconfig(ifmedia_t media)
{
	const char *val, **options;

	val = ifconfig_media_get_type(media);
	if (val == NULL) {
		printf("<unknown type>");
		return;
	}

	/*
	 * Don't print the top-level type; it's not like we can
	 * change it, or anything.
	 */

	val = ifconfig_media_get_subtype(media);
	if (val == NULL) {
		printf("<unknown subtype>");
		return;
	}

	printf("media %s", val);

	val = ifconfig_media_get_mode(media);
	if (val != NULL)
		printf(" mode %s", val);

	options = ifconfig_media_get_options(media);
	if (options != NULL && options[0] != NULL) {
		printf(" mediaopt %s", options[0]);
		for (size_t i = 1; options[i] != NULL; ++i)
			printf(",%s", options[i]);
	}
	free(options);

	if (IFM_INST(media) != 0)
		printf(" instance %d", IFM_INST(media));
}

/**********************************************************************
 * ...until here.
 **********************************************************************/

static struct cmd media_cmds[] = {
	DEF_CMD_ARG("media",	setmedia),
	DEF_CMD_ARG("mode",	setmediamode),
	DEF_CMD_ARG("mediaopt",	setmediaopt),
	DEF_CMD_ARG("-mediaopt",unsetmediaopt),
	DEF_CMD_ARG("inst",	setmediainst),
	DEF_CMD_ARG("instance",	setmediainst),
};
static struct afswtch af_media = {
	.af_name	= "af_media",
	.af_af		= AF_UNSPEC,
	.af_other_status = media_status,
};

static __constructor void
ifmedia_ctor(void)
{
	for (size_t i = 0; i < nitems(media_cmds);  i++)
		cmd_register(&media_cmds[i]);
	af_register(&af_media);
}
