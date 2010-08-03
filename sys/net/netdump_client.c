/*-
 * Copyright (c) 2005 Sandvine Incorporated. All rights reserved.
 * Copyright (c) 2000 Darrell Anderson <anderson@cs.duke.edu>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * netdump_client.c
 * FreeBSD kernel module supporting netdump network dumps.
 * netdump_server must be running to accept client dumps.
 * XXX: This should be split into machdep and non-machdep parts
 *
*/

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_device_polling.h"
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/reboot.h>
#include <sys/eventhandler.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/kerneldump.h>
#include <sys/smp.h>
#include <net/if.h>
#include <net/route.h>
#include <net/ethernet.h>
#include <net/if_var.h>
#include <net/if_vlan_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/in_cksum.h>
#include <machine/smp.h>
#include <machine/elf.h>
#include <machine/md_var.h>
#include <machine/_inttypes.h>
#include <net/if_media.h>
#include <net/if_mib.h>
#include <machine/clock.h>

#include <ddb/ddb.h>

#include <netinet/netdump.h>
#include "opt_netdump.h"
#include "opt_ddb.h"

#ifdef NETDUMP_DEBUG
#define	NETDDEBUG(f, ...)		printf((f), ## __VA_ARGS__)
#define	NETDDEBUG_IF(i, f, ...)		if_printf((i), (f), ## __VA_ARGS__)
#if NETDUMP_DEBUG > 1
#define	NETDDEBUGV(f, ...)		printf((f), ## __VA_ARGS__)
#define	NETDDEBUGV_IF(i, f, ...)	if_printf((i), (f), ## __VA_ARGS__)
#else
#define	NETDDEBUGV(f, ...)
#define	NETDDEBUGV_IF(i, f, ...)
#endif
#else
#define	NETDDEBUG(f, ...)
#define	NETDDEBUG_IF(i, f, ...)
#define	NETDDEBUGV(f, ...)
#define	NETDDEBUGV_IF(i, f, ...)
#endif

#define NETDUMP_PORT 20023      /* server udp port number for data */
#define NETDUMP_ACKPORT 20024   /* client udp port number for acks */

#define NETDUMP_DATASIZE 8192   /* how big to let the packets be */

#define NETDUMP_HERALD 1        /* broadcast before starting a dump */
#define NETDUMP_FINISHED 2      /* send after finishing a dump */
#define NETDUMP_VMCORE 3        /* packet contains dump data */
#define NETDUMP_KDH 4           /* packet contains kernel dump header */

struct netdump_msg_hdr {
	u_int32_t type;		/* NETDUMP_HERALD, _FINISHED, _VMCORE or _KDH */
	u_int32_t seqno;	/* match acks with msgs */
	u_int64_t offset;	/* vmcore offset, in bytes */
	u_int32_t len;		/* attached data, in bytes */
	u_int8_t pad[4];	/* Pad for parifying 32 and 64 bits */
};

struct netdump_msg {
	struct netdump_msg_hdr hdr;
	unsigned char data[NETDUMP_DATASIZE];/* real message may contain less */
};

struct netdump_ack {
	u_int32_t seqno;	/* match acks with msgs */
};

extern struct pcb dumppcb; /* cheat.  dumppcb is a static! */

/* ---------------------------------------------------------------- */
/*
 * private globals.  don't touch.
 */
static eventhandler_tag nd_tag = NULL;       /* record of our shutdown event */
static uint32_t nd_seqno = 1;		     /* current sequence number */
static uint64_t rcvd_acks;		     /* flags for out of order acks */
static int dump_failed, have_server_mac;
static uint16_t nd_server_port = NETDUMP_PORT; /* port to respond on */
static unsigned char buf[MAXDUMPPGS*PAGE_SIZE]; /* Must be at least as big as
						 * the chunks dumpsys() gives
						 * us */
static struct ether_addr nd_server_mac;

#define NETDUMP_BROKEN_STATE_BUFFER_SIZE (5 * sizeof(struct mtx))

/* ---------------------------------------------------------------- */
/*
 * helpers
 */

/*
 * [netdump_supported_nic]
 *
 * Checks for netdump support on a network interface
 *
 * Parameters:
 *	ifn	The network interface that is being tested for support
 *
 * Returns:
 *	int	1 if the interface is supported, 0 if not
 */
static int
netdump_supported_nic(struct ifnet *ifn)
{
	return ifn->if_netdump != NULL;
}


/* ---------------------------------------------------------------- */
/*
 * sysctl pokables.
 */

static int nd_active = 0;
static int nd_enable = 0;  /* if we should perform a network dump */
static struct in_addr nd_server = {INADDR_ANY}; /* server address */
static struct in_addr nd_client = {INADDR_ANY}; /* client (our) address */
struct ifnet *nd_nic = NULL;
static int nd_force_crash=0;
static int nd_polls=10000; /* Times to poll the NIC (0.5ms each poll) before
			    * assuming packetloss occurred: 5s by default */
static int nd_retries=10; /* Times to retransmit lost packets */

/*
 * [sysctl_ip]
 *
 * sysctl handler to deal with converting a string sysctl to/from an in_addr
 *
 * Parameters:
 *	SYSCTL_HANDLER_ARGS
 *	 - arg1 is a pointer to the struct in_addr holding the IP
 *	 - arg2 is unused
 *
 * Returns:
 *	int	see errno.h, 0 for success
 */
static int
sysctl_ip(SYSCTL_HANDLER_ARGS)
{
	struct in_addr addr;
	char buf[INET_ADDRSTRLEN];
	int error;
	int len=req->newlen - req->newidx;

	inet_ntoa_r(*(struct in_addr *)arg1, buf);
	error = SYSCTL_OUT(req, buf, strlen(buf)+1);

	if (error || !req->newptr)
		return error;

	error=0;

	if (len >= INET_ADDRSTRLEN) {
		error = EINVAL;
	} else {
		error = SYSCTL_IN(req, buf, len);
		buf[len]='\0';
		if (error)
			return error;
		if (!inet_aton(buf, &addr))
			return EINVAL;
		*(struct in_addr *)arg1 = addr;
	}

	return error;
}

/*
 * [sysctl_nic]
 *
 * sysctl handler to deal with converting a string sysctl to/from an interface
 *
 * Parameters:
 *	SYSCTL_HANDLER_ARGS
 *	 - arg1 is a pointer to the struct ifnet * to the interface
 *	 - arg2 is the maximum string length (IFNAMSIZ)
 *
 * Returns:
 *	int	see errno.h, 0 for success
 */
static int
sysctl_nic(SYSCTL_HANDLER_ARGS)
{
	struct ifnet *ifn;
	char buf[arg2+1];
	int error;
	int len;

	if (*(struct ifnet **)arg1) {
		error = SYSCTL_OUT(req,
				(*(struct ifnet **)arg1)->if_xname,
				strlen((*(struct ifnet **)arg1)->if_xname));
	} else {
		error = SYSCTL_OUT(req, "none", 5);
	}

	if (error || !req->newptr)
		return error;

	error=0;
	len = req->newlen - req->newidx;
	if (len >= arg2) {
		error = EINVAL;
	} else {
		error = SYSCTL_IN(req, buf, len);
		buf[len]='\0';
		if (error)
			return error;

		if (!strcmp(buf, "none")) {
			ifn = NULL;
		} else {
			if ((ifn = TAILQ_FIRST(&ifnet)) != NULL) do {
				if (!strcmp(ifn->if_xname, buf)) break;
			} while ((ifn = TAILQ_NEXT(ifn, if_link)) != NULL);

			if (!ifn) return ENODEV;
			if (!netdump_supported_nic(ifn)) return EINVAL;
		}

		(*(struct ifnet **)arg1) = ifn;
	}

	return error;
}

/* From the watchdog code and modified */
static int
sysctl_force_crash(SYSCTL_HANDLER_ARGS) 
{
	int error;
	error = sysctl_handle_int(oidp, &nd_force_crash, 
				  sizeof(nd_force_crash), req);
	if (error)
	    return error;

	switch (nd_force_crash) {
		case 1:
			printf("\nCrashing system...\n");
			for (;;);
			break;
		case 2:
			printf("\nPanic'ing system...\n");
			panic("netdump forced crash");
			break;
		case 4:
			printf("\nStarting netdump then spinning "
				"(to test watchdog trigger)\n");
			// nd_force_crash == 4 is checked in netdump_dumper
		case 3:
			printf("\nDeadlocking system while holding the em lock\n");
			{
				nd_nic->if_netdump->test_get_lock(nd_nic);
				for (;;);
			}
			break;
		case 5:
			critical_enter();
			panic("netdump forced crash in critical section");
			break;
		case 6:
			critical_enter();
			printf("\nNetdump spinning in a critical section\n");
			for (;;);
		default:
			return EINVAL;
	}
	return 0;
}

SYSCTL_NODE(_net, OID_AUTO, dump, CTLFLAG_RW, 0, "netdump");
SYSCTL_INT(_net_dump, OID_AUTO, enable, CTLTYPE_INT|CTLFLAG_RW, &nd_enable, 0,
	"enable network dump");
SYSCTL_PROC(_net_dump, OID_AUTO, server, CTLTYPE_STRING|CTLFLAG_RW, &nd_server,
	0, sysctl_ip, "A", "dump server");
SYSCTL_PROC(_net_dump, OID_AUTO, client, CTLTYPE_STRING|CTLFLAG_RW, &nd_client,
	0, sysctl_ip, "A", "dump client");
SYSCTL_PROC(_net_dump, OID_AUTO, nic, CTLTYPE_STRING|CTLFLAG_RW, &nd_nic,
	IFNAMSIZ, sysctl_nic, "A", "NIC to dump on");
SYSCTL_PROC(_net_dump, OID_AUTO, crash, CTLTYPE_INT|CTLFLAG_RW,
	0, sizeof(nd_force_crash), sysctl_force_crash, "I", "force crashing");
SYSCTL_INT(_net_dump, OID_AUTO, polls, CTLTYPE_INT|CTLFLAG_RW, &nd_polls, 0,
	"times to poll NIC per retry");
SYSCTL_INT(_net_dump, OID_AUTO, retries, CTLTYPE_INT|CTLFLAG_RW, &nd_retries, 0,
	"times to retransmit lost packets");

/* ---------------------------------------------------------------- */

/*
 * [netdump_ether_output]
 *
 * Handles creation of the ethernet header, then places outgoing packets into
 * the tx buffer for the NIC
 *
 * Parameters:
 *	m	The mbuf containing the packet to be sent (will be freed by
 *		this function or the NIC driver)
 *	ifp	The interface to send on
 *	dst	The destination ethernet address (source address will be looked
 *		up using ifp)
 *	etype	The ETHERTYPE_* value for the protocol that is being sent
 *
 * Returns:
 *	int	see errno.h, 0 for success
 */
static int
netdump_ether_output(struct mbuf *m, struct ifnet *ifp, struct ether_addr dst,
		     u_short etype)
{
	struct ether_header *eh;

	/* fill in the ethernet header */
	M_PREPEND(m, ETHER_HDR_LEN, M_DONTWAIT);
	if (m == 0) {
		printf("netdump_ether_output: Out of mbufs\n");
		return ENOBUFS;
	}
	eh = mtod(m, struct ether_header *);
	bcopy(IF_LLADDR(ifp), eh->ether_shost, ETHER_ADDR_LEN);
	bcopy(dst.octet, eh->ether_dhost, ETHER_ADDR_LEN);
	eh->ether_type = htons(etype);

	if (((ifp->if_flags & (IFF_MONITOR|IFF_UP)) != IFF_UP) ||
	    (ifp->if_drv_flags & IFF_DRV_RUNNING) != IFF_DRV_RUNNING) {
		if_printf(ifp, "netdump_ether_output: Interface isn't up\n");
		m_freem(m);
		return ENETDOWN;
	}

	if (_IF_QFULL(&ifp->if_snd)) {
		if_printf(ifp, "netdump_ether_output: TX queue full\n");
		m_freem(m);
		return ENOBUFS;
	}

	_IF_ENQUEUE(&ifp->if_snd, m);
	return 0;
}

/*
 * [netdump_udp_output]
 *
 * unreliable transmission of an mbuf chain to the netdump server
 * Note: can't handle fragmentation; fails if the packet is larger than
 *	 nd_nic->if_mtu after adding the UDP/IP headers
 *
 * Parameters:
 *	m	mbuf chain
 *
 * Returns:
 *	int	see errno.h, 0 for success
 */
static int
netdump_udp_output(struct mbuf *m)
{
	struct udpiphdr *ui;
	struct ip *ip;

	M_PREPEND(m, sizeof(struct udpiphdr), M_DONTWAIT);
	if (m == 0) {
		printf("netdump_udp_output: Out of mbufs\n");
		return ENOBUFS;
	}
	ui = mtod(m, struct udpiphdr *);
	bzero(ui->ui_x1, sizeof(ui->ui_x1));
	ui->ui_pr = IPPROTO_UDP;
	ui->ui_len = htons(m->m_pkthdr.len - sizeof(struct ip));
	ui->ui_ulen = ui->ui_len;
	ui->ui_src = nd_client;
	ui->ui_dst = nd_server;
	/* Use this src port so that the server can connect() the socket */
	ui->ui_sport = htons(NETDUMP_ACKPORT);
	ui->ui_dport = htons(nd_server_port);
	ui->ui_sum = 0;
	if ((ui->ui_sum = in_cksum(m, m->m_pkthdr.len)) == 0)
		ui->ui_sum = 0xffff;

	ip = mtod(m, struct ip *);
	ip->ip_v = IPVERSION;
	ip->ip_hl = sizeof(struct ip) >> 2;
	ip->ip_tos = 0;
	ip->ip_len = htons(m->m_pkthdr.len);
	ip->ip_id = 0;
	ip->ip_off = htons(IP_DF);
	ip->ip_ttl = 32;
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m, sizeof(struct ip));

	if (m->m_pkthdr.len > nd_nic->if_mtu) {
		/* Whoops. The packet is too big. */
		printf("netdump_udp_output: Packet is too big: "
		       "%u > MTU %lu\n", m->m_pkthdr.len, nd_nic->if_mtu);
		m_freem(m);
		return ENOBUFS;
	}

	return netdump_ether_output(m, nd_nic, nd_server_mac, ETHERTYPE_IP);
}

/* ---------------------------------------------------------------- */
/*
 * this section provides reliable message delivery (in the absence
 * of resource shortages).
 */

/*
 * [netdump_network_poll]
 *
 * after trapping, instead of assuming that most of the network stack is sane
 * just poll the driver directly for packets
 *
 * Parameters:
 *	void
 *
 * Returns:
 *	void
 */
static void
netdump_network_poll(void)
{
	nd_nic->if_netdump->poll_locked(nd_nic, POLL_AND_CHECK_STATUS, 1000);
}


/*
 * [netdump_mbuf_nop]
 *
 * netdump wraps external mbufs around address ranges.  unlike most sane
 * counterparts, netdump uses a stop-and-wait approach to flow control and
 * retransmission, so the ack obviates the need for mbuf reference
 * counting.  we still need to tell other mbuf handlers not to do anything
 * special with our mbufs, so specify this nop handler.
 *
 * Parameters:
 *	ptr	 data to free (ignored)
 *	opt_args callback pointer (ignored)
 *
 * Returns:
 *	void
 */
static void
netdump_mbuf_nop(void *ptr, void *opt_args)
{
	;
}

/*
 * [netdump_send]
 *
 * construct and reliably send a netdump packet.  may fail from a resource
 * shortage or extreme number of unacknowledged retransmissions.  wait for
 * an acknowledgement before returning.  splits packets into chunks small
 * enough to be sent without fragmentation (looks up the interface MTU)
 *
 * Parameters:
 *	type	netdump packet type (HERALD, FINISHED, or VMCORE)
 *	offset	vmcore data offset (bytes)
 *	data	vmcore data
 *	datalen	vmcore data size (bytes)
 *
 * Returns:
 *	int see errno.h, 0 for success
 */
static int
netdump_send(uint32_t type, off_t offset, 
	     unsigned char *data, uint32_t datalen)
{
	struct netdump_msg_hdr *nd_msg_hdr;
	struct mbuf *m, *m2;
	int retries = 0, polls, error;
	uint32_t i, sent_so_far;
	uint64_t want_acks=0;

	rcvd_acks = 0;

retransmit:
	/* We might get chunks too big to fit in packets. Yuck. */
	for (i=sent_so_far=0; sent_so_far < datalen || (i==0 && datalen==0);
		i++) {
		uint32_t pktlen = datalen-sent_so_far;
		/* First bound: the packet structure */
		pktlen = min(pktlen, NETDUMP_DATASIZE);
		/* Second bound: the interface MTU (assume no IP options) */
		pktlen = min(pktlen, nd_nic->if_mtu -
				sizeof(struct udpiphdr) -
				sizeof(struct netdump_msg_hdr));

		/* Check if we're retransmitting and this has been ACKed
		 * already */
		if ((rcvd_acks & (1 << i)) != 0) {
			sent_so_far += pktlen;
			continue;
		}

		/*
		 * get and fill a header mbuf, then chain data as an extended
		 * mbuf.
		 */
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			printf("netdump_send: Out of mbufs!\n");
			return ENOBUFS;
		}
		m->m_pkthdr.len = m->m_len = sizeof(struct netdump_msg_hdr);
		/* leave room for udpip */
		MH_ALIGN(m, sizeof(struct netdump_msg_hdr));
		nd_msg_hdr = mtod(m, struct netdump_msg_hdr *);
		nd_msg_hdr->seqno = htonl(nd_seqno+i);
		nd_msg_hdr->type = htonl(type);
		nd_msg_hdr->offset = htonll(offset+sent_so_far);
		nd_msg_hdr->len = htonl(pktlen);

		if (pktlen) {
			if ((m2 = m_get(M_DONTWAIT, MT_DATA)) == NULL) {
				m_freem(m);
				printf("netdump_send: Out of mbufs!\n");
				return ENOBUFS;
			}
			MEXTADD(m2, data+sent_so_far, pktlen, netdump_mbuf_nop,
				NULL, M_RDONLY, EXT_MOD_TYPE);
			m2->m_len = pktlen;
			m->m_next = m2;
			m->m_pkthdr.len += m2->m_len;
		}

		if ((error = netdump_udp_output(m)) != 0) {
			return error;
		}

		/* Note that we're waiting for this packet in the bitfield */
		want_acks |= 1 << i;

		sent_so_far += pktlen;
	}

	if (i >= sizeof(want_acks)*8) {
		printf("Warning: Sent more than %zd packets (%d). "
		       "Acknowledgements will fail unless the size of "
		       "rcvd_acks/want_acks is increased.\n",
		       sizeof(want_acks)*8, i);
	}

	/*
	 * wait for acks. a *real* window would speed things up considerably.
	 */
	polls = 0;
	while (rcvd_acks != want_acks) {		
		if (polls++ > nd_polls) {
			if (retries++ > nd_retries) {
				return ETIMEDOUT; /* 10 s, no ack */
			}
			printf(". ");
			goto retransmit; /* 1 s, no ack */
		}
		/*
		 * this is not always necessary, but does no harm.
		 */
		netdump_network_poll();
		DELAY(500); /* 0.5 ms */
	}
	nd_seqno += i;
	return 0;
}

/* ---------------------------------------------------------------- */

static void nd_handle_ip(struct mbuf **mb);
static void nd_handle_arp(struct mbuf **mb);
/*
 * [netdump_pkt_in]
 *
 * Handler for incoming packets directly from the network adapter
 * Identifies the packet type (IP or ARP) and passes it along to one of the
 * helper functions nd_handle_ip or nd_handle_arp.
 *
 * Parameters:
 *	ifp	the interface the packet came from (should be nd_nic)
 *	m	an mbuf containing the packet received
 *
 * Return value:
 *	void
 */
/* Bits from sys/net/if_ethersubr.c:ether_input,
   	     sys/net/if_ethersubr.c:ether_demux */
static void
netdump_pkt_in(struct ifnet *ifp, struct mbuf *m)
{
	struct ether_header *eh;
	u_short etype;

	/* Ethernet processing */

	NETDDEBUGV_IF("Processing packet...\n");

	if ((m->m_flags & M_PKTHDR) == 0) {
		NETDDEBUG_IF(ifp, "discard frame w/o packet header\n");
		goto done;
	}
	if (m->m_len < ETHER_HDR_LEN) {
		NETDDEBUG_IF(ifp, "discard frome w/o leading ethernet "
		    "header (len %u pkt len %u)\n", m->m_len, m->m_pkthdr.len);
		goto done;
	}
	if (m->m_flags & M_HASFCS) {
		m_adj(m, -ETHER_CRC_LEN);
		m->m_flags &= ~M_HASFCS;
	}
	eh = mtod(m, struct ether_header *);
	m->m_pkthdr.header = eh;
	etype = ntohs(eh->ether_type);
	if ((ifp->if_nvlans && m_tag_locate(m, MTAG_VLAN, MTAG_VLAN_TAG, NULL))
	    || etype == ETHERTYPE_VLAN) {
		NETDDEBUG_IF(ifp, "ignoring vlan packets\n");
		goto done;
	}
	/* XXX: Probably should check if we're the recipient MAC address */
	/* Done ethernet processing. Strip off the ethernet header */
	m_adj(m, ETHER_HDR_LEN);

	switch (etype) {
		case ETHERTYPE_ARP:
			nd_handle_arp(&m);
			break;
		case ETHERTYPE_IP:
			nd_handle_ip(&m);
			break;
		default:
			NETDDEBUG_IF(ifp, "dropping unknown ethertype %hu\n",
			    etype);
			break;
	}

done:
	if (m) m_freem(m);
}

/*
 * [nd_handle_ip]
 *
 * Handler for IP packets: checks their sanity and then processes any netdump
 * ACK packets it finds.
 *
 * Parameters:
 *	mb	a pointer to an mbuf * containing the packet received
 *		Updates *mb if m_pullup et al change the pointer
 *		Assumes the calling function will take care of freeing the mbuf
 *
 * Return value:
 *	void
 */
/* Bits from sys/net/if_ethersubr.c:ether_input,
   	     sys/net/if_ethersubr.c:ether_demux */

/* Bits from sys/netinet/ip_input.c:ip_input,
 	     sys/netinet/udp_usrreq.c:udp_input */
static void
nd_handle_ip(struct mbuf **mb)
{
	unsigned short hlen;
	struct ip *ip;
	struct udpiphdr *udp;
	struct netdump_ack *nd_ack;
	struct mbuf *m;
	int rcv_ackno;

	/* IP processing */

	NETDDEBUGV("nd_handle_ip: Processing IP packet...\n");

	m = *mb;
	if (m->m_pkthdr.len < sizeof(struct ip)) {
		NETDDEBUG("nd_handle_ip: dropping packet too small for IP "
		    "header\n");
		return;
	}
	if (m->m_len < sizeof(struct ip) &&
	    (*mb = m = m_pullup(m, sizeof(struct ip))) == NULL) {
		NETDDEBUG("nd_handle_ip: m_pullup failed\n");
		return;
	}

	ip = mtod(m, struct ip *);

	/* IP version */
	if (ip->ip_v != IPVERSION) {
		NETDDEBUG("nd_handle_ip: Bad IP version %d\n", ip->ip_v);
		return;
	}

	/* Header length */
	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(struct ip)) {
		NETDDEBUG("nd_handle_ip: Bad IP header length (%hu)\n", hlen);
		return;
	}
	if (hlen > m->m_len) {
		if ((*mb = m = m_pullup(m, hlen)) == NULL) {
			NETDDEBUG("nd_handle_ip: m_pullup failed\n");
			return;
		}
		ip = mtod(m, struct ip *);
	}

#ifdef INVARIANTS
	if (((ntohl(ip->ip_dst.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET ||
	    (ntohl(ip->ip_src.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET) &&
	    (m->m_pkthdr.rcvif->if_flags & IFF_LOOPBACK) == 0) {
		NETDDEBUG("nd_handle_ip: Bad IP header (RFC1122)\n");
		return;
	}
#endif

	/* Checksum */
	if (m->m_pkthdr.csum_flags & CSUM_IP_CHECKED) {
		if (!(m->m_pkthdr.csum_flags & CSUM_IP_VALID)) {
			NETDDEBUG("nd_handle_ip: Bad IP checksum\n");
			return;
		}
	} else
		NETDDEBUG("nd_handle_ip: HW didn't check IP cksum\n");

	/* Convert fields to host byte order */
	ip->ip_len = ntohs(ip->ip_len);
	if (ip->ip_len < hlen) {
		NETDDEBUG("nd_handle_ip: IP packet smaller (%hu) than "
		    "header (%hu)\n", ip->ip_len, hlen);
		return;
	}
	ip->ip_off = ntohs(ip->ip_off);

	if (m->m_pkthdr.len < ip->ip_len) {
		NETDDEBUG("nd_handle_ip: IP packet bigger (%hu) than "
		    "ethernet packet (%hu)\n", ip->ip_len, m->m_pkthdr.len);
		return;
	}
	if (m->m_pkthdr.len > ip->ip_len) {
		/* Truncate the packet to the IP length */
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = ip->ip_len;
			m->m_pkthdr.len = ip->ip_len;
		} else
			m_adj(m, ip->ip_len - m->m_pkthdr.len);
	}

	/* We would process IP options here, but we'll ignore them instead. */
	/* Strip IP options */
	if (hlen > sizeof(struct ip)) {
		ip_stripoptions(m, (struct mbuf *)0);
		hlen = sizeof(struct ip);
	}

	/* Check that the source is the server's IP */
	if (ip->ip_src.s_addr != nd_server.s_addr) {
		NETDDEBUG("nd_handle_ip: Drop packet not from server\n");
		return;
	}

	/* Check if the destination IP is ours */
	if (ip->ip_dst.s_addr != nd_client.s_addr) {
		NETDDEBUGV("nd_handle_ip: Drop packet not to our IP\n");
		return;
	}

	if (ip->ip_p != IPPROTO_UDP) {
		NETDDEBUG("nd_handle_ip: Drop non-UDP packet\n");
		return;
	}

	/* Let's not deal with fragments */
	if (ip->ip_off & (IP_MF | IP_OFFMASK)) {
		NETDDEBUG("nd_handle_ip: Drop fragmented packet\n");
		return;
	}
	/* UDP custom is to have packet length not include IP header */
	ip->ip_len -= hlen;

	/* IP done */
	/* UDP processing */

	/* Get IP and UDP headers together, along with the netdump packet */
	if (m->m_pkthdr.len <
	    sizeof(struct udpiphdr) + sizeof(struct netdump_ack)) {
		NETDDEBUG("nd_handle_ip: Ignoring small packet\n");
		return;
	}
	if (m->m_len < sizeof(struct udpiphdr) + sizeof(struct netdump_ack) &&
	    (*mb = m = m_pullup(m, sizeof(struct udpiphdr) +
	    sizeof(struct netdump_ack))) == NULL) {
		NETDDEBUG("nd_handle_ip: m_pullup failed\n");
		return;
	}
	udp = mtod(m, struct udpiphdr *);

	NETDDEBUG("nd_handle_ip: Processing packet...");

	if (ntohs(udp->ui_u.uh_dport) != NETDUMP_ACKPORT) {
		NETDDEBUG("not on the netdump port.\n");
		return;
	}

	/* UDP done */
	/* Netdump processing */

	/*
	 * packet is meant for us.  extract the ack sequence number.
	 * if it's the first ack, extract the port number as well
	 */
	nd_ack = (struct netdump_ack *)
		(mtod(m, caddr_t) + sizeof(struct udpiphdr));
	rcv_ackno = ntohl(nd_ack->seqno);

	if (nd_server_port == NETDUMP_PORT) {
	    nd_server_port = ntohs(udp->ui_u.uh_sport);
	}

	if (rcv_ackno >= nd_seqno+64) {
		printf("nd_handle_ip: ACK %d too far in future!\n", rcv_ackno);
	} else if (rcv_ackno < nd_seqno) {
		/* Do nothing: A duplicated past ACK */
	} else {
		/* We're interested in this ack. Record it. */
		rcvd_acks |= 1 << (rcv_ackno-nd_seqno);
	}
}

/*
 * [nd_handle_arp]
 *
 * Handler for ARP packets: checks their sanity and then
 * 1. If the ARP is a request for our IP, respond with our MAC address
 * 2. If the ARP is a response from our server, record its MAC address
 *
 * Parameters:
 *	mb	a pointer to an mbuf * containing the packet received
 *		Updates *mb if m_pullup et al change the pointer
 *		Assumes the calling function will take care of freeing the mbuf
 *
 * Return value:
 *	void
 */
/* Bits from sys/netinet/if_ether.c:arpintr,
	     sys/netinet/if_ether.c:in_arpinput */
static void
nd_handle_arp(struct mbuf **mb)
{
	struct mbuf *m;
	struct arphdr *ah;
	struct ifnet *ifp;
	int req_len, op;
	struct in_addr isaddr, itaddr, myaddr;
	uint8_t *enaddr;
	struct ether_addr dst;

	m = *mb;
	ifp = m->m_pkthdr.rcvif;
	if (m->m_len < sizeof(struct arphdr) && ((*mb = m = m_pullup(m,
	    sizeof(struct arphdr))) == NULL)) {
		NETDDEBUG("nd_handle_arp: runt packet: m_pullup failed\n");
		return;
	}
	ah = mtod(m, struct arphdr *);

	if (ntohs(ah->ar_hrd) != ARPHRD_ETHER) {
		NETDDEBUG("nd_handle_arp: unknown hardware address fmt "
		    "0x%2D)\n", (unsigned char *)&ah->ar_hrd, "");
		return;
	}

	if (ntohs(ah->ar_pro) != ETHERTYPE_IP) {
		NETDDEBUG("nd_handle_arp: Drop ARP for unknown "
		    "protocol %d\n", ntohs(ah->ar_pro));
		return;
	}

	req_len = arphdr_len2(ifp->if_addrlen, sizeof(struct in_addr));
	if (m->m_len < req_len && (*mb = m = m_pullup(m, req_len)) == NULL) {
		NETDDEBUG("nd_handle_arp: runt packet: m_pullup failed\n");
		return;
	}
	ah = mtod(m, struct arphdr *);

	op = ntohs(ah->ar_op);
	bcopy(ar_spa(ah), &isaddr, sizeof(isaddr));
	bcopy(ar_tpa(ah), &itaddr, sizeof(itaddr));
	enaddr = (uint8_t *)IF_LLADDR(ifp);
	myaddr = nd_client;

	if (!bcmp(ar_sha(ah), enaddr, ifp->if_addrlen)) {
		NETDDEBUG("nd_handle_arp: ignoring ARP from myself\n");
		return;
	}

	if (isaddr.s_addr == nd_client.s_addr) {
		printf("nd_handle_arp: %*D is using my IP address %s!\n",
				ifp->if_addrlen, (u_char *)ar_sha(ah), ":",
				inet_ntoa(isaddr));
		return;
	}

	if (!bcmp(ar_sha(ah), ifp->if_broadcastaddr, ifp->if_addrlen)) {
		NETDDEBUG("nd_handle_arp: ignoring ARP from broadcast "
		    "address\n");
		return;
	}

	if (op == ARPOP_REPLY) {
		if (isaddr.s_addr != nd_server.s_addr) {
			char buf[INET_ADDRSTRLEN];
			inet_ntoa_r(isaddr, buf);
			NETDDEBUG("nd_handle_arp: ignoring ARP reply from "
			    "%s (not netdump server)\n", buf);
			return;
		}
		bcopy(ar_sha(ah), nd_server_mac.octet,
				min(ah->ar_hln, ETHER_ADDR_LEN));
		have_server_mac = 1;
		NETDDEBUG("\nnd_handle_arp: Got server MAC address %6D\n",
		    nd_server_mac.octet, ":");
		return;
	}

	if (op != ARPOP_REQUEST) {
		NETDDEBUG("nd_handle_arp: Ignoring non-request/non-reply "
		    "ARP\n");
		return;
	}

	if (itaddr.s_addr != nd_client.s_addr) {
		NETDDEBUG("nd_handle_arp: ignoring ARP not to our IP\n");
		return;
	}

	bcopy(ar_sha(ah), ar_tha(ah), ah->ar_hln);
	bcopy(enaddr, ar_sha(ah), ah->ar_hln);
	bcopy(ar_spa(ah), ar_tpa(ah), ah->ar_pln);
	bcopy(&itaddr, ar_spa(ah), ah->ar_pln);
	ah->ar_op = htons(ARPOP_REPLY);
	ah->ar_pro = htons(ETHERTYPE_IP); /* let's be sure! */
	m->m_flags &= ~(M_BCAST|M_MCAST); /* never reply by broadcast */
	m->m_len = sizeof(*ah) + (2 * ah->ar_pln) + (2 * ah->ar_hln);
	m->m_pkthdr.len = m->m_len;

	bcopy(dst.octet, ar_tha(ah), ETHER_ADDR_LEN);
	netdump_ether_output(m, ifp, dst, ETHERTYPE_ARP);
	*mb = NULL; /* Don't m_free upon return */
}

/*
 * [netdump_dumper]
 *
 * Callback from dumpsys() to dump a chunk of memory
 * Copies it out to our static buffer then sends it across the network
 * Detects the initial KDH and makes sure it's given a special packet type
 *
 * Parameters:
 *	priv	 Unused. Optional private pointer.
 *	virtual  Virtual address (where to read the data from)
 *	physical Unused. Physical memory address.
 *	offset	 Offset from start of core file
 *	length	 Data length
 *
 * Return value:
 *	0 on success
 *	errno on error
 */
static int
netdump_dumper(void *priv, void *virtual, vm_offset_t physical, off_t offset,
		size_t length)
{
	int err;
	int msgtype = NETDUMP_VMCORE;

	(void)priv;

	NETDDEBUGV("netdump_dumper(%p, %p, %"PRIxPTR", %"PRIx64", %zu)\n",
	    priv, virtual, physical, (uint64_t)offset, length);

	if (length > sizeof(buf))
		return ENOSPC;
	/*
	 * The first write (at offset 0) is the kernel dump header.  Flag it
	 * for the server to treat specially.  XXX: This doesn't strip out the
	 * footer KDH, although it shouldn't hurt anything.
	 */
	if (offset == 0 && length > 0)
		msgtype = NETDUMP_KDH;
	else if (offset > 0)
		offset -= sizeof(struct kerneldumpheader);

	bcopy(virtual, buf, length);
	if (wdog_tickler)
		(*wdog_tickler)();
	err=netdump_send(msgtype, offset, buf, length);
	if (err) {
		dump_failed=1;
		return err;
	}
	
	/* To test the hardware watchdog for problems during dumps, spin here
	 * if required */
	if (nd_force_crash == 4)
		for(;;);

	return 0;
}

/* ---------------------------------------------------------------- */

/*
 * [netdump_send_arp]
 *
 * Builds and sends a single ARP request to locate the server
 *
 * Parameters:
 *	void
 *
 * Return value:
 *	0 on success
 *	errno on error
 */
static int
netdump_send_arp(void)
{
	struct mbuf *m;
	int pktlen = arphdr_len2(ETHER_ADDR_LEN, sizeof(struct in_addr));
	struct arphdr *ah;
	struct ether_addr bcast;

	ETHER_SET_BROADCAST(&bcast);

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		printf("netdump_send_arp: Out of mbufs");
		return ENOBUFS;
	}
	m->m_pkthdr.len = m->m_len = pktlen;
	MH_ALIGN(m, pktlen); /* Make room for ethernet header */
	ah = mtod(m, struct arphdr *);
	ah->ar_hrd = htons(ARPHRD_ETHER);
	ah->ar_pro = htons(ETHERTYPE_IP);
	ah->ar_hln = ETHER_ADDR_LEN;
	ah->ar_pln = sizeof(struct in_addr);
	ah->ar_op = htons(ARPOP_REQUEST);
	bcopy(IF_LLADDR(nd_nic), ar_sha(ah), ETHER_ADDR_LEN);
	((struct in_addr *)ar_spa(ah))->s_addr = nd_client.s_addr;
	bzero(ar_tha(ah), ETHER_ADDR_LEN);
	((struct in_addr *)ar_tpa(ah))->s_addr = nd_server.s_addr;

	return netdump_ether_output(m, nd_nic, bcast, ETHERTYPE_ARP);
}

/*
 * [netdump_arp_server]
 *
 * Sends ARP requests to locate the server and waits for a response
 *
 * Parameters:
 *	void
 *
 * Return value:
 *	0 on success
 *	errno on error
 */
static int
netdump_arp_server(void)
{
	int err, polls, retries;

	for (retries=0; retries < nd_retries && !have_server_mac; retries++) {
		err = netdump_send_arp();

		if (err)
			return err;

		for (polls=0; polls < nd_polls && !have_server_mac; polls++) {
			netdump_network_poll();
#ifdef HW_WDOG
		if (wdog_tickler)
			(*wdog_tickler)();
#endif
			DELAY(500); /* 0.5 ms */
		}

		if (!have_server_mac) printf("(ARP retry)");
	}

	if (have_server_mac)
		return 0;

	printf("\nARP timed out.\n");

	return ETIMEDOUT;
}

/*
 * [netdump_trigger]
 *
 * called from kern_shutdown during "boot" (invoked on panic).  perform a
 * network dump, and if successful cancel the normal disk dump.
 *
 * Parameters:
 *	arg	unused
 *	howto   boot flags (only dump if RB_DUMP set)
 *
 * Returns:
 *	void
 */
static void
netdump_trigger(void *arg, int howto)
{
	struct dumperinfo dumper;
	int broke_lock = 0;
	uint8_t broken_state[NETDUMP_BROKEN_STATE_BUFFER_SIZE];
	void (*old_if_input)(struct ifnet *, struct mbuf *)=NULL;
	int error;
#ifdef SMP
	u_int cpumap=0;
#endif
	
	if ((howto&(RB_HALT|RB_DUMP))!=RB_DUMP || !nd_enable || nd_active) {
		return;
	}
	nd_active = 1;

	if (!nd_nic) {
		printf("netdump_trigger: Can't netdump: no NIC given\n");
		nd_active = 0;
		return;
	}

	if (nd_server.s_addr == INADDR_ANY) {
		printf("netdump_trigger: Can't netdump; no server IP given\n");
		nd_active = 0;
		return;
	}
	if (nd_client.s_addr == INADDR_ANY) {
		printf("netdump_trigger: Can't netdump; no client IP given\n");
		nd_active = 0;
		return;
	}

	/*
	 * netdump is invoked as a shutdown handler instead of as
	 * a real dumpdev dump routine.  (networks are shut down
	 * before dumpsys() gets called.)  make sure the dump context
	 * is set so a debugger can find the stack trace.
	 */
	savectx(&dumppcb);

	/***** Beyond this point, don't return: goto abort *****/

	/* Stop all the other CPUs */
#ifdef SMP
	if (smp_active != 0) {
		printf("netdump_trigger called on cpu#%d\n", PCPU_GET(cpuid));

		cpumap = PCPU_GET(other_cpus) & ~ stopped_cpus;
		if (cpumap != 0) {
			unsigned long long end_ts;

			printf("netdump_trigger: Stopping other CPUs\n");

			/* 1 second */
			end_ts = rdtsc() + 1ULL * tsc_freq;

			ipi_selected(cpumap, IPI_STOP);

			while ((atomic_load_acq_int(&stopped_cpus) & cpumap)
			    != cpumap) {
				if (rdtsc() > end_ts) {
					printf("netdump_trigger: Stopping other"
					       "CPUs timed out. Continuing "
					       "anyway.\n");
					break;
				}
			}
		}
	}
#endif
	bzero(broken_state, sizeof(broken_state));
	error = nd_nic->if_netdump->break_lock(nd_nic, &broke_lock, broken_state, sizeof(broken_state));

	if(error) {
		printf("netdump_trigger: Could not acquire lock on %s\n", nd_nic->if_xname);
		nd_active = 0;
		return;
	}
	
	/* At this point, we should 'own' the driver lock */

	/* We don't want interrupts potentially messing with our dump process */
	critical_enter();

	/* Make the card use *our* receive callback */
	old_if_input = nd_nic->if_input;
	nd_nic->if_input = netdump_pkt_in;

	/* Check if we can use polling */
	if (!(nd_nic->if_capenable & IFCAP_POLLING)) {
		printf("netdump_trigger: Can't dump: interface %s does not have"
		       " polling enabled.\n", nd_nic->if_xname);
		goto abort;
	}

	printf("\n-----------------------------------\n");
	printf("netdump in progress. searching for server.. ");
	if (netdump_arp_server()) {
		printf("Failed to locate server MAC address\n");
		goto abort;
	}
	if (netdump_send(NETDUMP_HERALD, 0, NULL, 0) != 0) {
		printf("Failed to contact netdump server\n");
		goto abort;
	}
	printf("dumping to %s (%6D)\n", inet_ntoa(nd_server),
			nd_server_mac.octet, ":");
	printf("-----------------------------------\n");

	/*
	 * dump memory.
	 */
	dumper.dumper = netdump_dumper;
	dumper.priv = NULL;
	dumper.blocksize = NETDUMP_DATASIZE;
	dumper.flags = DF_NET;

	/* in dump_machdep.c */
	dumpsys(&dumper);

	if (dump_failed)
		goto abort;

	if (netdump_send(NETDUMP_FINISHED, 0, NULL, 0) != 0) {
		goto abort;
	}
	printf("\nnetdump finished.\n");
	printf("cancelling normal dump\n");
	set_dumper(NULL);
	goto cleanup;
abort:
	printf("\nnetdump failed, proceeding to normal dump\n");
cleanup:
	if (old_if_input)
		nd_nic->if_input = old_if_input;
	critical_exit();
	/* Even if we broke the lock, this seems like the most sane thing to
	 * do */
	nd_nic->if_netdump->release_lock(nd_nic);
#ifdef SMP
	if (cpumap) {
		restart_cpus(cpumap);
	}
#endif
	nd_active = 0;
}

/* this isn't declared in any header file... */
extern int system_panic;

int
netdump_break_lock(struct mtx *lock, const char *name, int *broke_lock,
    uint8_t *broken_state, u_int index, u_int bstatesz)
{
	/* XXX: Technically this might be bad because it's possible to be called
	   from within a critical section (such as when the software watchdog
	   triggers), although it's only a trylock */
	if (!mtx_trylock(lock)) {
		if(system_panic) {
			/* The lock is already held. Attempting to use the card is
			 * probably unsafe at this point, but since we're panicking
			 * anyway, there's nothing to lose. Be horrible and break the
			 * lock. 
			 */
			critical_enter(); /* No interrupts so that this is less likely
			                   * to mess up 
			                   */
			if(bstatesz >= (index + sizeof(*lock))) {
				bcopy(lock, broken_state + index, sizeof(*lock));
			} else {
				printf("Netdump: cannot save state of lock %s!", name);
			}

			_release_lock_quick(lock);
			mtx_destroy(lock);
			mtx_init(lock, name, MTX_NETWORK_LOCK,
				MTX_DEF);
			*broke_lock = 1;
			critical_exit();/* We can't grab a lock in a critical section */
			mtx_lock(lock);
		} else {
			return EAGAIN;
		}
	} 
	return 0;
}

/*------------------------------------------------*/
/*
 * module specific handling
 */

/*
 * [netdump_config_defaults]
 *
 * Called upon module load. Initializes the sysctl variables to sane defaults
 * (locates the first available NIC and uses the first IPv4 IP on that card as
 * the client IP). Leaves the server IP unconfigured.
 *
 * Parameters:
 *	void
 *
 * Returns:
 *	void
 */
static void
netdump_config_defaults(void)
{
	struct ifnet *ifn;
	struct ifaddr *ifa;

	nd_nic = NULL;
	nd_client.s_addr = INADDR_ANY;

	/* Default the nic to the first available interface */
	if ((ifn = TAILQ_FIRST(&ifnet)) != NULL) do {
		if ((ifn->if_flags & IFF_UP) == 0)
			continue;

		if (netdump_supported_nic(ifn) &&
		    (nd_nic == NULL || nd_nic->if_dunit < ifn->if_dunit))
			nd_nic = ifn;

	} while ((ifn = TAILQ_NEXT(ifn, if_link)) != NULL && nd_nic == NULL);

	if (nd_nic == NULL)
		return;

	/* Default the client to the first IP on nd_nic */
	if ((ifa = TAILQ_FIRST(&nd_nic->if_addrhead)) != NULL) do {
		if (ifa->ifa_addr->sa_family != AF_INET) {
			continue;
		}
		nd_client = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
	} while ((ifa = TAILQ_NEXT(ifa, ifa_link)) != NULL &&
			nd_client.s_addr == INADDR_ANY);
}

static int
netdump_modevent(module_t mod, int type, void *unused) 
{
	switch (type) {
	case MOD_LOAD:
		/* PRI_FIRST happens before the networks are disabled */
		nd_tag = EVENTHANDLER_REGISTER(shutdown_pre_sync, 
					       netdump_trigger, NULL, 
					       SHUTDOWN_PRI_FIRST);

		netdump_config_defaults();

#ifdef NETDUMP_DEBUG
		if (!nd_nic)
			printf("netdump: Warning: No default interface "
			    "found. Manual configuration required.\n");
		else {
			char buf[INET_ADDRSTRLEN];
			inet_ntoa_r(nd_client, buf);
			printf("netdump: Using interface %s; client IP "
			    "%s\n", nd_nic->if_xname, buf);
		}
#endif

		printf("netdump initialized\n");
		break;
	case MOD_UNLOAD:
		if (nd_tag) {
			EVENTHANDLER_DEREGISTER(shutdown_pre_sync, nd_tag);
			nd_tag = NULL;
		}
		printf("netdump unloaded\n");
		break;
	default:
		break;
	}
	return 0;
}
static moduledata_t netdump_mod = {"netdump", netdump_modevent, 0};
DECLARE_MODULE(netdump, netdump_mod, SI_SUB_PROTO_END, SI_ORDER_ANY);

#ifdef DDB
DB_COMMAND(netdump, ddb_force_netdump)
{
	if(nd_active) {
		db_printf("netdump in progress");
		return;
	}

	if(!nd_enable) {
		db_printf("netdump not enabled");
		return;
	}

	netdump_trigger(NULL, RB_DUMP);
}
#endif

