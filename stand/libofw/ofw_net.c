/*-
 * Copyright (c) 2000-2001 Benno Rice
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND
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
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

#include <stand.h>
#include <net.h>
#include <netif.h>

#include "libofw.h"
#include "openfirm.h"

static int	ofwn_probe(struct netif *, void *);
static int	ofwn_match(struct netif *, void *);
static void	ofwn_init(struct iodesc *, void *);
static ssize_t	ofwn_get(struct iodesc *, void **, time_t);
static ssize_t	ofwn_put(struct iodesc *, void *, size_t);
static void	ofwn_end(struct netif *);

extern struct netif_stats	ofwn_stats[];

struct netif_dif ofwn_ifs[] = {
	{
		.dif_unit=0,
		.dif_nsel=1,
		.dif_stats=&ofwn_stats[0],
		.dif_private=0,
	},
};

struct netif_stats ofwn_stats[nitems(ofwn_ifs)];

struct netif_driver ofwnet = {
	.netif_bname="net",
	.netif_match=ofwn_match,
	.netif_probe=ofwn_probe,
	.netif_init=ofwn_init,
	.netif_get=ofwn_get,
	.netif_put=ofwn_put,
	.netif_end=ofwn_end,
	.netif_ifs=ofwn_ifs,
	.netif_nifs=nitems(ofwn_ifs)
};

static ihandle_t	netinstance;

static void		*dmabuf;

static int
ofwn_match(struct netif *nif, void *machdep_hint)
{
	return (1);
}

static int
ofwn_probe(struct netif *nif, void *machdep_hint)
{
	return (0);
}

static ssize_t
ofwn_put(struct iodesc *desc, void *pkt, size_t len)
{
	size_t			sendlen;
	ssize_t			rv;

#if defined(NETIF_DEBUG)
	struct ether_header	*eh;
	printf("netif_put: desc=0x%x pkt=0x%x len=%d\n", desc, pkt, len);
	eh = pkt;
	printf("dst: %s ", ether_sprintf(eh->ether_dhost));
	printf("src: %s ", ether_sprintf(eh->ether_shost));
	printf("type: 0x%x\n", eh->ether_type & 0xffff);
#endif

	sendlen = len;
	if (sendlen < 60) {
		sendlen = 60;
#if defined(NETIF_DEBUG)
		printf("netif_put: length padded to %d\n", sendlen);
#endif
	}

	if (dmabuf) {
		bcopy(pkt, dmabuf, sendlen);
		pkt = dmabuf;
	}

	rv = OF_write(netinstance, pkt, len);

#if defined(NETIF_DEBUG)
	printf("netif_put: OF_write returned %d\n", rv);
#endif

	return (rv);
}

static ssize_t
ofwn_get(struct iodesc *desc, void **pkt, time_t timeout)
{
	time_t	t;
	ssize_t	length;
	size_t	len;
	char	*buf, *ptr;

#if defined(NETIF_DEBUG)
	printf("netif_get: pkt=%p, timeout=%d\n", pkt, timeout);
#endif

	/*
	 * We should read the "max-frame-size" int property instead,
	 * but at this time the iodesc does not have mtu, so we will take
	 * a small shortcut here.
	 */
	len = ETHER_MAX_LEN;
	buf = malloc(len + ETHER_ALIGN);
	if (buf == NULL)
		return (-1);
	ptr = buf + ETHER_ALIGN;

	t = getsecs();
	do {
		length = OF_read(netinstance, ptr, len);
	} while ((length == -2 || length == 0) &&
		(getsecs() - t < timeout));

#if defined(NETIF_DEBUG)
	printf("netif_get: received length=%d (%x)\n", length, length);
#endif

	if (length < 12) {
		free(buf);
		return (-1);
	}

#if defined(NETIF_VERBOSE_DEBUG)
	{
		char *ch = ptr;
		int i;

		for(i = 0; i < 96; i += 4) {
			printf("%02x%02x%02x%02x  ", ch[i], ch[i+1],
			    ch[i+2], ch[i+3]);
		}
		printf("\n");
	}
#endif

#if defined(NETIF_DEBUG)
	{
		struct ether_header *eh = ptr;

		printf("dst: %s ", ether_sprintf(eh->ether_dhost));
		printf("src: %s ", ether_sprintf(eh->ether_shost));
		printf("type: 0x%x\n", eh->ether_type & 0xffff);
	}
#endif

	*pkt = buf;
	return (length);
}

static void
ofwn_init(struct iodesc *desc, void *machdep_hint)
{
	phandle_t	netdev;
	char		path[64];
	char		*ch;
	int		pathlen;

	pathlen = OF_getprop(chosen, "bootpath", path, 64);
	if ((ch = strchr(path, ':')) != NULL)
		*ch = '\0';
	netdev = OF_finddevice(path);
	if (OF_getprop(netdev, "local-mac-address", desc->myea, 6) == -1)
		goto punt;

	printf("boot: ethernet address: %s\n", ether_sprintf(desc->myea));

	if ((netinstance = OF_open(path)) == -1) {
		printf("Could not open network device.\n");
		goto punt;
	}

#if defined(NETIF_DEBUG)
	printf("ofwn_init: Open Firmware instance handle: %08x\n", netinstance);
#endif
	dmabuf = NULL;
	if (OF_call_method("dma-alloc", netinstance, 1, 1, (64 * 1024), &dmabuf)
	    < 0) {
		printf("Failed to allocate DMA buffer (got %p).\n", dmabuf);
		goto punt;
	}
#if defined(NETIF_DEBUG)
	printf("ofwn_init: allocated DMA buffer: %p\n", dmabuf);
#endif

	return;

punt:
	printf("\n");
	printf("Could not boot from %s.\n", path);
	OF_enter();
}

static void
ofwn_end(struct netif *nif)
{
#ifdef BROKEN
	/* dma-free freezes at least some Apple ethernet controllers */
	OF_call_method("dma-free", netinstance, 2, 0, dmabuf, MAXPHYS);
#endif
	OF_close(netinstance);
}

#if 0
int
ofwn_getunit(const char *path)
{
	int		i;
	char		newpath[255];

	OF_canon(path, newpath, 254);

	for (i = 0; i < nofwninfo; i++) {
		printf(">>> test =\t%s\n", ofwninfo[i].ofwn_path);
		if (strcmp(path, ofwninfo[i].ofwn_path) == 0)
			return (i);

		if (strcmp(newpath, ofwninfo[i].ofwn_path) == 0)
			return (i);
	}

	return (-1);
}
#endif

/*
 * To properly match network devices, we have to subclass the netdev device.
 * It has a different devdesc than a normal network device (which is fine:
 * it's a struct superset) and different matching criteria (since it has to
 * look at the path, find a handle and see if that handle is a network node
 * or not).
 */

static int ofwnd_init(void);
static int ofwnd_parsedev(struct devdesc **, const char *, const char **);
static bool ofwnd_match(struct devsw *, const char *);
static char *ofwnd_fmtdev(struct devdesc *);

struct devsw ofw_netdev = {
	.dv_name = "network",
	.dv_type = DEVT_NET,
	.dv_init = ofwnd_init,
	.dv_match = ofwnd_match,
	.dv_fmtdev = ofwnd_fmtdev,
	.dv_parsedev = ofwnd_parsedev,
};

static int ofwnd_init(void)
{
	netdev.dv_init();
	ofw_netdev.dv_strategy = netdev.dv_strategy;
	ofw_netdev.dv_open = netdev.dv_open;
	ofw_netdev.dv_close = netdev.dv_close;
	ofw_netdev.dv_ioctl = netdev.dv_ioctl;
	ofw_netdev.dv_print = netdev.dv_print;
	ofw_netdev.dv_fmtdev = netdev.dv_fmtdev;
	/* parsedev is unique to ofwnd */
	/* match is unique to ofwnd */
	return (0);
}

static int
ofwnd_parsedev(struct devdesc **dev, const char *devspec, const char **path)
{
	return (ofw_common_parsedev(dev, devspec, path, ofw_netdev.dv_name));
}

static bool
ofwnd_match(struct devsw *devsw, const char *devspec)
{
	const char *path;

	return (ofw_path_to_handle(devspec, devsw->dv_name, &path) != -1);
}

static char *
ofwnd_fmtdev(struct devdesc *idev)
{
	struct ofw_devdesc *dev = (struct ofw_devdesc *)idev;

	return (dev->d_path);
}
