/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *
 *	from: @(#)autoconf.c	7.1 (Berkeley) 5/9/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mount.h>

#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsdiskless.h>

static int inaddr_to_sockaddr(char *ev, struct sockaddr_in *sa);
static int hwaddr_to_sockaddr(char *ev, struct sockaddr_dl *sa);
static int decode_nfshandle(char *ev, u_char *fh);

/*
 * Populate the essential fields in the nfsv3_diskless structure.
 *
 * The loader is expected to export the following environment variables:
 *
 * boot.netif.ip		IP address on boot interface
 * boot.netif.netmask		netmask on boot interface
 * boot.netif.gateway		default gateway (optional)
 * boot.netif.hwaddr		hardware address of boot interface
 * boot.nfsroot.server		IP address of root filesystem server
 * boot.nfsroot.path		path of the root filesystem on server
 * boot.nfsroot.nfshandle	NFS handle for root filesystem on server
 */
void
nfs_setup_diskless(void)
{
	struct nfs_diskless *nd = &nfs_diskless;
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl, ourdl;
	struct sockaddr_in myaddr, netmask;
	char *cp;

	if (nfs_diskless_valid)
		return;
	/* set up interface */
	if (inaddr_to_sockaddr("boot.netif.ip", &myaddr))
		return;
	if (inaddr_to_sockaddr("boot.netif.netmask", &netmask)) {
		printf("nfs_diskless: no netmask\n");
		return;
	}
	bcopy(&myaddr, &nd->myif.ifra_addr, sizeof(myaddr));
	bcopy(&myaddr, &nd->myif.ifra_broadaddr, sizeof(myaddr));
	((struct sockaddr_in *) &nd->myif.ifra_broadaddr)->sin_addr.s_addr =
		myaddr.sin_addr.s_addr | ~ netmask.sin_addr.s_addr;
	bcopy(&netmask, &nd->myif.ifra_mask, sizeof(netmask));

	if (hwaddr_to_sockaddr("boot.netif.hwaddr", &ourdl)) {
		printf("nfs_diskless: no hardware address\n");
		return;
	}
	ifa = NULL;
	IFNET_RLOCK();
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if ((ifa->ifa_addr->sa_family == AF_LINK) &&
			    (sdl = ((struct sockaddr_dl *)ifa->ifa_addr))) {
				if ((sdl->sdl_type == ourdl.sdl_type) &&
				    (sdl->sdl_alen == ourdl.sdl_alen) &&
				    !bcmp(sdl->sdl_data + sdl->sdl_nlen,
					  ourdl.sdl_data + ourdl.sdl_nlen, 
					  sdl->sdl_alen)) {
				    IFNET_RUNLOCK();
				    goto match_done;
				}
			}
		}
	}
	IFNET_RUNLOCK();
	printf("nfs_diskless: no interface\n");
	return;	/* no matching interface */
match_done:
	sprintf(nd->myif.ifra_name, "%s%d", ifp->if_name, ifp->if_unit);
	
	/* set up gateway */
	inaddr_to_sockaddr("boot.netif.gateway", &nd->mygateway);

	/* set up root mount */
	nd->root_args.rsize = 8192;		/* XXX tunable? */
	nd->root_args.wsize = 8192;
	nd->root_args.sotype = SOCK_DGRAM;
	nd->root_args.flags = (NFSMNT_WSIZE | NFSMNT_RSIZE | NFSMNT_RESVPORT);
	if (inaddr_to_sockaddr("boot.nfsroot.server", &nd->root_saddr)) {
		printf("nfs_diskless: no server\n");
		return;
	}
	nd->root_saddr.sin_port = htons(NFS_PORT);
	if (decode_nfshandle("boot.nfsroot.nfshandle", &nd->root_fh[0]) == 0) {
		printf("nfs_diskless: no NFS handle\n");
		return;
	}
	if ((cp = getenv("boot.nfsroot.path")) != NULL) {
		strncpy(nd->root_hostnam, cp, MNAMELEN - 1);
		freeenv(cp);
	}

	nfs_diskless_valid = 1;
}

static int
inaddr_to_sockaddr(char *ev, struct sockaddr_in *sa)
{
	u_int32_t a[4];
	char *cp;
	int count;

	bzero(sa, sizeof(*sa));
	sa->sin_len = sizeof(*sa);
	sa->sin_family = AF_INET;

	if ((cp = getenv(ev)) == NULL)
		return (1);
	count = sscanf(cp, "%d.%d.%d.%d", &a[0], &a[1], &a[2], &a[3]);
	freeenv(cp);
	if (count != 4)
		return (1);
	sa->sin_addr.s_addr =
	    htonl((a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3]);
	return (0);
}

static int
hwaddr_to_sockaddr(char *ev, struct sockaddr_dl *sa)
{
	char *cp;
	u_int32_t a[6];
	int count;

	bzero(sa, sizeof(*sa));
	sa->sdl_len = sizeof(*sa);
	sa->sdl_family = AF_LINK;
	sa->sdl_type = IFT_ETHER;
	sa->sdl_alen = ETHER_ADDR_LEN;
	if ((cp = getenv(ev)) == NULL)
		return (1);
	count = sscanf(cp, "%x:%x:%x:%x:%x:%x",
	    &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]);
	freeenv(cp);
	if (count != 6)
		return (1);
	sa->sdl_data[0] = a[0];
	sa->sdl_data[1] = a[1];
	sa->sdl_data[2] = a[2];
	sa->sdl_data[3] = a[3];
	sa->sdl_data[4] = a[4];
	sa->sdl_data[5] = a[5];
	return (0);
}

static int
decode_nfshandle(char *ev, u_char *fh) 
{
	u_char *cp, *ep;
	int len, val;

	ep = cp = getenv(ev);
	if (cp == NULL)
		return (0);
	if ((strlen(cp) < 2) || (*cp != 'X')) {
		freeenv(ep);
		return (0);
	}
	len = 0;
	cp++;
	for (;;) {
		if (*cp == 'X') {
			freeenv(ep);
			return (len);
		}
		if ((sscanf(cp, "%2x", &val) != 1) || (val > 0xff)) {
			freeenv(ep);
			return (0);
		}
		*(fh++) = val;
		len++;
		cp += 2;
		if (len > NFSX_V2FH) {
		    freeenv(ep);
		    return (0);
		}
	}
}
