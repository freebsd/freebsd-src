/* $FreeBSD$ */
/*-
 * Copyright (c) 2006 ATMEL
 * Copyright (c) 2007 Hans Petter Selasky <hselasky@freebsd.org>
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
 * USB Device Port (UDP) register definition, based on
 * "AT91RM9200.h" provided by ATMEL.
 */

#ifndef _AT9100_DCI_H_
#define	_AT9100_DCI_H_

#define	AT91_UDP_FRM 	0x00		/* Frame number register */
#define	AT91_UDP_FRM_MASK     (0x7FF <<  0)	/* Frame Number as Defined in
						 * the Packet Field Formats */
#define	AT91_UDP_FRM_ERR      (0x1 << 16)	/* Frame Error */
#define	AT91_UDP_FRM_OK       (0x1 << 17)	/* Frame OK */

#define	AT91_UDP_GSTATE 0x04		/* Global state register */
#define	AT91_UDP_GSTATE_ADDR  (0x1 <<  0)	/* Addressed state */
#define	AT91_UDP_GSTATE_CONFG (0x1 <<  1)	/* Configured */
#define	AT91_UDP_GSTATE_ESR   (0x1 <<  2)	/* Enable Send Resume */
#define	AT91_UDP_GSTATE_RSM   (0x1 <<  3)	/* A Resume Has Been Sent to
						 * the Host */
#define	AT91_UDP_GSTATE_RMW   (0x1 <<  4)	/* Remote Wake Up Enable */

#define	AT91_UDP_FADDR	0x08		/* Function Address Register */
#define	AT91_UDP_FADDR_MASK  (0x7F << 0)/* Function Address Mask */
#define	AT91_UDP_FADDR_EN    (0x1 <<  8)/* Function Enable */

#define	AT91_UDP_RES0	0x0C		/* Reserved 0 */

#define	AT91_UDP_IER	0x10		/* Interrupt Enable Register */
#define	AT91_UDP_IDR	0x14		/* Interrupt Disable Register */
#define	AT91_UDP_IMR	0x18		/* Interrupt Mask Register */
#define	AT91_UDP_ISR	0x1C		/* Interrupt Status Register */
#define	AT91_UDP_ICR	0x20		/* Interrupt Clear Register */
#define	AT91_UDP_INT_EP(n)   (0x1 <<(n))/* Endpoint "n" Interrupt */
#define	AT91_UDP_INT_RXSUSP  (0x1 <<  8)/* USB Suspend Interrupt */
#define	AT91_UDP_INT_RXRSM   (0x1 <<  9)/* USB Resume Interrupt */
#define	AT91_UDP_INT_EXTRSM  (0x1 << 10)/* USB External Resume Interrupt */
#define	AT91_UDP_INT_SOFINT  (0x1 << 11)/* USB Start Of frame Interrupt */
#define	AT91_UDP_INT_END_BR  (0x1 << 12)/* USB End Of Bus Reset Interrupt */
#define	AT91_UDP_INT_WAKEUP  (0x1 << 13)/* USB Resume Interrupt */

#define	AT91_UDP_INT_BUS \
  (AT91_UDP_INT_RXSUSP|AT91_UDP_INT_RXRSM| \
   AT91_UDP_INT_END_BR)

#define	AT91_UDP_INT_EPS \
  (AT91_UDP_INT_EP(0)|AT91_UDP_INT_EP(1)| \
   AT91_UDP_INT_EP(2)|AT91_UDP_INT_EP(3)| \
   AT91_UDP_INT_EP(4)|AT91_UDP_INT_EP(5))

#define	AT91_UDP_INT_DEFAULT \
  (AT91_UDP_INT_EPS|AT91_UDP_INT_BUS)

#define	AT91_UDP_RES1	0x24		/* Reserved 1 */
#define	AT91_UDP_RST	0x28		/* Reset Endpoint Register */
#define	AT91_UDP_RST_EP(n) (0x1 <<  (n))/* Reset Endpoint "n" */

#define	AT91_UDP_RES2	0x2C		/* Reserved 2 */

#define	AT91_UDP_CSR(n) (0x30 + (4*(n)))/* Endpoint Control and Status
					 * Register */
#define	AT91_UDP_CSR_TXCOMP (0x1 <<  0)	/* Generates an IN packet with data
					 * previously written in the DPR */
#define	AT91_UDP_CSR_RX_DATA_BK0 (0x1 <<  1)	/* Receive Data Bank 0 */
#define	AT91_UDP_CSR_RXSETUP     (0x1 <<  2)	/* Sends STALL to the Host
						 * (Control endpoints) */
#define	AT91_UDP_CSR_ISOERROR    (0x1 <<  3)	/* Isochronous error
						 * (Isochronous endpoints) */
#define	AT91_UDP_CSR_STALLSENT   (0x1 <<  3)	/* Stall sent (Control, bulk,
						 * interrupt endpoints) */
#define	AT91_UDP_CSR_TXPKTRDY    (0x1 <<  4)	/* Transmit Packet Ready */
#define	AT91_UDP_CSR_FORCESTALL  (0x1 <<  5)	/* Force Stall (used by
						 * Control, Bulk and
						 * Isochronous endpoints). */
#define	AT91_UDP_CSR_RX_DATA_BK1 (0x1 <<  6)	/* Receive Data Bank 1 (only
						 * used by endpoints with
						 * ping-pong attributes). */
#define	AT91_UDP_CSR_DIR         (0x1 <<  7)	/* Transfer Direction */
#define	AT91_UDP_CSR_ET_MASK     (0x7 <<  8)	/* Endpoint transfer type mask */
#define	AT91_UDP_CSR_ET_CTRL     (0x0 <<  8)	/* Control IN+OUT */
#define	AT91_UDP_CSR_ET_ISO      (0x1 <<  8)	/* Isochronous */
#define	AT91_UDP_CSR_ET_BULK     (0x2 <<  8)	/* Bulk */
#define	AT91_UDP_CSR_ET_INT      (0x3 <<  8)	/* Interrupt */
#define	AT91_UDP_CSR_ET_DIR_OUT  (0x0 <<  8)	/* OUT tokens */
#define	AT91_UDP_CSR_ET_DIR_IN   (0x4 <<  8)	/* IN tokens */
#define	AT91_UDP_CSR_DTGLE       (0x1 << 11)	/* Data Toggle */
#define	AT91_UDP_CSR_EPEDS       (0x1 << 15)	/* Endpoint Enable Disable */
#define	AT91_UDP_CSR_RXBYTECNT   (0x7FF << 16)	/* Number Of Bytes Available
						 * in the FIFO */

#define	AT91_UDP_FDR(n) (0x50 + (4*(n)))/* Endpoint FIFO Data Register */
#define	AT91_UDP_RES3	0x70		/* Reserved 3 */
#define	AT91_UDP_TXVC	0x74		/* Transceiver Control Register */
#define	AT91_UDP_TXVC_DIS      (0x1 <<  8)

#define	AT91_UDP_EP_MAX 6		/* maximum number of endpoints
					 * supported */

#define	AT91_UDP_READ_4(sc, reg) \
  bus_space_read_4((sc)->sc_io_tag, (sc)->sc_io_hdl, reg)

#define	AT91_UDP_WRITE_4(sc, reg, data)	\
  bus_space_write_4((sc)->sc_io_tag, (sc)->sc_io_hdl, reg, data)

struct at91dci_td;

typedef uint8_t (at91dci_cmd_t)(struct at91dci_td *td);

struct at91dci_td {
	bus_space_tag_t io_tag;
	bus_space_handle_t io_hdl;
	struct at91dci_td *obj_next;
	at91dci_cmd_t *func;
	struct usb2_page_cache *pc;
	uint32_t offset;
	uint32_t remainder;
	uint16_t max_packet_size;
	uint8_t	status_reg;
	uint8_t	fifo_reg;
	uint8_t	fifo_bank:1;
	uint8_t	error:1;
	uint8_t	alt_next:1;
	uint8_t	short_pkt:1;
	uint8_t	support_multi_buffer:1;
	uint8_t	did_stall:1;
};

struct at91dci_std_temp {
	at91dci_cmd_t *func;
	struct usb2_page_cache *pc;
	struct at91dci_td *td;
	struct at91dci_td *td_next;
	uint32_t len;
	uint32_t offset;
	uint16_t max_frame_size;
	uint8_t	short_pkt;
	/*
         * short_pkt = 0: transfer should be short terminated
         * short_pkt = 1: transfer should not be short terminated
         */
	uint8_t	setup_alt_next;
};

struct at91dci_config_desc {
	struct usb2_config_descriptor confd;
	struct usb2_interface_descriptor ifcd;
	struct usb2_endpoint_descriptor endpd;
} __packed;

union at91dci_hub_temp {
	uWord	wValue;
	struct usb2_port_status ps;
};

struct at91dci_ep_flags {
	uint8_t	fifo_bank:1;		/* hardware specific */
};

struct at91dci_flags {
	uint8_t	change_connect:1;
	uint8_t	change_suspend:1;
	uint8_t	status_suspend:1;	/* set if suspended */
	uint8_t	status_vbus:1;		/* set if present */
	uint8_t	status_bus_reset:1;	/* set if reset complete */
	uint8_t	remote_wakeup:1;
	uint8_t	self_powered:1;
	uint8_t	clocks_off:1;
	uint8_t	port_powered:1;
	uint8_t	port_enabled:1;
	uint8_t	d_pulled_up:1;
};

struct at91dci_softc {
	struct usb2_bus sc_bus;
	union at91dci_hub_temp sc_hub_temp;
	LIST_HEAD(, usb2_xfer) sc_interrupt_list_head;
	struct usb2_sw_transfer sc_root_ctrl;
	struct usb2_sw_transfer sc_root_intr;
	struct usb2_config_td sc_config_td;

	struct resource *sc_io_res;
	struct resource *sc_irq_res;
	void   *sc_intr_hdl;
	bus_size_t sc_io_size;
	bus_space_tag_t sc_io_tag;
	bus_space_handle_t sc_io_hdl;

	void    (*sc_clocks_on) (void *arg);
	void    (*sc_clocks_off) (void *arg);
	void   *sc_clocks_arg;

	void    (*sc_pull_up) (void *arg);
	void    (*sc_pull_down) (void *arg);
	void   *sc_pull_arg;

	uint8_t	sc_rt_addr;		/* root HUB address */
	uint8_t	sc_dv_addr;		/* device address */
	uint8_t	sc_conf;		/* root HUB config */

	uint8_t	sc_hub_idata[1];

	struct at91dci_flags sc_flags;
	struct at91dci_ep_flags sc_ep_flags[AT91_UDP_EP_MAX];
};

/* prototypes */

usb2_error_t at91dci_init(struct at91dci_softc *sc);
void	at91dci_uninit(struct at91dci_softc *sc);
void	at91dci_suspend(struct at91dci_softc *sc);
void	at91dci_resume(struct at91dci_softc *sc);
void	at91dci_interrupt(struct at91dci_softc *sc);

#endif					/* _AT9100_DCI_H_ */
