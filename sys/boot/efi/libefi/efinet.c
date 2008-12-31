/*-
 * Copyright (c) 2001 Doug Rabson
 * Copyright (c) 2002, 2006 Marcel Moolenaar
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
__FBSDID("$FreeBSD: src/sys/boot/efi/libefi/efinet.c,v 1.7.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <stand.h>
#include <net.h>
#include <netif.h>

#include <dev_net.c>

#include <efi.h>
#include <efilib.h>

static EFI_GUID sn_guid = EFI_SIMPLE_NETWORK_PROTOCOL;

static void efinet_end(struct netif *);
static int efinet_get(struct iodesc *, void *, size_t, time_t);
static void efinet_init(struct iodesc *, void *);
static int efinet_match(struct netif *, void *);
static int efinet_probe(struct netif *, void *);
static int efinet_put(struct iodesc *, void *, size_t);

struct netif_driver efinetif = {   
	.netif_bname = "efinet",
	.netif_match = efinet_match,
	.netif_probe = efinet_probe,
	.netif_init = efinet_init,
	.netif_get = efinet_get,
	.netif_put = efinet_put,
	.netif_end = efinet_end,
	.netif_ifs = NULL,
	.netif_nifs = 0
};

#ifdef EFINET_DEBUG
static void
dump_mode(EFI_SIMPLE_NETWORK_MODE *mode)
{
	int i;

	printf("State                 = %x\n", mode->State);
	printf("HwAddressSize         = %u\n", mode->HwAddressSize);
	printf("MediaHeaderSize       = %u\n", mode->MediaHeaderSize);
	printf("MaxPacketSize         = %u\n", mode->MaxPacketSize);
	printf("NvRamSize             = %u\n", mode->NvRamSize);
	printf("NvRamAccessSize       = %u\n", mode->NvRamAccessSize);
	printf("ReceiveFilterMask     = %x\n", mode->ReceiveFilterMask);
	printf("ReceiveFilterSetting  = %u\n", mode->ReceiveFilterSetting);
	printf("MaxMCastFilterCount   = %u\n", mode->MaxMCastFilterCount);
	printf("MCastFilterCount      = %u\n", mode->MCastFilterCount);
	printf("MCastFilter           = {");
	for (i = 0; i < mode->MCastFilterCount; i++)
		printf(" %s", ether_sprintf(mode->MCastFilter[i].Addr));
	printf(" }\n");
	printf("CurrentAddress        = %s\n",
	    ether_sprintf(mode->CurrentAddress.Addr));
	printf("BroadcastAddress      = %s\n",
	    ether_sprintf(mode->BroadcastAddress.Addr));
	printf("PermanentAddress      = %s\n",
	    ether_sprintf(mode->PermanentAddress.Addr));
	printf("IfType                = %u\n", mode->IfType);
	printf("MacAddressChangeable  = %d\n", mode->MacAddressChangeable);
	printf("MultipleTxSupported   = %d\n", mode->MultipleTxSupported);
	printf("MediaPresentSupported = %d\n", mode->MediaPresentSupported);
	printf("MediaPresent          = %d\n", mode->MediaPresent);
}
#endif

static int
efinet_match(struct netif *nif, void *machdep_hint)
{

	return (1);
}

static int
efinet_probe(struct netif *nif, void *machdep_hint)
{

	return (0);
}

static int
efinet_put(struct iodesc *desc, void *pkt, size_t len)
{
	struct netif *nif = desc->io_netif;
	EFI_SIMPLE_NETWORK *net;
	EFI_STATUS status;
	void *buf;

	net = nif->nif_devdata;

	status = net->Transmit(net, 0, len, pkt, 0, 0, 0);
	if (status != EFI_SUCCESS)
		return (-1);

	/* Wait for the buffer to be transmitted */
	do {
		buf = 0;	/* XXX Is this needed? */
		status = net->GetStatus(net, 0, &buf);
		/*
		 * XXX EFI1.1 and the E1000 card returns a different 
		 * address than we gave.  Sigh.
		 */
	} while (status == EFI_SUCCESS && buf == 0);

	/* XXX How do we deal with status != EFI_SUCCESS now? */
	return ((status == EFI_SUCCESS) ? len : -1);
}

static int
efinet_get(struct iodesc *desc, void *pkt, size_t len, time_t timeout)
{
	struct netif *nif = desc->io_netif;
	EFI_SIMPLE_NETWORK *net;
	EFI_STATUS status;
	UINTN bufsz;
	time_t t;
	char buf[2048];

	net = nif->nif_devdata;

	t = time(0);
	while ((time(0) - t) < timeout) {
		bufsz = sizeof(buf);
		status = net->Receive(net, 0, &bufsz, buf, 0, 0, 0);
		if (status == EFI_SUCCESS) {
			/*
			 * XXX EFI1.1 and the E1000 card trash our
			 * workspace if we do not do this silly copy.
			 * Either they are not respecting the len
			 * value or do not like the alignment.
			 */
			if (bufsz > len)
				bufsz = len;
			bcopy(buf, pkt, bufsz);
			return (bufsz);
		}
		if (status != EFI_NOT_READY)
			return (0);
	}

	return (0);
}

static void
efinet_init(struct iodesc *desc, void *machdep_hint)
{
	struct netif *nif = desc->io_netif;
	EFI_SIMPLE_NETWORK *net;
	EFI_HANDLE h;
	EFI_STATUS status;

	h = nif->nif_driver->netif_ifs[nif->nif_unit].dif_private;
	status = BS->HandleProtocol(h, &sn_guid, (VOID **)&nif->nif_devdata);
	if (status != EFI_SUCCESS) {
		printf("net%d: cannot start interface (status=%ld)\n",
		    nif->nif_unit, (long)status);
		return;
	}

	net = nif->nif_devdata;
	if (net->Mode->State == EfiSimpleNetworkStopped) {
		status = net->Start(net);
		if (status != EFI_SUCCESS) {
			printf("net%d: cannot start interface (status=%ld)\n",
			    nif->nif_unit, (long)status);
			return;
		}
	}

	if (net->Mode->State != EfiSimpleNetworkInitialized) {
		status = net->Initialize(net, 0, 0);
		if (status != EFI_SUCCESS) {
			printf("net%d: cannot init. interface (status=%ld)\n",
			    nif->nif_unit, (long)status);
			return;
		}
	}

	if (net->Mode->ReceiveFilterSetting == 0) {
		UINT32 mask = EFI_SIMPLE_NETWORK_RECEIVE_UNICAST |
		    EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST;

		status = net->ReceiveFilters(net, mask, 0, FALSE, 0, 0);
		if (status != EFI_SUCCESS) {
			printf("net%d: cannot set rx. filters (status=%ld)\n",
			    nif->nif_unit, (long)status);
			return;
		}
	}

#ifdef EFINET_DEBUG
	dump_mode(net->Mode);
#endif

	bcopy(net->Mode->CurrentAddress.Addr, desc->myea, 6);
	desc->xid = 1;
}

static void
efinet_end(struct netif *nif)
{
	EFI_SIMPLE_NETWORK *net = nif->nif_devdata; 

	net->Shutdown(net);
}

static int efinet_dev_init(void);
static void efinet_dev_print(int);

struct devsw efinet_dev = {
	.dv_name = "net",
	.dv_type = DEVT_NET,
	.dv_init = efinet_dev_init,
	.dv_strategy = net_strategy,
	.dv_open = net_open,
	.dv_close = net_close,
	.dv_ioctl = noioctl,
	.dv_print = efinet_dev_print,
	.dv_cleanup = NULL
};

static int
efinet_dev_init()
{
	struct netif_dif *dif;
	struct netif_stats *stats;
	EFI_HANDLE *handles;
	EFI_STATUS status;
	UINTN sz;
	int err, i, nifs;

	sz = 0;
	status = BS->LocateHandle(ByProtocol, &sn_guid, 0, &sz, 0);
	if (status == EFI_BUFFER_TOO_SMALL) {
		handles = (EFI_HANDLE *)malloc(sz);
		status = BS->LocateHandle(ByProtocol, &sn_guid, 0, &sz,
		    handles);
		if (EFI_ERROR(status))
			free(handles);
	}
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));
	nifs = sz / sizeof(EFI_HANDLE);
	err = efi_register_handles(&efinet_dev, handles, nifs);
	free(handles);
	if (err != 0)
		return (err);

	efinetif.netif_nifs = nifs;
	efinetif.netif_ifs = calloc(nifs, sizeof(struct netif_dif));

	stats = calloc(nifs, sizeof(struct netif_stats));

	for (i = 0; i < nifs; i++) {
		dif = &efinetif.netif_ifs[i];
		dif->dif_unit = i;
		dif->dif_nsel = 1;
		dif->dif_stats = &stats[i];
		dif->dif_private = efi_find_handle(&efinet_dev, i);
	}

	return (0);
}

static void
efinet_dev_print(int verbose)
{
	char line[80];
	EFI_HANDLE h;
	int unit;

	for (unit = 0, h = efi_find_handle(&efinet_dev, 0);
	    h != NULL; h = efi_find_handle(&efinet_dev, ++unit)) {
		sprintf(line, "    %s%d:\n", efinet_dev.dv_name, unit);
		pager_output(line);
	}
}
