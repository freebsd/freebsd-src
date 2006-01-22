/*
 * Copyright (c) 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <stdlib.h>
#include <unistd.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_vlan_var.h>
#include <net/route.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include "ifconfig.h"

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif
static struct vlanreq		__vreq;
static int			__have_dev = 0;
static int			__have_tag = 0;

static void
vlan_status(int s)
{
	struct vlanreq		vreq;

	bzero((char *)&vreq, sizeof(vreq));
	ifr.ifr_data = (caddr_t)&vreq;

	if (ioctl(s, SIOCGETVLAN, (caddr_t)&ifr) == -1)
		return;

	printf("\tvlan: %d parent interface: %s\n",
	    vreq.vlr_tag, vreq.vlr_parent[0] == '\0' ?
	    "<none>" : vreq.vlr_parent);
}

static void
setvlantag(const char *val, int d, int s, const struct afswtch	*afp)
{
	char			*endp;
	u_long			ul;

	ul = strtoul(val, &endp, 0);
	if (*endp != '\0')
		errx(1, "invalid value for vlan");
	__vreq.vlr_tag = ul;
	/* check if the value can be represented in vlr_tag */
	if (__vreq.vlr_tag != ul)
		errx(1, "value for vlan out of range");
	/* the kernel will do more specific checks on vlr_tag */
	__have_tag = 1;
}

static void
setvlandev(const char *val, int d, int s, const struct afswtch	*afp)
{

	strncpy(__vreq.vlr_parent, val, sizeof(__vreq.vlr_parent));
	__have_dev = 1;
}

static void
unsetvlandev(const char *val, int d, int s, const struct afswtch *afp)
{

	if (val != NULL)
		warnx("argument to -vlandev is useless and hence deprecated");

	bzero((char *)&__vreq, sizeof(__vreq));
	ifr.ifr_data = (caddr_t)&__vreq;
#if 0	/* this code will be of use when we can alter vlan or vlandev only */
	if (ioctl(s, SIOCGETVLAN, (caddr_t)&ifr) == -1)
		err(1, "SIOCGETVLAN");

	bzero((char *)&__vreq.vlr_parent, sizeof(__vreq.vlr_parent));
	__vreq.vlr_tag = 0; /* XXX clear parent only (no kernel support now) */
#endif
	if (ioctl(s, SIOCSETVLAN, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETVLAN");
	__have_dev = __have_tag = 0;
}

static void
vlan_cb(int s, void *arg)
{

	if (__have_tag ^ __have_dev)
		errx(1, "both vlan and vlandev must be specified");

	if (__have_tag && __have_dev) {
		ifr.ifr_data = (caddr_t)&__vreq;
		if (ioctl(s, SIOCSETVLAN, (caddr_t)&ifr) == -1)
			err(1, "SIOCSETVLAN");
	}
}

static struct cmd vlan_cmds[] = {
	DEF_CMD_ARG("vlan",				setvlantag),
	DEF_CMD_ARG("vlandev",				setvlandev),
	/* XXX For compatibility.  Should become DEF_CMD() some day. */
	DEF_CMD_OPTARG("-vlandev",			unsetvlandev),
	DEF_CMD("vlanmtu",	IFCAP_VLAN_MTU,		setifcap),
	DEF_CMD("-vlanmtu",	-IFCAP_VLAN_MTU,	setifcap),
	DEF_CMD("vlanhwtag",	IFCAP_VLAN_HWTAGGING,	setifcap),
	DEF_CMD("-vlanhwtag",	-IFCAP_VLAN_HWTAGGING,	setifcap),
};
static struct afswtch af_vlan = {
	.af_name	= "af_vlan",
	.af_af		= AF_UNSPEC,
	.af_other_status = vlan_status,
};

static __constructor void
vlan_ctor(void)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	int i;

	for (i = 0; i < N(vlan_cmds);  i++)
		cmd_register(&vlan_cmds[i]);
	af_register(&af_vlan);
	callback_register(vlan_cb, NULL);
#undef N
}
