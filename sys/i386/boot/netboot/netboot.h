
/**************************************************************************
NETBOOT -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters
  Date: Dec/93

**************************************************************************/

#include <sys/types.h>
#include <a.out.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <net/if.h>
#include <netinet/in.h>
#include <nfs/nfsv2.h>
#include <nfs/nfsdiskless.h>
#include <machine/bootinfo.h>

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

#define ETHER_ADDR_SIZE		6	/* Size of Ethernet address */
#define ETHER_HDR_SIZE		14	/* Size of ethernet header */

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
#define RFC1048_GATEWAY		3
#define RFC1048_HOSTNAME	12
#define RFC1048_END		255
#define BOOTP_VENDOR_LEN	64

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

/*
static inline unsigned short inw(a)
	unsigned short a;
{
	unsigned char d;
	asm volatile( "inw %1, %0" : "=a" (d) : "d" (a));
	return d;
}

static inline unsigned char inb(a)
	unsigned short a;
{
	unsigned char d;
	asm volatile( "inb %1, %0" : "=a" (d) : "d" (a));
	return d;
}

static inline void outw(a,d)
	unsigned short a;
	unsigned short d;
{
	asm volatile( "outw %0, %1" : : "a" (d), "d" (a));
}

static inline void outb(a,d)
	unsigned short a;
	unsigned char d;
{
	asm volatile( "outb %0, %1" : : "a" (d), "d" (a));
}
*/

#if __GNUC__ < 2

#define	inb(port)		inbv(port)
#define	outb(port, data)	outbv(port, data)

#else /* __GNUC >= 2 */

/*
 * Use an expression-statement instead of a conditional expression
 * because gcc-2.6.0 would promote the operands of the conditional
 * and produce poor code for "if ((inb(var) & const1) == const2)".
 */
#define	inb(port)	({						\
	u_char	_data;							\
	if (__builtin_constant_p((int) (port)) && (port) < 256ul)	\
		_data = inbc(port);					\
	else								\
		_data = inbv(port);					\
	_data; })

#define	outb(port, data) \
	(__builtin_constant_p((int) (port)) && (port) < 256ul \
	 ? outbc(port, data) : outbv(port, data))

static __inline u_char
inbc(u_int port)
{
	u_char	data;

	__asm __volatile("inb %1,%0" : "=a" (data) : "i" (port));
	return (data);
}

static __inline void
outbc(u_int port, u_char data)
{
	__asm __volatile("outb %0,%1" : : "a" (data), "i" (port));
}

#endif /* __GNUC <= 2 */

static __inline u_char
inbv(u_int port)
{
	u_char	data;
	/*
	 * We use %%dx and not %1 here because i/o is done at %dx and not at
	 * %edx, while gcc generates inferior code (movw instead of movl)
	 * if we tell it to load (u_short) port.
	 */
	__asm __volatile("inb %%dx,%0" : "=a" (data) : "d" (port));
	return (data);
}

static __inline u_long
inl(u_int port)
{
	u_long	data;

	__asm __volatile("inl %%dx,%0" : "=a" (data) : "d" (port));
	return (data);
}

static __inline void
insb(u_int port, void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; insb"
			 : : "d" (port), "D" (addr), "c" (cnt)
			 : "di", "cx", "memory");
}

static __inline void
insw(u_int port, void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; insw"
			 : : "d" (port), "D" (addr), "c" (cnt)
			 : "di", "cx", "memory");
}

static __inline void
insl(u_int port, void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; insl"
			 : : "d" (port), "D" (addr), "c" (cnt)
			 : "di", "cx", "memory");
}

static __inline u_short
inw(u_int port)
{
	u_short	data;

	__asm __volatile("inw %%dx,%0" : "=a" (data) : "d" (port));
	return (data);
}

static __inline void
outbv(u_int port, u_char data)
{
	u_char	al;
	/*
	 * Use an unnecessary assignment to help gcc's register allocator.
	 * This make a large difference for gcc-1.40 and a tiny difference
	 * for gcc-2.6.0.  For gcc-1.40, al had to be ``asm("ax")'' for
	 * best results.  gcc-2.6.0 can't handle this.
	 */
	al = data;
	__asm __volatile("outb %0,%%dx" : : "a" (al), "d" (port));
}

static __inline void
outl(u_int port, u_long data)
{
	/*
	 * outl() and outw() aren't used much so we haven't looked at
	 * possible micro-optimizations such as the unnecessary
	 * assignment for them.
	 */
	__asm __volatile("outl %0,%%dx" : : "a" (data), "d" (port));
}

static __inline void
outsb(u_int port, void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; outsb"
			 : : "d" (port), "S" (addr), "c" (cnt)
			 : "si", "cx");
}

static __inline void
outsw(u_int port, void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; outsw"
			 : : "d" (port), "S" (addr), "c" (cnt)
			 : "si", "cx");
}

static __inline void
outsl(u_int port, void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; outsl"
			 : : "d" (port), "S" (addr), "c" (cnt)
			 : "si", "cx");
}

static __inline void
outw(u_int port, u_short data)
{
	__asm __volatile("outw %0,%%dx" : : "a" (data), "d" (port));
}

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
