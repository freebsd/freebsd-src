
/**************************************************************************
NETBOOT -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters
  Date: Dec/93

**************************************************************************/

#include <a.out.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <net/if.h>
#include <nfs/nfsv2.h>
#include <nfs/nfsdiskless.h>

#ifndef DEFAULT_BOOTFILE
#define DEFAULT_BOOTFILE	"386bsd"
#endif

#ifndef MAX_TFTP_RETRIES
#define MAX_TFTP_RETRIES	20
#endif

#ifndef MAX_BOOTP_RETRIES
#define MAX_BOOTP_RETRIES	20
#endif

#ifndef MAX_ARP_RETRIES
#define MAX_ARP_RETRIES		20
#endif

#ifndef TIMEOUT			/* Inter-packet retry in ticks 18/sec */
#define TIMEOUT			20
#endif

#ifndef NULL
#define NULL	((void *)0)
#endif

#define ETHER_ADDR_SIZE		6	/* Size of Ethernet address */
#define ETHER_HDR_SIZE		14	/* Size of ethernet header */

#define ARP_CLIENT	0
#define ARP_SERVER	1
#define ARP_GATEWAY	2
#define ARP_NS		3
#define MAX_ARP		ARP_NS+1

#define IP		0x0800
#define ARP		0x0806

#define BOOTP_SERVER	67
#define BOOTP_CLIENT	68
#define TFTP		69

#define IP_UDP		17
#define IP_BROADCAST	0xFFFFFFFF

#define ARP_REQUEST	1
#define ARP_REPLY	2

#define BOOTP_REQUEST	1
#define BOOTP_REPLY	2

#define TFTP_RRQ	1
#define TFTP_WRQ	2
#define TFTP_DATA	3
#define TFTP_ACK	4
#define TFTP_ERROR	5

#define TFTP_CODE_EOF	1
#define TFTP_CODE_MORE	2
#define TFTP_CODE_ERROR	3
#define TFTP_CODE_BOOT	4
#define TFTP_CODE_CFG	5

struct arptable_t {
	unsigned long ipaddr;
	unsigned char node[6];
} arptable[MAX_ARP];

struct arprequest {
	unsigned short hwtype;
	unsigned short protocol;
	char hwlen;
	char protolen;
	unsigned short opcode;
	char shwaddr[6];
	char sipaddr[4];
	char thwaddr[6];
	char tipaddr[4];
};

struct iphdr {
	char verhdrlen;
	char service;
	unsigned short len;
	unsigned short ident;
	unsigned short frags;
	char ttl;
	char protocol;
	unsigned short chksum;
	char src[4];
	char dest[4];
};

struct udphdr {
	unsigned short src;
	unsigned short dest;
	unsigned short len;
	unsigned short chksum;
};

struct bootp_t {
	struct iphdr ip;
	struct udphdr udp;
	char bp_op;
	char bp_htype;
	char bp_hlen;
	char bp_hops;
	unsigned long bp_xid;
	unsigned short bp_secs;
	unsigned short unused;
	char bp_ciaddr[4];
	char bp_yiaddr[4];
	char bp_siaddr[4];
	char bp_giaddr[4];
	char bp_hwaddr[16];
	char bp_sname[64];
	char bp_file[128];
	char bp_vend[64];
};

struct tftp_t {
	struct iphdr ip;
	struct udphdr udp;
	unsigned short opcode;
	union {
		char rrq[512];
		struct {
			unsigned short block;
			char download[512];
		} data;
		struct {
			unsigned short block;
		} ack;
		struct {
			unsigned short errcode;
			char errmsg[512];
		} err;
	} u;
};
#define TFTP_MIN_PACKET_SIZE	(sizeof(struct iphdr) + sizeof(struct udphdr) + 4)

#ifdef oldstuff
static inline unsigned short htons(unsigned short in)
{
	return((in >> 8) | (in << 8));
}
#define ntohs htons

static inline unsigned long htonl(unsigned long in)
{
	return((in >> 24) | ((in >> 16) & 0x0000FF00) |
		((in << 16) & 0x00FF0000) | (in << 24));
}
#define ntohl htonl
#endif

static inline unsigned char inb(a)
	unsigned short a;
{
	unsigned char d;
	asm volatile( "inb %1, %0" : "=a" (d) : "d" (a));
	return d;
}

static inline void outb(a,d)
	unsigned short a;
	unsigned char d;
{
	asm volatile( "outb %0, %1" : : "a" (d), "d" (a));
}
