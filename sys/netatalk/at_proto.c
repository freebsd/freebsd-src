/*-
 * Copyright (c) 1990, 1991 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Mike Clark
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-763-0525
 *	netatalk@itd.umich.edu
 *
 * $FreeBSD: src/sys/netatalk/at_proto.c,v 1.13.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/socket.h>

#include <sys/kernel.h>

#include <net/route.h>

#include <netatalk/at.h>
#include <netatalk/ddp_var.h>
#include <netatalk/at_extern.h>

static struct domain	atalkdomain;

static struct protosw	atalksw[] = {
	{
		/* Identifiers */
		.pr_type =		SOCK_DGRAM,
		.pr_domain =		&atalkdomain,
		.pr_protocol =		ATPROTO_DDP,
		.pr_flags =		PR_ATOMIC|PR_ADDR,
		.pr_output =		ddp_output,
		.pr_init =		ddp_init,
		.pr_usrreqs =		&ddp_usrreqs,
	},
};

static struct domain	atalkdomain = {
	.dom_family =		AF_APPLETALK,
	.dom_name =		"appletalk",
	.dom_protosw =		atalksw,
	.dom_protoswNPROTOSW =	&atalksw[sizeof(atalksw)/sizeof(atalksw[0])],
	.dom_rtattach =		at_inithead,
	.dom_rtoffset =		offsetof(struct sockaddr_at, sat_addr) << 3,
	.dom_maxrtkey =		sizeof(struct sockaddr_at),
};

DOMAIN_SET(atalk);
