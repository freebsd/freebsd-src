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
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <sys/param.h>

#include <net/ethernet.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
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
 * BTX's private data area. The scratch buffer is used to send information to
 * the PXE BIOS, and the data buffer is used to receive data from the PXE BIOS.
 */
#define	PXE_BUFFER_SIZE		0x2000
static char	scratch_buffer[PXE_BUFFER_SIZE];
static char	data_buffer[PXE_BUFFER_SIZE];

static pxenv_t *pxenv_p = NULL;	/* PXENV+ */
static pxe_t *pxe_p = NULL;		/* !PXE */

#ifdef PXE_DEBUG
static int	pxe_debug = 0;
#endif

void		pxe_enable(void *pxeinfo);
static void	(*pxe_call)(int func);
static void	pxenv_call(int func);
static void	bangpxe_call(int func);

static int	pxe_init(void);
static int	pxe_print(int verbose);
static void	pxe_cleanup(void);

static void	pxe_perror(int error);
static int	pxe_netif_match(struct netif *nif, void *machdep_hint);
static int	pxe_netif_probe(struct netif *nif, void *machdep_hint);
static void	pxe_netif_init(struct iodesc *desc, void *machdep_hint);
static ssize_t	pxe_netif_get(struct iodesc *, void **, time_t);
static ssize_t	pxe_netif_put(struct iodesc *desc, void *pkt, size_t len);
static void	pxe_netif_end(struct netif *nif);

extern struct netif_stats	pxe_st[];
extern uint16_t			__bangpxeseg;
extern uint16_t			__bangpxeoff;
extern void			__bangpxeentry(void);
extern uint16_t			__pxenvseg;
extern uint16_t			__pxenvoff;
extern void			__pxenventry(void);

struct netif_dif pxe_ifs[] = {
/*	dif_unit        dif_nsel        dif_stats       dif_private     */
	{0,             1,              &pxe_st[0],     0}
};

struct netif_stats pxe_st[nitems(pxe_ifs)];

struct netif_driver pxenetif = {
	.netif_bname = "pxenet",
	.netif_match = pxe_netif_match,
	.netif_probe = pxe_netif_probe,
	.netif_init = pxe_netif_init,
	.netif_get = pxe_netif_get,
	.netif_put = pxe_netif_put,
	.netif_end = pxe_netif_end,
	.netif_ifs = pxe_ifs,
	.netif_nifs = nitems(pxe_ifs)
};

struct netif_driver *netif_drivers[] = {
	&pxenetif,
	NULL
};

struct devsw pxedisk = {
	.dv_name = "net",
	.dv_type = DEVT_NET,
	.dv_init = pxe_init,
	.dv_strategy = NULL,	/* Will be set in pxe_init */
	.dv_open = NULL,	/* Will be set in pxe_init */
	.dv_close = NULL,	/* Will be set in pxe_init */
	.dv_ioctl = noioctl,
	.dv_print = pxe_print,
	.dv_cleanup = pxe_cleanup
};

/*
 * This function is called by the loader to enable PXE support if we
 * are booted by PXE. The passed in pointer is a pointer to the PXENV+
 * structure.
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
	t_PXENV_GET_CACHED_INFO *gci_p;
	int counter;
	uint8_t checksum;
	uint8_t *checkptr;
	extern struct devsw netdev;

	if (pxenv_p == NULL)
		return (0);

	/* look for "PXENV+" */
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

	pxedisk.dv_open = netdev.dv_open;
	pxedisk.dv_close = netdev.dv_close;
	pxedisk.dv_strategy = netdev.dv_strategy;

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
	gci_p->PacketType = PXENV_PACKET_TYPE_BINL_REPLY;
	pxe_call(PXENV_GET_CACHED_INFO);
	if (gci_p->Status != 0) {
		pxe_perror(gci_p->Status);
		pxe_p = NULL;
		return (0);
	}
	free(bootp_response);
	if ((bootp_response = malloc(gci_p->BufferSize)) != NULL) {
		bootp_response_size = gci_p->BufferSize;
		bcopy(PTOV((gci_p->Buffer.segment << 4) + gci_p->Buffer.offset),
		    bootp_response, bootp_response_size);
	}
	return (1);
}

static int
pxe_print(int verbose)
{
	if (pxe_call == NULL)
		return (0);

	printf("%s devices:", pxedisk.dv_name);
	if (pager_output("\n") != 0)
		return (1);
	printf("    %s0:", pxedisk.dv_name);
	if (verbose) {
		printf("    %s:%s", inet_ntoa(rootip), rootpath);
	}
	return (pager_output("\n"));
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


static int
pxe_netif_match(struct netif *nif, void *machdep_hint)
{
	return (1);
}

static int
pxe_netif_probe(struct netif *nif, void *machdep_hint)
{
	if (pxe_call == NULL)
		return (-1);

	return (0);
}

static void
pxe_netif_end(struct netif *nif)
{
	t_PXENV_UNDI_CLOSE *undi_close_p;

	undi_close_p = (t_PXENV_UNDI_CLOSE *)scratch_buffer;
	bzero(undi_close_p, sizeof(*undi_close_p));
	pxe_call(PXENV_UNDI_CLOSE);
	if (undi_close_p->Status != 0)
		printf("undi close failed: %x\n", undi_close_p->Status);
}

static void
pxe_netif_init(struct iodesc *desc, void *machdep_hint)
{
	t_PXENV_UNDI_GET_INFORMATION *undi_info_p;
	t_PXENV_UNDI_OPEN *undi_open_p;
	uint8_t *mac;
	int i, len;

	undi_info_p = (t_PXENV_UNDI_GET_INFORMATION *)scratch_buffer;
	bzero(undi_info_p, sizeof(*undi_info_p));
	pxe_call(PXENV_UNDI_GET_INFORMATION);
	if (undi_info_p->Status != 0) {
		printf("undi get info failed: %x\n", undi_info_p->Status);
		return;
	}

	/* Make sure the CurrentNodeAddress is valid. */
	for (i = 0; i < undi_info_p->HwAddrLen; ++i) {
		if (undi_info_p->CurrentNodeAddress[i] != 0)
			break;
	}
	if (i < undi_info_p->HwAddrLen) {
		for (i = 0; i < undi_info_p->HwAddrLen; ++i) {
			if (undi_info_p->CurrentNodeAddress[i] != 0xff)
				break;
		}
	}
	if (i < undi_info_p->HwAddrLen)
		mac = undi_info_p->CurrentNodeAddress;
	else
		mac = undi_info_p->PermNodeAddress;

	len = min(sizeof (desc->myea), undi_info_p->HwAddrLen);
	for (i = 0; i < len; ++i)
		desc->myea[i] = mac[i];

	if (bootp_response != NULL)
		desc->xid = bootp_response->bp_xid;
	else
		desc->xid = 0;

	undi_open_p = (t_PXENV_UNDI_OPEN *)scratch_buffer;
	bzero(undi_open_p, sizeof(*undi_open_p));
	undi_open_p->PktFilter = FLTR_DIRECTED | FLTR_BRDCST;
	pxe_call(PXENV_UNDI_OPEN);
	if (undi_open_p->Status != 0)
		printf("undi open failed: %x\n", undi_open_p->Status);
}

static int
pxe_netif_receive(void **pkt)
{
	t_PXENV_UNDI_ISR *isr = (t_PXENV_UNDI_ISR *)scratch_buffer;
	char *buf, *ptr, *frame;
	size_t size, rsize;

	bzero(isr, sizeof(*isr));
	isr->FuncFlag = PXENV_UNDI_ISR_IN_START;
	pxe_call(PXENV_UNDI_ISR);
	if (isr->Status != 0)
		return (-1);

	bzero(isr, sizeof(*isr));
	isr->FuncFlag = PXENV_UNDI_ISR_IN_PROCESS;
	pxe_call(PXENV_UNDI_ISR);
	if (isr->Status != 0)
		return (-1);

	while (isr->FuncFlag == PXENV_UNDI_ISR_OUT_TRANSMIT) {
		/*
		 * Wait till transmit is done.
		 */
		bzero(isr, sizeof(*isr));
		isr->FuncFlag = PXENV_UNDI_ISR_IN_GET_NEXT;
		pxe_call(PXENV_UNDI_ISR);
		if (isr->Status != 0 ||
		    isr->FuncFlag == PXENV_UNDI_ISR_OUT_DONE)
			return (-1);
	}

	while (isr->FuncFlag != PXENV_UNDI_ISR_OUT_RECEIVE) {
		if (isr->Status != 0 ||
		    isr->FuncFlag == PXENV_UNDI_ISR_OUT_DONE) {
			return (-1);
		}
		bzero(isr, sizeof(*isr));
		isr->FuncFlag = PXENV_UNDI_ISR_IN_GET_NEXT;
		pxe_call(PXENV_UNDI_ISR);
	}

	size = isr->FrameLength;
	buf = malloc(size + ETHER_ALIGN);
	if (buf == NULL)
		return (-1);
	ptr = buf + ETHER_ALIGN;
	rsize = 0;

	while (rsize < size) {
		frame = (char *)((uintptr_t)isr->Frame.segment << 4);
		frame += isr->Frame.offset;
		bcopy(PTOV(frame), ptr, isr->BufferLength);
		ptr += isr->BufferLength;
		rsize += isr->BufferLength;

		bzero(isr, sizeof(*isr));
		isr->FuncFlag = PXENV_UNDI_ISR_IN_GET_NEXT;
		pxe_call(PXENV_UNDI_ISR);
		if (isr->Status != 0) {
			free(buf);
			return (-1);
		}

		/* Did we got another update? */
		if (isr->FuncFlag == PXENV_UNDI_ISR_OUT_RECEIVE)
			continue;
		break;
	}

	*pkt = buf;
	return (rsize);
}

static ssize_t
pxe_netif_get(struct iodesc *desc, void **pkt, time_t timeout)
{
	time_t t;
	void *ptr;
	int ret = -1;

	t = getsecs();
	while ((getsecs() - t) < timeout) {
		ret = pxe_netif_receive(&ptr);
		if (ret != -1) {
			*pkt = ptr;
			break;
		}
	}
	return (ret);
}

static ssize_t
pxe_netif_put(struct iodesc *desc, void *pkt, size_t len)
{
	t_PXENV_UNDI_TRANSMIT *trans_p;
	t_PXENV_UNDI_TBD *tbd_p;
	char *data;

	trans_p = (t_PXENV_UNDI_TRANSMIT *)scratch_buffer;
	bzero(trans_p, sizeof(*trans_p));
	tbd_p = (t_PXENV_UNDI_TBD *)(scratch_buffer + sizeof(*trans_p));
	bzero(tbd_p, sizeof(*tbd_p));

	data = scratch_buffer + sizeof(*trans_p) + sizeof(*tbd_p);

	trans_p->TBD.segment = VTOPSEG(tbd_p);
	trans_p->TBD.offset  = VTOPOFF(tbd_p);

	tbd_p->ImmedLength = len;
	tbd_p->Xmit.segment = VTOPSEG(data);
	tbd_p->Xmit.offset  = VTOPOFF(data);
	bcopy(pkt, data, len);

	pxe_call(PXENV_UNDI_TRANSMIT);
	if (trans_p->Status != 0) {
		return (-1);
	}

	return (len);
}
