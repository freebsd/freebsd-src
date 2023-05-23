/*	$FreeBSD$ */
/*	from $OpenBSD: ifconfig.c,v 1.82 2003/10/19 05:43:35 mcbride Exp $ */

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002 Michael Shalayeff. All rights reserved.
 * Copyright (c) 2003 Ryan McBride. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <stdlib.h>
#include <unistd.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip_carp.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>

#include <libifconfig.h>

#include "ifconfig.h"

static const char *carp_states[] = { CARP_STATES };

static void setcarp_callback(int, void *);

static int carpr_vhid = -1;
static int carpr_advskew = -1;
static int carpr_advbase = -1;
static int carpr_state = -1;
static struct in_addr carp_addr;
static struct in6_addr carp_addr6;
static unsigned char const *carpr_key;

static void
carp_status(if_ctx *ctx __unused)
{
	struct ifconfig_carp carpr[CARP_MAXVHID];
	char addr_buf[NI_MAXHOST];

	if (ifconfig_carp_get_info(lifh, name, carpr, CARP_MAXVHID) == -1)
		return;

	for (size_t i = 0; i < carpr[0].carpr_count; i++) {
		printf("\tcarp: %s vhid %d advbase %d advskew %d",
		    carp_states[carpr[i].carpr_state], carpr[i].carpr_vhid,
		    carpr[i].carpr_advbase, carpr[i].carpr_advskew);
		if (printkeys && carpr[i].carpr_key[0] != '\0')
			printf(" key \"%s\"\n", carpr[i].carpr_key);
		else
			printf("\n");

		inet_ntop(AF_INET6, &carpr[i].carpr_addr6, addr_buf,
		    sizeof(addr_buf));

		printf("\t      peer %s peer6 %s\n",
		    inet_ntoa(carpr[i].carpr_addr), addr_buf);
	}
}

static void
setcarp_vhid(if_ctx *ctx, const char *val, int dummy __unused)
{
	const struct afswtch *afp = ctx->afp;

	carpr_vhid = atoi(val);

	if (carpr_vhid <= 0 || carpr_vhid > CARP_MAXVHID)
		errx(1, "vhid must be greater than 0 and less than %u",
		    CARP_MAXVHID);

	if (afp->af_setvhid == NULL)
		errx(1, "%s doesn't support carp(4)", afp->af_name);
	afp->af_setvhid(carpr_vhid);
	callback_register(setcarp_callback, NULL);
}

static void
setcarp_callback(int s, void *arg __unused)
{
	struct ifconfig_carp carpr = { };

	if (ifconfig_carp_get_vhid(lifh, name, &carpr, carpr_vhid) == -1) {
		if (ifconfig_err_errno(lifh) != ENOENT)
			return;
	}

	carpr.carpr_vhid = carpr_vhid;
	if (carpr_key != NULL)
		/* XXX Should hash the password into the key here? */
		strlcpy(carpr.carpr_key, carpr_key, CARP_KEY_LEN);
	if (carpr_advskew > -1)
		carpr.carpr_advskew = carpr_advskew;
	if (carpr_advbase > -1)
		carpr.carpr_advbase = carpr_advbase;
	if (carpr_state > -1)
		carpr.carpr_state = carpr_state;
	if (carp_addr.s_addr != INADDR_ANY)
		carpr.carpr_addr = carp_addr;
	if (! IN6_IS_ADDR_UNSPECIFIED(&carp_addr6))
		memcpy(&carpr.carpr_addr6, &carp_addr6,
		    sizeof(carp_addr6));

	if (ifconfig_carp_set_info(lifh, name, &carpr))
		err(1, "SIOCSVH");
}

static void
setcarp_passwd(if_ctx *ctx __unused, const char *val, int dummy __unused)
{

	if (carpr_vhid == -1)
		errx(1, "passwd requires vhid");

	carpr_key = val;
}

static void
setcarp_advskew(if_ctx *ctx __unused, const char *val, int dummy __unused)
{

	if (carpr_vhid == -1)
		errx(1, "advskew requires vhid");

	carpr_advskew = atoi(val);
}

static void
setcarp_advbase(if_ctx *ctx __unused, const char *val, int dummy __unused)
{

	if (carpr_vhid == -1)
		errx(1, "advbase requires vhid");

	carpr_advbase = atoi(val);
}

static void
setcarp_state(if_ctx *ctx __unused, const char *val, int dummy __unused)
{
	int i;

	if (carpr_vhid == -1)
		errx(1, "state requires vhid");

	for (i = 0; i <= CARP_MAXSTATE; i++)
		if (strcasecmp(carp_states[i], val) == 0) {
			carpr_state = i;
			return;
		}

	errx(1, "unknown state");
}

static void
setcarp_peer(if_ctx *ctx __unused, const char *val, int dummy __unused)
{
	carp_addr.s_addr = inet_addr(val);
}

static void
setcarp_mcast(if_ctx *ctx __unused, const char *val __unused, int dummy __unused)
{
	carp_addr.s_addr = htonl(INADDR_CARP_GROUP);
}

static void
setcarp_peer6(if_ctx *ctx __unused, const char *val, int dummy __unused)
{
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_flags = AI_NUMERICHOST;

	if (getaddrinfo(val, NULL, &hints, &res) != 0)
		errx(1, "Invalid IPv6 address %s", val);

	memcpy(&carp_addr6, &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr,
	    sizeof(carp_addr6));
	freeaddrinfo(res);
}

static void
setcarp_mcast6(if_ctx *ctx __unused, const char *val __unused, int dummy __unused)
{
	bzero(&carp_addr6, sizeof(carp_addr6));
	carp_addr6.s6_addr[0] = 0xff;
	carp_addr6.s6_addr[1] = 0x02;
	carp_addr6.s6_addr[15] = 0x12;
}

static struct cmd carp_cmds[] = {
	DEF_CMD_ARG("advbase",	setcarp_advbase),
	DEF_CMD_ARG("advskew",	setcarp_advskew),
	DEF_CMD_ARG("pass",	setcarp_passwd),
	DEF_CMD_ARG("vhid",	setcarp_vhid),
	DEF_CMD_ARG("state",	setcarp_state),
	DEF_CMD_ARG("peer",	setcarp_peer),
	DEF_CMD("mcast",	0,	setcarp_mcast),
	DEF_CMD_ARG("peer6",	setcarp_peer6),
	DEF_CMD("mcast6", 	0,	setcarp_mcast6),
};
static struct afswtch af_carp = {
	.af_name	= "af_carp",
	.af_af		= AF_UNSPEC,
	.af_other_status = carp_status,
};

static __constructor void
carp_ctor(void)
{
	/* Default to multicast. */
	setcarp_mcast(NULL, NULL, 0);
	setcarp_mcast6(NULL, NULL, 0);

	for (size_t i = 0; i < nitems(carp_cmds);  i++)
		cmd_register(&carp_cmds[i]);
	af_register(&af_carp);
}
