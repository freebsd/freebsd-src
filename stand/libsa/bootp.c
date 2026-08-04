/*	$NetBSD: bootp.c,v 1.14 1998/02/16 11:10:54 drochner Exp $	*/

/*
 * Copyright (c) 1992 Regents of the University of California.
 * All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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

#include <stddef.h>
#include <sys/types.h>
#include <sys/limits.h>
#include <sys/endian.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <string.h>

#define SUPPORT_DHCP

#define	DHCP_ENV_NOVENDOR	1	/* do not parse vendor options */
#define	DHCP_ENV_PXE		10	/* assume pxe vendor options */
#define	DHCP_ENV_FREEBSD	11	/* assume freebsd vendor options */
/* set DHCP_ENV to one of the values above to export dhcp options to kenv */
#define DHCP_ENV		DHCP_ENV_NO_VENDOR

#include "stand.h"
#include "net.h"
#include "netif.h"
#include "bootp.h"


struct in_addr servip;

static time_t	bot;

static	char vm_rfc1048[4] = VM_RFC1048;
#ifdef BOOTP_VEND_CMU
static	char vm_cmu[4] = VM_CMU;
#endif

/* Local forwards */
static	ssize_t bootpsend(struct iodesc *, void *, size_t);
static	ssize_t bootprecv(struct iodesc *, void **, void **, time_t, void *);
static	int vend_rfc1048(u_char *, u_int);
#ifdef BOOTP_VEND_CMU
static	void vend_cmu(u_char *);
#endif

#ifdef DHCP_ENV		/* export the dhcp response to kenv */
struct dhcp_opt;
static void setenv_(u_char *cp,  u_char *ep, struct dhcp_opt *opts);
#else
#define setenv_(a, b, c)
#endif

#ifdef SUPPORT_DHCP
static char expected_dhcpmsgtype = -1, dhcp_ok;
struct in_addr dhcp_serverip;
#endif
struct bootp *bootp_response;
size_t bootp_response_size;

/*
 * DHCP Client-System Architecture per RFC 4578 (option 93).  Each netif
 * driver that has a spec-defined value for what it is sets this from
 * its own init path (e.g. efinet_dev_init on UEFI, pxe_init on legacy
 * BIOS PXE).  BOOTP_ARCH_UNSET means the loader has no honest value to
 * report (e.g. U-Boot or OpenFirmware, for which RFC 4578 has no
 * assignment), and the option is omitted from the DHCP request rather
 * than misrepresenting the client.
 */
#define	BOOTP_ARCH_UNSET	0xFFFF
uint16_t bootp_client_arch = BOOTP_ARCH_UNSET;

static void
bootp_fill_request(unsigned char *bp_vend)
{
	int off = 0;

	/*
	 * We are booting from PXE, we want to send the string
	 * 'PXEClient' to the DHCP server so you have the option of
	 * only responding to PXE aware dhcp requests.
	 */
	bp_vend[off++] = TAG_CLASSID;
	bp_vend[off++] = 9;
	bcopy("PXEClient", &bp_vend[off], 9);
	off += 9;

	bp_vend[off++] = TAG_USER_CLASS;
	/* len of each user class + number of user class */
	bp_vend[off++] = 8;
	/* len of the first user class */
	bp_vend[off++] = 7;
	bcopy("FreeBSD", &bp_vend[off], 7);
	off += 7;

	/*
	 * Client architecture (RFC 4578).  Value is 2 bytes big-endian.
	 * Omit entirely for loaders that don't have a spec-defined value.
	 */
	if (bootp_client_arch != BOOTP_ARCH_UNSET) {
		bp_vend[off++] = TAG_CLIENT_ARCH;
		bp_vend[off++] = 2;
		bp_vend[off++] = (bootp_client_arch >> 8) & 0xff;
		bp_vend[off++] = bootp_client_arch & 0xff;
	}

	/*
	 * Maximum DHCP message size we can accept (RFC 2132).  The loader's
	 * UDP read path allocates dynamically per packet up to the interface
	 * MTU, so advertise ~MTU to unlock large option payloads that some
	 * servers would otherwise trim to the RFC 2131 § 4.4.1 576-byte
	 * default.
	 */
	bp_vend[off++] = TAG_MAXSIZE;
	bp_vend[off++] = 2;
	bp_vend[off++] = (1472 >> 8) & 0xff;
	bp_vend[off++] = 1472 & 0xff;

	bp_vend[off++] = TAG_PARAM_REQ;
	bp_vend[off++] = 7;
	bp_vend[off++] = TAG_ROOTPATH;
	bp_vend[off++] = TAG_HOSTNAME;
	bp_vend[off++] = TAG_SWAPSERVER;
	bp_vend[off++] = TAG_GATEWAY;
	bp_vend[off++] = TAG_SUBNET_MASK;
	bp_vend[off++] = TAG_INTF_MTU;
	bp_vend[off++] = TAG_SERVERID;
	bp_vend[off] = TAG_END;
}

/* Fetch required bootp infomation */
void
bootp(int sock)
{
	void *pkt = NULL;
	struct iodesc *d;
	struct bootp *bp;
	struct {
		u_char header[HEADER_SIZE];
		struct bootp wbootp;
	} wbuf;
	struct bootp *rbootp;
	bool init_reboot = false;

	DEBUG_PRINTF(1, ("bootp: socket=%d\n", sock));
	if (!bot)
		bot = getsecs();

	if (!(d = socktodesc(sock))) {
		printf("bootp: bad socket. %d\n", sock);
		return;
	}
	DEBUG_PRINTF(1, ("bootp: socktodesc=%lx\n", (long)d));

restart:
	bp = &wbuf.wbootp;
	bzero(bp, sizeof(*bp));

	bp->bp_op = BOOTREQUEST;
	bp->bp_htype = 1;		/* 10Mb Ethernet (48 bits) */
	bp->bp_hlen = 6;
	MACPY(d->myea, bp->bp_chaddr);
	strncpy(bp->bp_file, bootfile, sizeof(bp->bp_file));
	bcopy(vm_rfc1048, bp->bp_vend, sizeof(vm_rfc1048));

	d->myip.s_addr = INADDR_ANY;
	d->myport = htons(IPPORT_BOOTPC);
	d->destip.s_addr = INADDR_BROADCAST;
	d->destport = htons(IPPORT_BOOTPS);

#ifdef SUPPORT_DHCP
	/*
	 * If a DHCP reply is already cached (populated by a lower layer
	 * that captured it from firmware, e.g. UEFI PXE Base Code), enter
	 * RFC 2131 § 4.3.2 INIT-REBOOT: skip DISCOVER/OFFER and go
	 * straight to REQUEST with option 50 = cached IP.  The DHCP
	 * server confirms the lease with an ACK containing whatever
	 * options this client asks for, which may be a superset of what
	 * the firmware asked for (e.g. option 26 interface-MTU, option 16
	 * swap-server).  On NAK or timeout, fall back to a full
	 * DISCOVER/OFFER/REQUEST/ACK cycle.
	 */
	if (bootp_response != NULL &&
	    bootp_response->bp_yiaddr.s_addr != INADDR_ANY) {
		init_reboot = true;
		rbootp = bootp_response;
		/*
		 * INIT-REBOOT is a new DHCP transaction, distinct from
		 * the firmware's cached DISCOVER/OFFER/REQUEST/ACK.
		 * Use the firmware's xid just as an entropy source.
		 */
		d->xid = ntohl(rbootp->bp_xid) + 1;
		DEBUG_PRINTF(1, ("bootp: using cached DHCP reply "
		    "(INIT-REBOOT), yiaddr=%s xid=0x%08x\n",
		    inet_ntoa(rbootp->bp_yiaddr), (unsigned)d->xid));
	} else
#endif
	if (d->xid == 0) {
		/*
		 * RFC 951 / RFC 2131 § 4.1 recommend a random xid.
		 * Without a real RNG in the loader, seed from the
		 * interface MAC (host-unique) mixed with the current
		 * time (unique across reboots on the same host).  This
		 * avoids the pre-existing behavior of every fresh
		 * BOOTP/DHCP client sending xid=0, which risks
		 * broadcast-reply mis-correlation when several PXE
		 * clients boot concurrently — bootprecv() enforces
		 * xid matching.
		 */
		memcpy(&d->xid, &d->myea[2], sizeof(d->xid));
		d->xid ^= (uint32_t)getsecs();
	}
	bp->bp_xid = htonl(d->xid);

	if (!init_reboot) {
#ifdef SUPPORT_DHCP
		bp->bp_vend[4] = TAG_DHCP_MSGTYPE;
		bp->bp_vend[5] = 1;
		bp->bp_vend[6] = DHCPDISCOVER;
		bootp_fill_request(&bp->bp_vend[7]);
		expected_dhcpmsgtype = DHCPOFFER;
		dhcp_ok = 0;
#else
		bp->bp_vend[4] = TAG_END;
#endif

		if (sendrecv(d,
			    bootpsend, bp, sizeof(*bp),
			    bootprecv, &pkt, (void **)&rbootp, NULL) == -1) {
			printf("bootp: no reply\n");
			return;
		}
	}

#ifdef SUPPORT_DHCP
	if (dhcp_ok || init_reboot) {
		uint32_t leasetime;
		int off;

		bp->bp_vend[4] = TAG_DHCP_MSGTYPE;
		bp->bp_vend[5] = 1;
		bp->bp_vend[6] = DHCPREQUEST;
		bp->bp_vend[7] = TAG_REQ_ADDR;
		bp->bp_vend[8] = 4;
		bcopy(&rbootp->bp_yiaddr, &bp->bp_vend[9], 4);
		off = 13;
		if (!init_reboot) {
			/* SELECTING: server-id identifies the server whose
			 * OFFER we accepted.  INIT-REBOOT omits it (RFC 2131
			 * § 4.3.2). */
			bp->bp_vend[off++] = TAG_SERVERID;
			bp->bp_vend[off++] = 4;
			bcopy(&dhcp_serverip.s_addr, &bp->bp_vend[off], 4);
			off += 4;
		}
		bp->bp_vend[off++] = TAG_LEASETIME;
		bp->bp_vend[off++] = 4;
		leasetime = htonl(300);
		bcopy(&leasetime, &bp->bp_vend[off], 4);
		off += 4;
		bootp_fill_request(&bp->bp_vend[off]);

		expected_dhcpmsgtype = DHCPACK;

		if (pkt != NULL) {
			free(pkt);
			pkt = NULL;
		}
		if (sendrecv(d,
			    bootpsend, bp, sizeof(*bp),
			    bootprecv, &pkt, (void **)&rbootp, NULL) == -1) {
			if (init_reboot) {
				printf("bootp: INIT-REBOOT failed, "
				    "falling back to DISCOVER\n");
				free(bootp_response);
				bootp_response = NULL;
				bootp_response_size = 0;
				init_reboot = false;
				++d->xid;
				if (pkt != NULL) {
					free(pkt);
					pkt = NULL;
				}
				goto restart;
			}
			printf("DHCPREQUEST failed\n");
			return;
		}
		if (init_reboot)
			DEBUG_PRINTF(1, ("bootp: INIT-REBOOT ACK received\n"));
	}
#endif

	myip = d->myip = rbootp->bp_yiaddr;
	servip = rbootp->bp_siaddr;
	if (rootip.s_addr == INADDR_ANY)
		rootip = servip;
	bcopy(rbootp->bp_file, bootfile, sizeof(bootfile));
	bootfile[sizeof(bootfile) - 1] = '\0';

	if (!netmask) {
		if (IN_CLASSA(ntohl(myip.s_addr)))
			netmask = htonl(IN_CLASSA_NET);
		else if (IN_CLASSB(ntohl(myip.s_addr)))
			netmask = htonl(IN_CLASSB_NET);
		else
			netmask = htonl(IN_CLASSC_NET);
		DEBUG_PRINTF(1, ("'native netmask' is %s\n", intoa(netmask)));
	}

	DEBUG_PRINTF(1,("rootip: %s\n", inet_ntoa(rootip)));
	DEBUG_PRINTF(1,("mask: %s\n", intoa(netmask)));

	/* We need a gateway if root is on a different net */
	if (!SAMENET(myip, rootip, netmask)) {
		DEBUG_PRINTF(1,("need gateway for root ip\n"));
	}

	/* Toss gateway if on a different net */
	if (!SAMENET(myip, gateip, netmask)) {
		DEBUG_PRINTF(1,("gateway ip (%s) bad\n", inet_ntoa(gateip)));
		gateip.s_addr = 0;
	}

	/* Bump xid so next request will be unique. */
	++d->xid;
	free(pkt);
}

/* Transmit a bootp request */
static ssize_t
bootpsend(struct iodesc *d, void *pkt, size_t len)
{
	struct bootp *bp;

	DEBUG_PRINTF(1,("bootpsend: d=%lx called.\n", (long)d));
	bp = pkt;
	bp->bp_secs = htons((u_short)(getsecs() - bot));

	DEBUG_PRINTF(1,("bootpsend: calling sendudp\n"));

	return (sendudp(d, pkt, len));
}

static ssize_t
bootprecv(struct iodesc *d, void **pkt, void **payload, time_t tleft,
    void *extra)
{
	ssize_t n;
	struct bootp *bp;
	void *ptr;

	DEBUG_PRINTF(1,("bootp_recvoffer: called\n"));

	ptr = NULL;
	n = readudp(d, &ptr, (void **)&bp, tleft);
	if (n == -1 || n < sizeof(struct bootp) - BOOTP_VENDSIZE)
		goto bad;

	DEBUG_PRINTF(1,("bootprecv: checked.  bp = %p, n = %zd\n", bp, n));

	if (bp->bp_xid != htonl(d->xid)) {
		DEBUG_PRINTF(1,("bootprecv: expected xid 0x%lx, got 0x%x\n",
			d->xid, ntohl(bp->bp_xid)));
		goto bad;
	}

	DEBUG_PRINTF(1,("bootprecv: got one!\n"));

	/* Suck out vendor info */
	if (bcmp(vm_rfc1048, bp->bp_vend, sizeof(vm_rfc1048)) == 0) {
		int vsize = n - offsetof(struct bootp, bp_vend);
		if (vend_rfc1048(bp->bp_vend, vsize) != 0)
		    goto bad;

		/* Save copy of bootp reply or DHCP ACK message */
		if (bp->bp_op == BOOTREPLY &&
		    ((dhcp_ok == 1 && expected_dhcpmsgtype == DHCPACK) ||
		    dhcp_ok == 0)) {
			free(bootp_response);
			bootp_response = malloc(n);
			if (bootp_response != NULL) {
				bootp_response_size = n;
				bcopy(bp, bootp_response, bootp_response_size);
			}
		}
	}
#ifdef BOOTP_VEND_CMU
	else if (bcmp(vm_cmu, bp->bp_vend, sizeof(vm_cmu)) == 0)
		vend_cmu(bp->bp_vend);
#endif
	else
		printf("bootprecv: unknown vendor 0x%lx\n", (long)bp->bp_vend);

	*pkt = ptr;
	*payload = bp;
	return (n);
bad:
	free(ptr);
	errno = 0;
	return (-1);
}

static int
vend_rfc1048(u_char *cp, u_int len)
{
	u_char *ep;
	int size;
	u_char tag;
	const char *val;

	DEBUG_PRINTF(1,("vend_rfc1048 bootp info. len=%d\n", len));
	ep = cp + len;

	/* Step over magic cookie */
	cp += sizeof(int);

	setenv_(cp, ep, NULL);

	while (cp < ep) {
		tag = *cp++;
		size = *cp++;
		if (tag == TAG_END)
			break;

		if (tag == TAG_SUBNET_MASK) {
			bcopy(cp, &netmask, sizeof(netmask));
		}
		if (tag == TAG_GATEWAY) {
			bcopy(cp, &gateip.s_addr, sizeof(gateip.s_addr));
		}
		if (tag == TAG_SWAPSERVER) {
			/* let it override bp_siaddr */
			bcopy(cp, &rootip.s_addr, sizeof(rootip.s_addr));
		}
		if (tag == TAG_ROOTPATH) {
			if ((val = getenv("dhcp.root-path")) == NULL)
				val = (const char *)cp;
			strlcpy(rootpath, val, sizeof(rootpath));
		}
		if (tag == TAG_HOSTNAME) {
			if ((val = getenv("dhcp.host-name")) == NULL)
				val = (const char *)cp;
			strlcpy(hostname, val, sizeof(hostname));
		}
		if (tag == TAG_INTF_MTU) {
			intf_mtu = 0;
			if ((val = getenv("dhcp.interface-mtu")) != NULL) {
				unsigned long tmp;
				char *end;

				errno = 0;
				/*
				 * Do not allow MTU to exceed max IPv4 packet
				 * size, max value of 16-bit word.
				 */
				tmp = strtoul(val, &end, 0);
				if (errno != 0 ||
				    *val == '\0' || *end != '\0' ||
				    tmp > USHRT_MAX) {
					printf("%s: bad value: \"%s\", "
					    "ignoring\n",
					    "dhcp.interface-mtu", val);
				} else {
					intf_mtu = (u_int)tmp;
				}
			}
			if (intf_mtu <= 0)
				intf_mtu = be16dec(cp);
		}
#ifdef SUPPORT_DHCP
		if (tag == TAG_DHCP_MSGTYPE) {
			if(*cp != expected_dhcpmsgtype)
			    return(-1);
			dhcp_ok = 1;
		}
		if (tag == TAG_SERVERID) {
			bcopy(cp, &dhcp_serverip.s_addr,
			      sizeof(dhcp_serverip.s_addr));
		}
#endif
		cp += size;
	}
	return(0);
}

#ifdef BOOTP_VEND_CMU
static void
vend_cmu(u_char *cp)
{
	struct cmu_vend *vp;

	DEBUG_PRINTF(1,("vend_cmu bootp info.\n"));

	vp = (struct cmu_vend *)cp;

	if (vp->v_smask.s_addr != 0) {
		netmask = vp->v_smask.s_addr;
	}
	if (vp->v_dgate.s_addr != 0) {
		gateip = vp->v_dgate;
	}
}
#endif

#ifdef DHCP_ENV
/*
 * Parse DHCP options and store them into kenv variables.
 * Original code from Danny Braniss, modifications by Luigi Rizzo.
 *
 * The parser is driven by tables which specify the type and name of
 * each dhcp option and how it appears in kenv.
 * The first entry in the list contains the prefix used to set the kenv
 * name (including the . if needed), the last entry must have a 0 tag.
 * Entries do not need to be sorted though it helps for readability.
 *
 * Certain vendor-specific tables can be enabled according to DHCP_ENV.
 * Set it to 0 if you don't want any.
 */
enum opt_fmt { __NONE = 0,
	__8 = 1, __16 = 2, __32 = 4,	/* Unsigned fields, value=size	*/
	__IP,				/* IPv4 address			*/
	__TXT,				/* C string			*/
	__BYTES,			/* byte sequence, printed %02x	*/
	__INDIR,			/* name=value			*/
	__ILIST,			/* name=value;name=value ... */
	__VE,				/* vendor specific, recurse	*/
};

struct dhcp_opt {
	uint8_t	tag;
	uint8_t	fmt;
	const char	*desc;
};

static struct dhcp_opt vndr_opt[] = { /* Vendor Specific Options */
#if DHCP_ENV == DHCP_ENV_FREEBSD /* FreeBSD table in the original code */
	{0,	0,	"FreeBSD"},		/* prefix */
	{1,	__TXT,	"kernel"},
	{2,	__TXT,	"kernelname"},
	{3,	__TXT,	"kernel_options"},
	{4,	__IP,	"usr-ip"},
	{5,	__TXT,	"conf-path"},
	{6,	__TXT,	"rc.conf0"},
	{7,	__TXT,	"rc.conf1"},
	{8,	__TXT,	"rc.conf2"},
	{9,	__TXT,	"rc.conf3"},
	{10,	__TXT,	"rc.conf4"},
	{11,	__TXT,	"rc.conf5"},
	{12,	__TXT,	"rc.conf6"},
	{13,	__TXT,	"rc.conf7"},
	{14,	__TXT,	"rc.conf8"},
	{15,	__TXT,	"rc.conf9"},

	{20,	__TXT,  "boot.nfsroot.options"},

	{245,	__INDIR, ""},
	{246,	__INDIR, ""},
	{247,	__INDIR, ""},
	{248,	__INDIR, ""},
	{249,	__INDIR, ""},
	{250,	__INDIR, ""},
	{251,	__INDIR, ""},
	{252,	__INDIR, ""},
	{253,	__INDIR, ""},
	{254,	__INDIR, ""},

#elif DHCP_ENV == DHCP_ENV_PXE		/* some pxe options, RFC4578 */
	{0,	0,	"pxe"},		/* prefix */
	{93,	__16,	"system-architecture"},
	{94,	__BYTES,	"network-interface"},
	{97,	__BYTES,	"machine-identifier"},
#else					/* default (empty) table */
	{0,	0,	"dhcp.vendor."},		/* prefix */
#endif
	{0,	__TXT,	"%soption-%d"}
};

static struct dhcp_opt dhcp_opt[] = {
	/* DHCP Option names, formats and codes, from RFC2132. */
	{0,	0,	"dhcp."},	// prefix
	{1,	__IP,	"subnet-mask"},
	{2,	__32,	"time-offset"}, /* this is signed */
	{3,	__IP,	"routers"},
	{4,	__IP,	"time-servers"},
	{5,	__IP,	"ien116-name-servers"},
	{6,	__IP,	"domain-name-servers"},
	{7,	__IP,	"log-servers"},
	{8,	__IP,	"cookie-servers"},
	{9,	__IP,	"lpr-servers"},
	{10,	__IP,	"impress-servers"},
	{11,	__IP,	"resource-location-servers"},
	{12,	__TXT,	"host-name"},
	{13,	__16,	"boot-size"},
	{14,	__TXT,	"merit-dump"},
	{15,	__TXT,	"domain-name"},
	{16,	__IP,	"swap-server"},
	{17,	__TXT,	"root-path"},
	{18,	__TXT,	"extensions-path"},
	{19,	__8,	"ip-forwarding"},
	{20,	__8,	"non-local-source-routing"},
	{21,	__IP,	"policy-filter"},
	{22,	__16,	"max-dgram-reassembly"},
	{23,	__8,	"default-ip-ttl"},
	{24,	__32,	"path-mtu-aging-timeout"},
	{25,	__16,	"path-mtu-plateau-table"},
	{26,	__16,	"interface-mtu"},
	{27,	__8,	"all-subnets-local"},
	{28,	__IP,	"broadcast-address"},
	{29,	__8,	"perform-mask-discovery"},
	{30,	__8,	"mask-supplier"},
	{31,	__8,	"perform-router-discovery"},
	{32,	__IP,	"router-solicitation-address"},
	{33,	__IP,	"static-routes"},
	{34,	__8,	"trailer-encapsulation"},
	{35,	__32,	"arp-cache-timeout"},
	{36,	__8,	"ieee802-3-encapsulation"},
	{37,	__8,	"default-tcp-ttl"},
	{38,	__32,	"tcp-keepalive-interval"},
	{39,	__8,	"tcp-keepalive-garbage"},
	{40,	__TXT,	"nis-domain"},
	{41,	__IP,	"nis-servers"},
	{42,	__IP,	"ntp-servers"},
	{43,	__VE,	"vendor-encapsulated-options"},
	{44,	__IP,	"netbios-name-servers"},
	{45,	__IP,	"netbios-dd-server"},
	{46,	__8,	"netbios-node-type"},
	{47,	__TXT,	"netbios-scope"},
	{48,	__IP,	"x-font-servers"},
	{49,	__IP,	"x-display-managers"},
	{50,	__IP,	"dhcp-requested-address"},
	{51,	__32,	"dhcp-lease-time"},
	{52,	__8,	"dhcp-option-overload"},
	{53,	__8,	"dhcp-message-type"},
	{54,	__IP,	"dhcp-server-identifier"},
	{55,	__8,	"dhcp-parameter-request-list"},
	{56,	__TXT,	"dhcp-message"},
	{57,	__16,	"dhcp-max-message-size"},
	{58,	__32,	"dhcp-renewal-time"},
	{59,	__32,	"dhcp-rebinding-time"},
	{60,	__TXT,	"vendor-class-identifier"},
	{61,	__TXT,	"dhcp-client-identifier"},
	{64,	__TXT,	"nisplus-domain"},
	{65,	__IP,	"nisplus-servers"},
	{66,	__TXT,	"tftp-server-name"},
	{67,	__TXT,	"bootfile-name"},
	{68,	__IP,	"mobile-ip-home-agent"},
	{69,	__IP,	"smtp-server"},
	{70,	__IP,	"pop-server"},
	{71,	__IP,	"nntp-server"},
	{72,	__IP,	"www-server"},
	{73,	__IP,	"finger-server"},
	{74,	__IP,	"irc-server"},
	{75,	__IP,	"streettalk-server"},
	{76,	__IP,	"streettalk-directory-assistance-server"},
	{77,	__TXT,	"user-class"},
	{85,	__IP,	"nds-servers"},
	{86,	__TXT,	"nds-tree-name"},
	{87,	__TXT,	"nds-context"},
	{210,	__TXT,	"authenticate"},

	/* use the following entries for arbitrary variables */
	{246,	__ILIST, ""},
	{247,	__ILIST, ""},
	{248,	__ILIST, ""},
	{249,	__ILIST, ""},
	{250,	__INDIR, ""},
	{251,	__INDIR, ""},
	{252,	__INDIR, ""},
	{253,	__INDIR, ""},
	{254,	__INDIR, ""},
	{0,	__TXT,	"%soption-%d"}
};

/*
 * parse a dhcp response, set environment variables translating options
 * names and values according to the tables above. Also set dhcp.tags
 * to the list of selected tags.
 */
static void
setenv_(u_char *cp,  u_char *ep, struct dhcp_opt *opts)
{
    u_char	*ncp;
    u_char	tag;
    char	tags[512], *tp;	/* the list of tags */

#define FLD_SEP	','	/* separator in list of elements */
    ncp = cp;
    tp = tags;
    if (opts == NULL)
	opts = dhcp_opt;

    while (ncp < ep) {
	unsigned int	size;		/* option size */
	char *vp, *endv, buf[256];	/* the value buffer */
	struct dhcp_opt *op;

	tag = *ncp++;			/* extract tag and size */
	size = *ncp++;
	cp = ncp;			/* current payload */
	ncp += size;			/* point to the next option */

	if (tag == TAG_END)
	    break;
	if (tag == 0)
	    continue;

	for (op = opts+1; op->tag && op->tag != tag; op++)
		;
	/* if not found we end up on the default entry */

	/*
	 * Copy data into the buffer. While the code uses snprintf, it's also
	 * careful never to insert strings that would be truncated. inet_ntoa is
	 * tricky to know the size, so it assumes we can always insert it
	 * because we reserve 16 bytes at the end of the string for its worst
	 * case. Other cases are covered because they will write fewer than
	 * these reserved bytes at the end. Source strings can't overflow (as
	 * noted below) because buf is 256 bytes and all strings are limited by
	 * the protocol to be 256 bytes or smaller.
	 */
	vp = buf;
	*vp = '\0';
	endv = buf + sizeof(buf) - 1 - 16;	/* last valid write position */

	switch(op->fmt) {
	case __NONE:
	    break;	/* should not happen */

	case __VE: /* recurse, vendor specific */
	    setenv_(cp, cp+size, vndr_opt);
	    break;

	case __IP:	/* ip address */
	    for (; size > 0 && vp < endv; size -= 4, cp += 4) {
		struct	in_addr in_ip;		/* ip addresses */
		if (vp != buf)
		    *vp++ = FLD_SEP;
		bcopy(cp, &in_ip.s_addr, sizeof(in_ip.s_addr));
		snprintf(vp, endv - vp, "%s", inet_ntoa(in_ip));
		vp += strlen(vp);
	    }
	    break;

	case __BYTES:	/* opaque byte string */
	    for (; size > 0 && vp < endv; size -= 1, cp += 1) {
		snprintf(vp, endv - vp, "%02x", *cp);
		vp += strlen(vp);
	    }
	    break;

	case __TXT:
	    bcopy(cp, buf, size);	/* cannot overflow */
	    buf[size] = 0;
	    break;

	case __32:
	case __16:
	case __8:	/* op->fmt is also the length of each field */
	    for (; size > 0 && vp < endv; size -= op->fmt, cp += op->fmt) {
		uint32_t v;
		if (op->fmt == __32)
			v = (cp[0]<<24) + (cp[1]<<16) + (cp[2]<<8) + cp[3];
		else if (op->fmt == __16)
			v = (cp[0]<<8) + cp[1];
		else
			v = cp[0];
		if (vp != buf)
		    *vp++ = FLD_SEP;
		snprintf(vp, endv - vp, "%u", v);
		vp += strlen(vp);
	    }
	    break;

	case __INDIR:	/* name=value */
	case __ILIST:	/* name=value;name=value... */
	    bcopy(cp, buf, size);	/* cannot overflow */
	    buf[size] = '\0';
	    for (endv = buf; endv; endv = vp) {
		char *s = NULL;	/* semicolon ? */

		/* skip leading whitespace */
		while (*endv && strchr(" \t\n\r", *endv))
		    endv++;
		vp = strchr(endv, '=');	/* find name=value separator */
		if (!vp)
		    break;
		*vp++ = 0;
		if (op->fmt == __ILIST && (s = strchr(vp, ';')))
		    *s++ = '\0';
		setenv(endv, vp, 1);
		vp = s;	/* prepare for next round */
	    }
	    buf[0] = '\0';	/* option already done */
	    break;
	}

	if (tp - tags < sizeof(tags) - 5) {	/* add tag to the list */
	    if (tp != tags)
		*tp++ = FLD_SEP;
	    snprintf(tp, sizeof(tags) - (tp - tags), "%d", tag);
	    tp += strlen(tp);
	}
	if (buf[0]) {
	    char	env[128];	/* the string name */

	    if (op->tag == 0)
		snprintf(env, sizeof(env), op->desc, opts[0].desc, tag);
	    else
		snprintf(env, sizeof(env), "%s%s", opts[0].desc, op->desc);
	    /*
	     * Do not replace existing values in the environment, so that
	     * locally-obtained values can override server-provided values.
	     */
	    setenv(env, buf, 0);
	}
    }
    if (tp != tags) {
	char	env[128];	/* the string name */
	snprintf(env, sizeof(env), "%stags", opts[0].desc);
	setenv(env, tags, 1);
    }
}
#endif /* additional dhcp */
