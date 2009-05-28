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

struct usb_ether;
struct usb_device_request;

typedef void (usb2_ether_fn_t)(struct usb_ether *);

struct usb_ether_methods {
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

struct usb_ether_cfg_task {
	struct usb_proc_msg hdr;
	struct usb_ether *ue;
};

struct usb_ether {
	/* NOTE: the "ue_ifp" pointer must be first --hps */
	struct ifnet		*ue_ifp;
	struct mtx		*ue_mtx;
	const struct usb_ether_methods *ue_methods;
	struct sysctl_oid	*ue_sysctl_oid;
	void			*ue_sc;
	struct usb_device	*ue_udev; /* used by usb2_ether_do_request() */
	device_t		ue_dev;
	device_t		ue_miibus;

	struct usb_process	ue_tq;
	struct sysctl_ctx_list	ue_sysctl_ctx;
	struct ifqueue		ue_rxq;
	struct usb_callout	ue_watchdog;
	struct usb_ether_cfg_task	ue_sync_task[2];
	struct usb_ether_cfg_task	ue_media_task[2];
	struct usb_ether_cfg_task	ue_multi_task[2];
	struct usb_ether_cfg_task	ue_promisc_task[2];
	struct usb_ether_cfg_task	ue_tick_task[2];

	int			ue_unit;

	/* ethernet address from eeprom */
	uint8_t			ue_eaddr[ETHER_ADDR_LEN];
};

#define	usb2_ether_do_request(ue,req,data,timo) \
    usb2_do_request_proc((ue)->ue_udev,&(ue)->ue_tq,req,data,0,NULL,timo)

uint8_t		usb2_ether_pause(struct usb_ether *, unsigned int);
struct ifnet	*usb2_ether_getifp(struct usb_ether *);
struct mii_data *usb2_ether_getmii(struct usb_ether *);
void		*usb2_ether_getsc(struct usb_ether *);
int		usb2_ether_ifattach(struct usb_ether *);
void		usb2_ether_ifdetach(struct usb_ether *);
int		usb2_ether_ioctl(struct ifnet *, u_long, caddr_t);
struct mbuf	*usb2_ether_newbuf(void);
int		usb2_ether_rxmbuf(struct usb_ether *, struct mbuf *, 
		    unsigned int);
int		usb2_ether_rxbuf(struct usb_ether *,
		    struct usb_page_cache *, 
		    unsigned int, unsigned int);
void		usb2_ether_rxflush(struct usb_ether *);
uint8_t		usb2_ether_is_gone(struct usb_ether *);
#endif					/* _USB2_ETHERNET_H_ */
