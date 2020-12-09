/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <net/if.h>

#include <err.h>
#include <libifconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ifconfig.h"

typedef enum {
	MT_PREFIX,
	MT_FILTER,
} clone_match_type;

static void
list_cloners(void)
{
	ifconfig_handle_t *lifh;
	char *cloners;
	size_t cloners_count;

	lifh = ifconfig_open();
	if (lifh == NULL)
		return;

	if (ifconfig_list_cloners(lifh, &cloners, &cloners_count) < 0)
		errc(1, ifconfig_err_errno(lifh), "unable to list cloners");
	ifconfig_close(lifh);

	for (const char *name = cloners;
	    name < cloners + cloners_count * IFNAMSIZ;
	    name += IFNAMSIZ) {
		if (name > cloners)
			putchar(' ');
		printf("%s", name);
	}
	putchar('\n');
	free(cloners);
}

struct clone_defcb {
	union {
		char ifprefix[IFNAMSIZ];
		clone_match_func *ifmatch;
	};
	clone_match_type clone_mt;
	clone_callback_func *clone_cb;
	SLIST_ENTRY(clone_defcb) next;
};

static SLIST_HEAD(, clone_defcb) clone_defcbh =
   SLIST_HEAD_INITIALIZER(clone_defcbh);

void
clone_setdefcallback_prefix(const char *ifprefix, clone_callback_func *p)
{
	struct clone_defcb *dcp;

	dcp = malloc(sizeof(*dcp));
	strlcpy(dcp->ifprefix, ifprefix, IFNAMSIZ-1);
	dcp->clone_mt = MT_PREFIX;
	dcp->clone_cb = p;
	SLIST_INSERT_HEAD(&clone_defcbh, dcp, next);
}

void
clone_setdefcallback_filter(clone_match_func *filter, clone_callback_func *p)
{
	struct clone_defcb *dcp;

	dcp = malloc(sizeof(*dcp));
	dcp->ifmatch  = filter;
	dcp->clone_mt = MT_FILTER;
	dcp->clone_cb = p;
	SLIST_INSERT_HEAD(&clone_defcbh, dcp, next);
}

/*
 * Do the actual clone operation.  Any parameters must have been
 * setup by now.  If a callback has been setup to do the work
 * then defer to it; otherwise do a simple create operation with
 * no parameters.
 */
static void
ifclonecreate(int s, void *arg)
{
	struct ifreq ifr;
	struct clone_defcb *dcp;

	memset(&ifr, 0, sizeof(ifr));
	(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

	/* Try to find a default callback by filter */
	SLIST_FOREACH(dcp, &clone_defcbh, next) {
		if (dcp->clone_mt == MT_FILTER &&
		    dcp->ifmatch(ifr.ifr_name) != 0)
			break;
	}

	if (dcp == NULL) {
		/* Try to find a default callback by prefix */
		SLIST_FOREACH(dcp, &clone_defcbh, next) {
			if (dcp->clone_mt == MT_PREFIX &&
			    strncmp(dcp->ifprefix, ifr.ifr_name,
			    strlen(dcp->ifprefix)) == 0)
				break;
		}
	}

	if (dcp == NULL || dcp->clone_cb == NULL) {
		/* NB: no parameters */
	  	ioctl_ifcreate(s, &ifr);
	} else {
		dcp->clone_cb(s, &ifr);
	}

	/*
	 * If we get a different name back than we put in, update record and
	 * indicate it should be printed later.
	 */
	if (strncmp(name, ifr.ifr_name, sizeof(name)) != 0) {
		strlcpy(name, ifr.ifr_name, sizeof(name));
		printifname = 1;
	}
}

static
DECL_CMD_FUNC(clone_create, arg, d)
{
	callback_register(ifclonecreate, NULL);
}

static
DECL_CMD_FUNC(clone_destroy, arg, d)
{
	(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCIFDESTROY, &ifr) < 0)
		err(1, "SIOCIFDESTROY");
}

static struct cmd clone_cmds[] = {
	DEF_CLONE_CMD("create",	0,	clone_create),
	DEF_CMD("destroy",	0,	clone_destroy),
	DEF_CLONE_CMD("plumb",	0,	clone_create),
	DEF_CMD("unplumb",	0,	clone_destroy),
};

static void
clone_Copt_cb(const char *optarg __unused)
{
	list_cloners();
	exit(exit_code);
}
static struct option clone_Copt = { .opt = "C", .opt_usage = "[-C]", .cb = clone_Copt_cb };

static __constructor void
clone_ctor(void)
{
	size_t i;

	for (i = 0; i < nitems(clone_cmds);  i++)
		cmd_register(&clone_cmds[i]);
	opt_register(&clone_Copt);
}
