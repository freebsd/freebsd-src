/**************************************************************************
NETBOOT -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters
  Date: Dec/93

**************************************************************************/

#include <sys/types.h>
#include <sys/reboot.h>
#include <a.out.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <nfs/nfsv2.h>
#include <nfs/nfsdiskless.h>
#include <machine/bootinfo.h>
#include <machine/cpufunc.h>

#define ESC		0x1B

#ifndef DEFAULT_BOOTFILE
#define DEFAULT_BOOTFILE	"/kernel"
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

#ifndef MAX_RPC_RETRIES
#define MAX_RPC_RETRIES		20
#endif

#ifndef TIMEOUT			/* Inter-packet retry in ticks 18/sec */
#define TIMEOUT			20
#endif

#ifndef NULL
#define NULL	((void *)0)
#endif

#define TRUE		1
#define FALSE		0

#define VENDOR_NONE	0
#define VENDOR_WD	1
#define VENDOR_NOVELL	2
#define VENDOR_3COM	3
#define VENDOR_3C509	4

#define FLAG_PIO	0x01
#define FLAG_16BIT	0x02
#define FLAG_790	0x04

#define ARP_CLIENT	0
#define ARP_SERVER	1
#define ARP_GATEWAY	2
#define ARP_NS		3
#define ARP_ROOTSERVER	4
#define ARP_SWAPSERVER	5
#define MAX_ARP		ARP_SWAPSERVER+1

#define IP		0x0800
#define ARP		0x0806

#define BOOTP_SERVER	67
#define BOOTP_CLIENT	68
#define TFTP		69
#define SUNRPC		111

#define RPC_SOCKET	620			/* Arbitrary */

#define IP_UDP		17
#define IP_BROADCAST	0xFFFFFFFF

#define ARP_REQUEST	1
#define ARP_REPLY	2

#define BOOTP_REQUEST	1
#define BOOTP_REPLY	2

#define TAG_LEN(p)		(*((p)+1))
#define RFC1048_COOKIE		{ 99, 130, 83, 99 }
#define RFC1048_PAD		0
#define RFC1048_NETMASK		1
#define RFC1048_TIME_OFFSET	2
#define RFC1048_GATEWAY		3
#define RFC1048_TIME_SERVER	4
#define RFC1048_NAME_SERVER	5
#define RFC1048_DOMAIN_SERVER	6
#define RFC1048_HOSTNAME	12
#define RFC1048_BOOT_SIZE	12	/* XXX */
#define RFC1048_SWAP_SERVER	16
#define RFC1048_ROOT_PATH	17
#define RFC1048_SWAP_PATH	128	/* T128 */
#define RFC1048_SWAP_LEN	129	/* T129 */

#define RFC1048_END		255
#define BOOTP_VENDOR_LEN	256	/* Extended vendor field */

#define BOOTP_MIN_LEN		300	/* Minimum size of bootp udp packet */

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

#define PROG_PORTMAP	100000
#define PROG_NFS	100003
#define PROG_MOUNT	100005

#define MSG_CALL	0
#define MSG_REPLY	1

#define PORTMAP_LOOKUP	3

#define MOUNT_ADDENTRY	1
#define NFS_LOOKUP	4
#define NFS_READ	6

#define NFS_READ_SIZE	1024


#define AWAIT_ARP	0
#define AWAIT_BOOTP	1
#define AWAIT_TFTP	2
#define AWAIT_RPC	3

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
	char bp_vend[BOOTP_VENDOR_LEN];
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

struct rpc_t {
	struct iphdr	ip;
	struct udphdr	udp;
	union {
		char data[1400];
		struct {
			long id;
			long type;
			long rstatus;
			long verifier;
			long v2;
			long astatus;
			long data[1];
		} reply;
	} u;
};

#define TFTP_MIN_PACKET_SIZE	(sizeof(struct iphdr) + sizeof(struct udphdr) + 4)

/***************************************************************************
RPC Functions
***************************************************************************/
#define PUTLONG(val) {\
	register int lval = val; \
	*(rpcptr++) = ((lval) >> 24); \
	*(rpcptr++) = ((lval) >> 16); \
	*(rpcptr++) = ((lval) >> 8); \
	*(rpcptr++) = (lval); \
	rpclen+=4; }

char *sprintf();
