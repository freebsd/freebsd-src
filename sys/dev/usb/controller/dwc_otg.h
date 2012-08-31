/* $FreeBSD$ */
/*-
 * Copyright (c) 2012 Hans Petter Selasky. All rights reserved.
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

#ifndef _DWC_OTG_H_
#define	_DWC_OTG_H_

#define	DWC_OTG_MAX_DEVICES (USB_MIN_DEVICES + 1)
#define	DWC_OTG_FRAME_MASK 0x7FF
#define	DWC_OTG_MAX_TXP 4
#define	DWC_OTG_MAX_TXN (0x200 * DWC_OTG_MAX_TXP)

#define	DWC_OTG_READ_4(sc, reg) \
  bus_space_read_4((sc)->sc_io_tag, (sc)->sc_io_hdl, reg)

#define	DWC_OTG_WRITE_4(sc, reg, data)	\
  bus_space_write_4((sc)->sc_io_tag, (sc)->sc_io_hdl, reg, data)

struct dwc_otg_td;
struct dwc_otg_softc;

typedef uint8_t (dwc_otg_cmd_t)(struct dwc_otg_td *td);

struct dwc_otg_td {
	struct dwc_otg_td *obj_next;
	dwc_otg_cmd_t *func;
	struct usb_page_cache *pc;
	uint32_t tx_bytes;
	uint32_t offset;
	uint32_t remainder;
	uint16_t max_packet_size;	/* packet_size */
	uint16_t npkt;
	uint8_t	ep_no;
	uint8_t	error:1;
	uint8_t	alt_next:1;
	uint8_t	short_pkt:1;
	uint8_t	did_stall:1;
};

struct dwc_otg_std_temp {
	dwc_otg_cmd_t *func;
	struct usb_page_cache *pc;
	struct dwc_otg_td *td;
	struct dwc_otg_td *td_next;
	uint32_t len;
	uint32_t offset;
	uint16_t max_frame_size;
	uint8_t	short_pkt;
	/*
	 * short_pkt = 0: transfer should be short terminated
	 * short_pkt = 1: transfer should not be short terminated
	 */
	uint8_t	setup_alt_next;
	uint8_t did_stall;
	uint8_t bulk_or_control;
};

struct dwc_otg_config_desc {
	struct usb_config_descriptor confd;
	struct usb_interface_descriptor ifcd;
	struct usb_endpoint_descriptor endpd;
} __packed;

union dwc_otg_hub_temp {
	uWord	wValue;
	struct usb_port_status ps;
};

struct dwc_otg_flags {
	uint8_t	change_connect:1;
	uint8_t	change_suspend:1;
	uint8_t	status_suspend:1;	/* set if suspended */
	uint8_t	status_vbus:1;		/* set if present */
	uint8_t	status_bus_reset:1;	/* set if reset complete */
	uint8_t	status_high_speed:1;	/* set if High Speed is selected */
	uint8_t	remote_wakeup:1;
	uint8_t	self_powered:1;
	uint8_t	clocks_off:1;
	uint8_t	port_powered:1;
	uint8_t	port_enabled:1;
	uint8_t	d_pulled_up:1;
};

struct dwc_otg_profile {
	struct usb_hw_ep_profile usb;
	uint16_t max_buffer;
};

struct dwc_otg_softc {
	struct usb_bus sc_bus;
	union dwc_otg_hub_temp sc_hub_temp;
	struct dwc_otg_profile sc_hw_ep_profile[16];

	struct usb_device *sc_devices[DWC_OTG_MAX_DEVICES];
	struct resource *sc_io_res;
	struct resource *sc_irq_res;
	void   *sc_intr_hdl;
	bus_size_t sc_io_size;
	bus_space_tag_t sc_io_tag;
	bus_space_handle_t sc_io_hdl;

	uint32_t sc_rx_bounce_buffer[1024 / 4];
	uint32_t sc_tx_bounce_buffer[(512 * DWC_OTG_MAX_TXP) / 4];

	uint32_t sc_fifo_size;
	uint32_t sc_irq_mask;
	uint32_t sc_last_rx_status;
	uint32_t sc_out_ctl[16];
	uint32_t sc_in_ctl[16];

	uint16_t sc_active_out_ep;

	uint8_t	sc_dev_ep_max;
	uint8_t sc_dev_in_ep_max;
	uint8_t	sc_rt_addr;		/* root HUB address */
	uint8_t	sc_conf;		/* root HUB config */

	uint8_t	sc_hub_idata[1];

	struct dwc_otg_flags sc_flags;
};

/* prototypes */

void dwc_otg_interrupt(struct dwc_otg_softc *);
int dwc_otg_init(struct dwc_otg_softc *);
void dwc_otg_uninit(struct dwc_otg_softc *);

#endif		/* _DWC_OTG_H_ */
