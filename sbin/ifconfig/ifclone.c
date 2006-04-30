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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ifconfig.h"

static void
list_cloners(void)
{
	struct if_clonereq ifcr;
	char *cp, *buf;
	int idx;
	int s;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		err(1, "socket(AF_INET,SOCK_DGRAM)");

	memset(&ifcr, 0, sizeof(ifcr));

	if (ioctl(s, SIOCIFGCLONERS, &ifcr) < 0)
		err(1, "SIOCIFGCLONERS for count");

	buf = malloc(ifcr.ifcr_total * IFNAMSIZ);
	if (buf == NULL)
		err(1, "unable to allocate cloner name buffer");

	ifcr.ifcr_count = ifcr.ifcr_total;
	ifcr.ifcr_buffer = buf;

	if (ioctl(s, SIOCIFGCLONERS, &ifcr) < 0)
		err(1, "SIOCIFGCLONERS for names");

	/*
	 * In case some disappeared in the mean time, clamp it down.
	 */
	if (ifcr.ifcr_count > ifcr.ifcr_total)
		ifcr.ifcr_count = ifcr.ifcr_total;

	for (cp = buf, idx = 0; idx < ifcr.ifcr_count; idx++, cp += IFNAMSIZ) {
		if (idx > 0)
			putchar(' ');
		printf("%s", cp);
	}

	putchar('\n');
	free(buf);
}

void
clone_create(void)
{
	int s;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		err(1, "socket(AF_INET,SOCK_DGRAM)");

	memset(&ifr, 0, sizeof(ifr));
	(void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCIFCREATE, &ifr) < 0)
		err(1, "SIOCIFCREATE");

	/*
	 * If we get a different name back then we put in, we probably
	 * want to print it out, but we might change our mind later so
	 * we just signal our intrest and leave the printout for later.
	 */
	if (strcmp(name, ifr.ifr_name) != 0) {
		printname = 1;
		strlcpy(name, ifr.ifr_name, sizeof(name));
	}

	close(s);
}

static void
clone_destroy(const char *val, int d, int s, const struct afswtch *rafp)
{

	(void) strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCIFDESTROY, &ifr) < 0)
		err(1, "SIOCIFDESTROY");
	/*
	 * If we create and destroy an interface in the same command,
	 * there isn't any reason to print it's name.
	 */
	printname = 0;
}

static struct cmd clone_cmds[] = {
	DEF_CMD("destroy",	0,	clone_destroy),
	DEF_CMD("unplumb",	0,	clone_destroy),
};

static void
clone_Copt_cb(const char *optarg __unused)
{
	list_cloners();
	exit(0);
}
static struct option clone_Copt = { "C", "[-C]", clone_Copt_cb };

static __constructor void
clone_ctor(void)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	int i;

	for (i = 0; i < N(clone_cmds);  i++)
		cmd_register(&clone_cmds[i]);
	opt_register(&clone_Copt);
#undef N
}
