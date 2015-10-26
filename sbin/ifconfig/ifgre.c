/*-
 * Copyright (c) 2008 Andrew Thompson. All rights reserved.
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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <stdlib.h>
#include <unistd.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_gre.h>
#include <net/route.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include "ifconfig.h"

static	void gre_status(int s);

static void
gre_status(int s)
{
	int grekey = 0;

	ifr.ifr_data = (caddr_t)&grekey;
	if (ioctl(s, GREGKEY, &ifr) == 0)
		if (grekey != 0)
			printf("\tgrekey: %d\n", grekey);
}

static void
setifgrekey(const char *val, int dummy __unused, int s, 
    const struct afswtch *afp)
{
	uint32_t grekey = atol(val);

	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	ifr.ifr_data = (caddr_t)&grekey;
	if (ioctl(s, GRESKEY, (caddr_t)&ifr) < 0)
		warn("ioctl (set grekey)");
}

static struct cmd gre_cmds[] = {
	DEF_CMD_ARG("grekey",			setifgrekey),
};
static struct afswtch af_gre = {
	.af_name	= "af_gre",
	.af_af		= AF_UNSPEC,
	.af_other_status = gre_status,
};

static __constructor void
gre_ctor(void)
{
	size_t i;

	for (i = 0; i < nitems(gre_cmds);  i++)
		cmd_register(&gre_cmds[i]);
	af_register(&af_gre);
}
