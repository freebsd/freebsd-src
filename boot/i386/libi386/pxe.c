/*-
 * Copyright (c) 2000 Alfred Perlstein <alfred@freebsd.org>
 * Copyright (c) 2000 Paul Saab <ps@freebsd.org>
 * Copyright (c) 2000 John Baldwin <jhb@freebsd.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <string.h>
#include <stdarg.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/udp.h>

#include <net.h>
#include <netif.h>
#include <nfsv2.h>
#include <iodesc.h>

#include <bootp.h>
#include <bootstrap.h>
#include "btxv86.h"
#include "pxe.h"

/*
 * Allocate the PXE buffers statically instead of sticking grimy fingers into
 * BTX's private data area.  The scratch buffer is used to send information to
 * the PXE BIOS, and the data buffer is used to receive data from the PXE BIOS.
 */
#define	PXE_BUFFER_SIZE		0x2000
#define	PXE_TFTP_BUFFER_SIZE	512
static char	scratch_buffer[PXE_BUFFER_SIZE];
static char	data_buffer[PXE_BUFFER_SIZE];

static pxenv_t	*pxenv_p = NULL;        /* PXENV+ */
static pxe_t	*pxe_p   = NULL;	/* !PXE */
static BOOTPLAYER	bootplayer;	/* PXE Cached information. */

static int 	pxe_debug = 0;
static int	pxe_sock = -1;
static int	pxe_opens = 0;

void		pxe_enable(void *pxeinfo);
static void	(*pxe_call)(int func);
static void	pxenv_call(int func);
static void	bangpxe_call(int func);

static int	pxe_init(void);
static int	pxe_strategy(void *devdata, int flag, daddr_t dblk,
			     size_t size, char *buf, size_t *rsize);
static int	pxe_open(struct open_file *f, ...);
static int	pxe_close(struct open_file *f);
static void	pxe_print(int verbose);
static void	pxe_cleanup(void);
static void	pxe_setnfshandle(char *rootpath);

static void	pxe_perror(int error);
static int	pxe_netif_match(struct netif *nif, void *machdep_hint);
static int	pxe_netif_probe(struct netif *nif, void *machdep_hint);
static void	pxe_netif_init(struct iodesc *desc, void *machdep_hint);
static int	pxe_netif_get(struct iodesc *desc, void *pkt, size_t len,
			      time_t timeout);
static int	pxe_netif_put(struct iodesc *desc, void *pkt, size_t len);
static void	pxe_netif_end(struct netif *nif);

extern struct netif_stats	pxe_st[];
extern u_int16_t		__bangpxeseg;
extern u_int16_t		__bangpxeoff;
extern void			__bangpxeentry(void);
extern u_int16_t		__pxenvseg;
extern u_int16_t		__pxenvoff;
extern void			__pxenventry(void);

struct netif_dif pxe_ifs[] = {
/*      dif_unit        dif_nsel        dif_stats       dif_private     */
	{0,             1,              &pxe_st[0],     0}
};

struct netif_stats pxe_st[NENTS(pxe_ifs)];

struct netif_driver pxenetif = {
	"pxenet",
	pxe_netif_match,
	pxe_netif_probe,
	pxe_netif_init,
	pxe_netif_get,
	pxe_netif_put,
	pxe_netif_end,
	pxe_ifs,
	NENTS(pxe_ifs)
};

struct netif_driver *netif_drivers[] = {
	&pxenetif,
	NULL
};

struct devsw pxedisk = {
	"pxe", 
	DEVT_NET,
	pxe_init,
	pxe_strategy, 
	pxe_open, 
	pxe_close, 
	noioctl,
	pxe_print,
	pxe_cleanup
};

/*
 * This function is called by the loader to enable PXE support if we
 * are booted by PXE.  The passed in pointer is a pointer to the
 * PXENV+ structure.
 */
void
pxe_enable(void *pxeinfo)
{
	pxenv_p  = (pxenv_t *)pxeinfo;
	pxe_p    = (pxe_t *)PTOV(pxenv_p->PXEPtr.segment * 16 +
				 pxenv_p->PXEPtr.offset);
	pxe_call = NULL;
}

/* 
 * return true if pxe structures are found/initialized,
 * also figures out our IP information via the pxe cached info struct 
 */
static int
pxe_init(void)
{
	t_PXENV_GET_CACHED_INFO	*gci_p;
	int	counter;
	uint8_t checksum;
	uint8_t *checkptr;
	
	if(pxenv_p == NULL)
		return (0);

	/*  look for "PXENV+" */
	if (bcmp((void *)pxenv_p->Signature, S_SIZE("PXENV+"))) {
		pxenv_p = NULL;
		return (0);
	}

	/* make sure the size is something we can handle */
	if (pxenv_p->Length > sizeof(*pxenv_p)) {
	  	printf("PXENV+ structure too large, ignoring\n");
		pxenv_p = NULL;
		return (0);
	}
	    
	/* 
	 * do byte checksum:
	 * add up each byte in the structure, the total should be 0
	 */
	checksum = 0;	
	checkptr = (uint8_t *) pxenv_p;
	for (counter = 0; counter < pxenv_p->Length; counter++)
		checksum += *checkptr++;
	if (checksum != 0) {
		printf("PXENV+ structure failed checksum, ignoring\n");
		pxenv_p = NULL;
		return (0);
	}

	
	/*
	 * PXENV+ passed, so use that if !PXE is not available or
	 * the checksum fails.
	 */
	pxe_call = pxenv_call;
	if (pxenv_p->Version >= 0x0200) {
		for (;;) {
			if (bcmp((void *)pxe_p->Signature, S_SIZE("!PXE"))) {
				pxe_p = NULL;
				break;
			}
			checksum = 0;
			checkptr = (uint8_t *)pxe_p;
			for (counter = 0; counter < pxe_p->StructLength;
			     counter++)
				checksum += *checkptr++;
			if (checksum != 0) {
				pxe_p = NULL;
				break;
			}
			pxe_call = bangpxe_call;
			break;
		}
	}
	
	printf("\nPXE version %d.%d, real mode entry point ",
	       (uint8_t) (pxenv_p->Version >> 8),
	       (uint8_t) (pxenv_p->Version & 0xFF));
	if (pxe_call == bangpxe_call)
		printf("@%04x:%04x\n",
		       pxe_p->EntryPointSP.segment,
		       pxe_p->EntryPointSP.offset);
	else
		printf("@%04x:%04x\n",
		       pxenv_p->RMEntry.segment, pxenv_p->RMEntry.offset);

	gci_p = (t_PXENV_GET_CACHED_INFO *) scratch_buffer;
	bzero(gci_p, sizeof(*gci_p));
	gci_p->PacketType =  PXENV_PACKET_TYPE_BINL_REPLY;
	pxe_call(PXENV_GET_CACHED_INFO);
	if (gci_p->Status != 0) {
		pxe_perror(gci_p->Status);
		pxe_p = NULL;
		return (0);
	}
	bcopy(PTOV((gci_p->Buffer.segment << 4) + gci_p->Buffer.offset),
	      &bootplayer, gci_p->BufferSize);
	return (1);
}


static int
pxe_strategy(void *devdata, int flag, daddr_t dblk, size_t size,
		char *buf, size_t *rsize)
{
	return (EIO);
}

static int
pxe_open(struct open_file *f, ...)
{
    va_list args;
    char *devname;		/* Device part of file name (or NULL). */
    char temp[FNAME_SIZE];
    int error = 0;
    int i;
	
    va_start(args, f);
    devname = va_arg(args, char*);
    va_end(args);

    /* On first open, do netif open, mount, etc. */
    if (pxe_opens == 0) {
	/* Find network interface. */
	if (pxe_sock < 0) {
	    pxe_sock = netif_open(devname);
	    if (pxe_sock < 0) {
		printf("pxe_open: netif_open() failed\n");
		return (ENXIO);
	    }
	    if (pxe_debug)
		printf("pxe_open: netif_open() succeeded\n");
	}
	if (rootip.s_addr == 0) {
		/*
		 * Do a bootp/dhcp request to find out where our
		 * NFS/TFTP server is.  Even if we dont get back
		 * the proper information, fall back to the server
		 * which brought us to life and a default rootpath.
		 */
		bootp(pxe_sock, BOOTP_PXE);
		if (rootip.s_addr == 0)
			rootip.s_addr = bootplayer.sip;
		if (!rootpath[1])
			strcpy(rootpath, PXENFSROOTPATH);

		for (i = 0; rootpath[i] != '\0' && i < FNAME_SIZE; i++)
			if (rootpath[i] == ':')
				break;
		if (i && i != FNAME_SIZE && rootpath[i] == ':') {
			rootpath[i++] = '\0';
			if (inet_addr(&rootpath[0]) != INADDR_NONE)
				rootip.s_addr = inet_addr(&rootpath[0]);
			bcopy(&rootpath[i], &temp[0], strlen(&rootpath[i])+1);
			bcopy(&temp[0], &rootpath[0], strlen(&rootpath[i])+1);
		}
		printf("pxe_open: server addr: %s\n", inet_ntoa(rootip));
		printf("pxe_open: server path: %s\n", rootpath);
		printf("pxe_open: gateway ip:  %s\n", inet_ntoa(gateip));

		setenv("boot.netif.ip", inet_ntoa(myip), 1);
		setenv("boot.netif.netmask", intoa(netmask), 1);
		setenv("boot.netif.gateway", inet_ntoa(gateip), 1);
		if (bootplayer.Hardware == ETHER_TYPE) {
		    sprintf(temp, "%6D", bootplayer.CAddr, ":");
		    setenv("boot.netif.hwaddr", temp, 1);
		}
		setenv("boot.nfsroot.server", inet_ntoa(rootip), 1);
		setenv("boot.nfsroot.path", rootpath, 1);
		setenv("dhcp.host-name", hostname, 1);
	}
    }
    pxe_opens++;
    f->f_devdata = &pxe_sock;
    return (error);
}

static int
pxe_close(struct open_file *f)
{

#ifdef	PXE_DEBUG
    if (pxe_debug)
	printf("pxe_close: opens=%d\n", pxe_opens);
#endif

    /* On last close, do netif close, etc. */
    f->f_devdata = NULL;
    /* Extra close call? */
    if (pxe_opens <= 0)
	return (0);
    pxe_opens--;
    /* Not last close? */
    if (pxe_opens > 0)
	return(0);

#ifdef LOADER_NFS_SUPPORT
    /* get an NFS filehandle for our root filesystem */
    pxe_setnfshandle(rootpath);
#endif

    if (pxe_sock >= 0) {

#ifdef PXE_DEBUG
	if (pxe_debug)
	    printf("pxe_close: calling netif_close()\n");
#endif
	netif_close(pxe_sock);
	pxe_sock = -1;
    }
    return (0);
}

static void
pxe_print(int verbose)
{
	if (pxe_call != NULL) {
		if (*bootplayer.Sname == '\0') {
			printf("      "IP_STR":%s\n",
			       IP_ARGS(htonl(bootplayer.sip)),
			       bootplayer.bootfile);
		} else {
			printf("      %s:%s\n", bootplayer.Sname,
			       bootplayer.bootfile);
		}
	}

	return;
}

static void
pxe_cleanup(void)
{
#ifdef PXE_DEBUG
	t_PXENV_UNLOAD_STACK *unload_stack_p =
	    (t_PXENV_UNLOAD_STACK *)scratch_buffer;
	t_PXENV_UNDI_SHUTDOWN *undi_shutdown_p =
	    (t_PXENV_UNDI_SHUTDOWN *)scratch_buffer;
#endif

	if (pxe_call == NULL)
		return;

	pxe_call(PXENV_UNDI_SHUTDOWN);

#ifdef PXE_DEBUG
	if (pxe_debug && undi_shutdown_p->Status != 0)
		printf("pxe_cleanup: UNDI_SHUTDOWN failed %x\n",
		       undi_shutdown_p->Status);
#endif

	pxe_call(PXENV_UNLOAD_STACK);

#ifdef PXE_DEBUG	
	if (pxe_debug && unload_stack_p->Status != 0)
		printf("pxe_cleanup: UNLOAD_STACK failed %x\n",
		    unload_stack_p->Status);
#endif
}

void
pxe_perror(int err)
{
	return;
}

/*
 * Reach inside the libstand NFS code and dig out an NFS handle
 * for the root filesystem.
 */
struct nfs_iodesc {
	struct	iodesc	*iodesc;
	off_t	off;
	u_char	fh[NFS_FHSIZE];
	/* structure truncated here */
};
extern struct	nfs_iodesc nfs_root_node;
extern int      rpc_port;

static void
pxe_rpcmountcall()
{
	struct	iodesc *d;
	int     error;

	if (!(d = socktodesc(pxe_sock)))
		return;
        d->myport = htons(--rpc_port);
        d->destip = rootip;
	if ((error = nfs_getrootfh(d, rootpath, nfs_root_node.fh)) != 0) 
		printf("NFS MOUNT RPC error: %d\n", error);
	nfs_root_node.iodesc = d;
}

static void
pxe_setnfshandle(char *rootpath)
{
	int	i;
	u_char	*fh;
	char	buf[2 * NFS_FHSIZE + 3], *cp;

	/*
	 * If NFS files were never opened, we need to do mount call
	 * ourselves. Use nfs_root_node.iodesc as flag indicating
	 * previous NFS usage.
	 */
	if (nfs_root_node.iodesc == NULL)
		pxe_rpcmountcall();

	fh = &nfs_root_node.fh[0];
	buf[0] = 'X';
	cp = &buf[1];
	for (i = 0; i < NFS_FHSIZE; i++, cp += 2)
		sprintf(cp, "%02x", fh[i]);
	sprintf(cp, "X");
	setenv("boot.nfsroot.nfshandle", buf, 1);
}

void
pxenv_call(int func)
{
#ifdef PXE_DEBUG
	if (pxe_debug)
		printf("pxenv_call %x\n", func);
#endif
	
	bzero(&v86, sizeof(v86));
	bzero(data_buffer, sizeof(data_buffer));

	__pxenvseg = pxenv_p->RMEntry.segment;
	__pxenvoff = pxenv_p->RMEntry.offset;
	
	v86.ctl  = V86_ADDR | V86_CALLF | V86_FLAGS;
	v86.es   = VTOPSEG(scratch_buffer);
	v86.edi  = VTOPOFF(scratch_buffer);
	v86.addr = (VTOPSEG(__pxenventry) << 16) | VTOPOFF(__pxenventry);
	v86.ebx  = func;
	v86int();
	v86.ctl  = V86_FLAGS;
}

void
bangpxe_call(int func)
{
#ifdef PXE_DEBUG
	if (pxe_debug)
		printf("bangpxe_call %x\n", func);
#endif
	
	bzero(&v86, sizeof(v86));
	bzero(data_buffer, sizeof(data_buffer));

	__bangpxeseg = pxe_p->EntryPointSP.segment;
	__bangpxeoff = pxe_p->EntryPointSP.offset;
	
	v86.ctl  = V86_ADDR | V86_CALLF | V86_FLAGS;
	v86.edx  = VTOPSEG(scratch_buffer);
	v86.eax  = VTOPOFF(scratch_buffer);
	v86.addr = (VTOPSEG(__bangpxeentry) << 16) | VTOPOFF(__bangpxeentry);
	v86.ebx  = func;
	v86int();
	v86.ctl  = V86_FLAGS;
}


time_t
getsecs()
{
	time_t n = 0;
	time(&n);
	return n;
}

static int
pxe_netif_match(struct netif *nif, void *machdep_hint)
{
	return 1;
}


static int
pxe_netif_probe(struct netif *nif, void *machdep_hint)
{
	t_PXENV_UDP_OPEN *udpopen_p = (t_PXENV_UDP_OPEN *)scratch_buffer;

	if (pxe_call == NULL)
		return -1;

	bzero(udpopen_p, sizeof(*udpopen_p));
	udpopen_p->src_ip = bootplayer.yip;
	pxe_call(PXENV_UDP_OPEN);

	if (udpopen_p->status != 0) {
		printf("pxe_netif_probe: failed %x\n", udpopen_p->status);
		return -1;
	}
	return 0;
}

static void
pxe_netif_end(struct netif *nif)
{
	t_PXENV_UDP_CLOSE *udpclose_p = (t_PXENV_UDP_CLOSE *)scratch_buffer;
	bzero(udpclose_p, sizeof(*udpclose_p));

	pxe_call(PXENV_UDP_CLOSE);
	if (udpclose_p->status != 0)
		printf("pxe_end failed %x\n", udpclose_p->status);
}

static void
pxe_netif_init(struct iodesc *desc, void *machdep_hint)
{
	int i;
	for (i = 0; i < 6; ++i)
		desc->myea[i] = bootplayer.CAddr[i];
	desc->xid = bootplayer.ident;
}

static int
pxe_netif_get(struct iodesc *desc, void *pkt, size_t len, time_t timeout)
{
	return len;
}

static int
pxe_netif_put(struct iodesc *desc, void *pkt, size_t len)
{
	return len;
}

ssize_t
sendudp(struct iodesc *h, void *pkt, size_t len)
{
	t_PXENV_UDP_WRITE *udpwrite_p = (t_PXENV_UDP_WRITE *)scratch_buffer;
	bzero(udpwrite_p, sizeof(*udpwrite_p));
	
	udpwrite_p->ip             = h->destip.s_addr;
	udpwrite_p->dst_port       = h->destport;
	udpwrite_p->src_port       = h->myport;
	udpwrite_p->buffer_size    = len;
	udpwrite_p->buffer.segment = VTOPSEG(pkt);
	udpwrite_p->buffer.offset  = VTOPOFF(pkt);

	if (netmask == 0 || SAMENET(myip, h->destip, netmask))
		udpwrite_p->gw = 0;
	else
		udpwrite_p->gw = gateip.s_addr;

	pxe_call(PXENV_UDP_WRITE);

#if 0
	/* XXX - I dont know why we need this. */
	delay(1000);
#endif
	if (udpwrite_p->status != 0) {
		/* XXX: This happens a lot.  It shouldn't. */
		if (udpwrite_p->status != 1)
			printf("sendudp failed %x\n", udpwrite_p->status);
		return -1;
	}
	return len;
}

ssize_t
readudp(struct iodesc *h, void *pkt, size_t len, time_t timeout)
{
	t_PXENV_UDP_READ *udpread_p = (t_PXENV_UDP_READ *)scratch_buffer;
	struct udphdr *uh = NULL;
	
	uh = (struct udphdr *) pkt - 1;
	bzero(udpread_p, sizeof(*udpread_p));
	
	udpread_p->dest_ip        = h->myip.s_addr;
	udpread_p->d_port         = h->myport;
	udpread_p->buffer_size    = len;
	udpread_p->buffer.segment = VTOPSEG(data_buffer);
	udpread_p->buffer.offset  = VTOPOFF(data_buffer);

	pxe_call(PXENV_UDP_READ);

#if 0
	/* XXX - I dont know why we need this. */
	delay(1000);
#endif
	if (udpread_p->status != 0) {
		/* XXX: This happens a lot.  It shouldn't. */
		if (udpread_p->status != 1)
			printf("readudp failed %x\n", udpread_p->status);
		return -1;
	}
	bcopy(data_buffer, pkt, udpread_p->buffer_size);
	uh->uh_sport = udpread_p->s_port;
	return udpread_p->buffer_size;
}
