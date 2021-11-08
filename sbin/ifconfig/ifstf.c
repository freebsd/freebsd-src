/*-
 * Copyright 2013 Ermal Luci
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
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <stdlib.h>
#include <unistd.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <sys/mbuf.h>
#include <net/if_stf.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include "ifconfig.h"

static int
do_cmd(int sock, u_long op, void *arg, size_t argsize, int set)
{
	struct ifdrv ifd;

	memset(&ifd, 0, sizeof(ifd));

	strlcpy(ifd.ifd_name, ifr.ifr_name, sizeof(ifd.ifd_name));
	ifd.ifd_cmd = op;
	ifd.ifd_len = argsize;
	ifd.ifd_data = arg;

	return (ioctl(sock, set ? SIOCSDRVSPEC : SIOCGDRVSPEC, &ifd));
}

static void
stf_status(int s)
{
	struct stfv4args param;

	if (do_cmd(s, STF6RD_GV4NET, &param, sizeof(param), 0) < 0)
		return;

	printf("\tv4net %s/%d -> ", inet_ntoa(param.srcv4_addr),
	    param.v4_prefixlen ? param.v4_prefixlen : 32);
	printf("tv4br %s\n", inet_ntoa(param.braddr));
}

static void
setstf_br(const char *val, int d, int s, const struct afswtch *afp)
{
	struct stfv4args req;
	struct sockaddr_in sin;

	memset(&req, 0, sizeof(req));

	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;

	if (!inet_aton(val, &sin.sin_addr))
		errx(1, "%s: bad value", val);

	req.braddr = sin.sin_addr;
	if (do_cmd(s, STF6RD_SBR, &req, sizeof(req), 1) < 0)
		err(1, "STF6RD_SBR%s",  val);
}

static void
setstf_set(const char *val, int d, int s, const struct afswtch *afp)
{
	struct stfv4args req;
	struct sockaddr_in sin;
	const char *errstr;
	char *p = NULL;

	memset(&req, 0, sizeof(req));

	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;

	p = strrchr(val, '/');
	if (p == NULL)
		errx(2, "Wrong argument given");

	*p = '\0';
	req.v4_prefixlen = (int)strtonum(p + 1, 0, 32, &errstr);
	if (errstr != NULL || req.v4_prefixlen == 0) {
		*p = '/';
		errx(1, "%s: bad value (prefix length %s)", val, errstr);
	}

	if (!inet_aton(val, &sin.sin_addr))
		errx(1, "%s: bad value", val);

	memcpy(&req.srcv4_addr, &sin.sin_addr, sizeof(req.srcv4_addr));
	if (do_cmd(s, STF6RD_SV4NET, &req, sizeof(req), 1) < 0)
		err(1, "STF6RD_SV4NET %s",  val);
}

static struct cmd stf_cmds[] = {
	DEF_CMD_ARG("stfv4net",		setstf_set),
	DEF_CMD_ARG("stfv4br",		setstf_br),
};

static struct afswtch af_stf = {
	.af_name		= "af_stf",
	.af_af			= AF_UNSPEC,
	.af_other_status	= stf_status,
};

static __constructor void
stf_ctor(void)
{
	int i;

	for (i = 0; i < nitems(stf_cmds);  i++)
		cmd_register(&stf_cmds[i]);
	af_register(&af_stf);
}
