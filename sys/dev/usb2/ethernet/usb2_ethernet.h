/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
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

#ifndef _USB2_ETHERNET_H_
#define	_USB2_ETHERNET_H_

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/limits.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/ethernet.h>

#include "miibus_if.h"

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

struct usb2_ether;
struct usb2_device_request;

typedef void (usb2_ether_fn_t)(struct usb2_ether *);

struct usb2_ether_methods {
	usb2_ether_fn_t		*ue_attach_post;
	usb2_ether_fn_t		*ue_start;
	usb2_ether_fn_t		*ue_init;
	usb2_ether_fn_t		*ue_stop;
	usb2_ether_fn_t		*ue_setmulti;
	usb2_ether_fn_t		*ue_setpromisc;
	usb2_ether_fn_t		*ue_tick;
	int			(*ue_mii_upd)(struct ifnet *);
	void			(*ue_mii_sts)(struct ifnet *,
				    struct ifmediareq *);
	int			(*ue_ioctl)(struct ifnet *, u_long, caddr_t);

};

struct usb2_ether_cfg_task {
	struct usb2_proc_msg hdr;
	struct usb2_ether *ue;
};

struct usb2_ether {
	/* NOTE: the "ue_ifp" pointer must be first --hps */
	struct ifnet		*ue_ifp;
	struct mtx		*ue_mtx;
	const struct usb2_ether_methods *ue_methods;
	struct sysctl_oid	*ue_sysctl_oid;
	void			*ue_sc;
	struct usb2_device	*ue_udev; /* used by usb2_ether_do_request() */
	device_t		ue_dev;
	device_t		ue_miibus;

	struct usb2_process	ue_tq;
	struct sysctl_ctx_list	ue_sysctl_ctx;
	struct ifqueue		ue_rxq;
	struct usb2_callout	ue_watchdog;
	struct usb2_ether_cfg_task	ue_sync_task[2];
	struct usb2_ether_cfg_task	ue_media_task[2];
	struct usb2_ether_cfg_task	ue_multi_task[2];
	struct usb2_ether_cfg_task	ue_promisc_task[2];
	struct usb2_ether_cfg_task	ue_tick_task[2];

	int			ue_unit;

	/* ethernet address from eeprom */
	uint8_t			ue_eaddr[ETHER_ADDR_LEN];
};

#define	usb2_ether_do_request(ue,req,data,timo) \
    usb2_do_request_proc((ue)->ue_udev,&(ue)->ue_tq,req,data,0,NULL,timo)

uint8_t		usb2_ether_pause(struct usb2_ether *, unsigned int);
struct ifnet	*usb2_ether_getifp(struct usb2_ether *);
struct mii_data *usb2_ether_getmii(struct usb2_ether *);
void		*usb2_ether_getsc(struct usb2_ether *);
int		usb2_ether_ifattach(struct usb2_ether *);
void		usb2_ether_ifdetach(struct usb2_ether *);
int		usb2_ether_ioctl(struct ifnet *, u_long, caddr_t);
int		usb2_ether_rxmbuf(struct usb2_ether *, struct mbuf *, 
		    unsigned int);
int		usb2_ether_rxbuf(struct usb2_ether *,
		    struct usb2_page_cache *, 
		    unsigned int, unsigned int);
void		usb2_ether_rxflush(struct usb2_ether *);
void		usb2_ether_ifshutdown(struct usb2_ether *);
uint8_t		usb2_ether_is_gone(struct usb2_ether *);
#endif					/* _USB2_ETHERNET_H_ */
