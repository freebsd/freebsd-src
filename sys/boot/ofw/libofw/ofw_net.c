/*
 * Copyright (c) 2000 Benno Rice
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <stand.h>
#include <net.h>
#include <netif.h>

int ofwn_probe();
int ofwn_match();
void ofwn_init();
int ofwn_get();
int ofwn_put();
void ofwn_end();

extern struct netif_stats	prom_stats[];

struct netif_dif prom_ifs[] = {
	/*	dif_unit	dif_nsel	dif_stats	dif_private	*/
	{	0,		1,		&prom_stats[0],	0,	},
};

struct netif_stats prom_stats[NENTS(prom_ifs)];

struct netif_driver ofwnet = {
	"net",			/* netif_bname */
	ofwn_match,		/* netif_match */
	ofwn_probe,		/* netif_probe */
	ofwn_init,		/* netif_init */
	ofwn_get,		/* netif_get */
	ofwn_put,		/* netif_put */
	ofwn_end,		/* netif_end */
	ofwn_ifs,		/* netif_ifs */
	NENTS(ofwn_ifs)		/* netif_nifs */
};

int netfd = 0, broken_firmware;

int
ofwn_match(struct netif *nif, void *machdep_hint)
{
	return (1);
}

int
ofwn_probe(struct netif *nif, void *machdep_hint)
{
	return 0;
}

int
ofwn_put(struct iodesc *desc, void *pkt, int len)
{
#if 0
	prom_write(netfd, len, pkt, 0);

	return len;
#endif
	return 0;
}

int
ofwn_get(struct iodesc *desc, void *pkt, int len, time_t timeout)
{
	return 0;
}

extern char *strchr();

void
ofw_init(struct iodesc *desc, void *machdep_hint)
{
	char devname[64];
	int devlen, i;
	int netbbinfovalid;
	char *enet_addr;
	prom_return_t ret;
	u_int64_t *qp, csum;

	broken_firmware = 0;

	csum = 0;
	for (i = 0, qp = (u_int64_t *)&netbbinfo;
	    i < (sizeof netbbinfo / sizeof (u_int64_t)); i++, qp++)
		csum += *qp;
	netbbinfovalid = (csum == 0);
	if (netbbinfovalid)
		netbbinfovalid = netbbinfo.set;

	ret.bits = prom_getenv(PROM_E_BOOTED_DEV, devname, sizeof(devname));
	devlen = ret.u.retval;

	/* Ethernet address is the 9th component of the booted_dev string. */
	enet_addr = devname;
	for (i = 0; i < 8; i++) {
		enet_addr = strchr(enet_addr, ' ');
		if (enet_addr == NULL) {
			printf(
		"boot: boot device name does not contain ethernet address.\n");
			goto punt;
		}
		enet_addr++;
	}
	if (enet_addr != NULL) {
		int hv, lv;

#define	dval(c)	(((c) >= '0' && (c) <= '9') ? ((c) - '0') : \
		(((c) >= 'A' && (c) <= 'F') ? (10 + (c) - 'A') : \
		(((c) >= 'a' && (c) <= 'f') ? (10 + (c) - 'a') : -1)))

		for (i = 0; i < 6; i++) {
			hv = dval(*enet_addr); enet_addr++;
			lv = dval(*enet_addr); enet_addr++;
			enet_addr++;

			if (hv == -1 || lv == -1) {
				printf(
		"boot: boot device name contains bogus ethernet address.\n");
				goto punt;
			}

			desc->myea[i] = (hv << 4) | lv;
		}
#undef dval
	}

	if (netbbinfovalid && netbbinfo.force) {
		printf("boot: using hard-coded ethernet address (forced).\n");
		bcopy(netbbinfo.ether_addr, desc->myea, sizeof desc->myea);
	}

gotit:
	printf("boot: ethernet address: %s\n", ether_sprintf(desc->myea));

	ret.bits = prom_open(devname, devlen + 1);
	if (ret.u.status) {
		printf("prom_init: open failed: %d\n", ret.u.status);
		goto reallypunt;
	}
	netfd = ret.u.retval;
	return;

punt:
	broken_firmware = 1;
	if (netbbinfovalid) {
		printf("boot: using hard-coded ethernet address.\n");
		bcopy(netbbinfo.ether_addr, desc->myea, sizeof desc->myea);
		goto gotit;
	}

reallypunt:
	printf("\n");
	printf("Boot device name was: \"%s\"\n", devname);
	printf("\n");
	printf("Your firmware may be too old to network-boot FreeBSD/alpha,\n");
	printf("or you might have to hard-code an ethernet address into\n");
	printf("your network boot block with setnetbootinfo(8).\n");
	halt();
}

void
ofwn_end(struct netif *nif)
{
    prom_close(netfd);
}
