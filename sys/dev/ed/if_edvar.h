/*
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 *
 * $FreeBSD$
 */

/*
 * ed_softc: per line info and status
 */
struct ed_softc {
	struct arpcom arpcom;	/* ethernet common */

	char   *type_str;	/* pointer to type string */
	u_char  vendor;		/* interface vendor */
	u_char  type;		/* interface type code */
	u_char	gone;		/* HW missing, presumed having a good time */

	int	port_rid;	/* resource id for port range */
	int	port_used;	/* nonzero if ports used */
	struct resource* port_res; /* resource for port range */
	int	mem_rid;	/* resource id for memory range */
	int	mem_used;	/* nonzero if memory used */
	struct resource* mem_res; /* resource for memory range */
	int	irq_rid;	/* resource id for irq */
	struct resource* irq_res; /* resource for irq */
	void*	irq_handle;	/* handle for irq handler */

	bus_space_tag_t		bst;	/* Bus Space tag */
	bus_space_handle_t	bsh;	/* Bus Space handle */

#ifdef __alpha__
	u_int asic_addr;	/* ASIC I/O bus address */
	u_int nic_addr;		/* NIC (DS8390) I/O bus address */
#else
	u_short asic_addr;	/* ASIC I/O bus address */
	u_short nic_addr;	/* NIC (DS8390) I/O bus address */
#endif

/*
 * The following 'proto' variable is part of a work-around for 8013EBT asics
 *	being write-only. It's sort of a prototype/shadow of the real thing.
 */
	u_char  wd_laar_proto;
	u_char	cr_proto;
	u_char  isa16bit;	/* width of access to card 0=8 or 1=16 */
	int	chip_type;	/* the type of chip (one of ED_CHIP_TYPE_*) */

/*
 * HP PC LAN PLUS card support.
 */

	u_short	hpp_options;	/* flags controlling behaviour of the HP card */
	u_short hpp_id;		/* software revision and other fields */
	u_long hpp_mem_start;	/* Memory-mapped IO register address */

	u_long mem_start;	/* NIC memory start address */
	u_long mem_end;		/* NIC memory end address */
	u_int32_t mem_size;	/* total NIC memory size */
	u_long mem_ring;	/* start of RX ring-buffer (in NIC mem) */

	u_char  mem_shared;	/* NIC memory is shared with host */
	u_char  xmit_busy;	/* transmitter is busy */
	u_char  txb_cnt;	/* number of transmit buffers */
	u_char  txb_inuse;	/* number of TX buffers currently in-use */

	u_char  txb_new;	/* pointer to where new buffer will be added */
	u_char  txb_next_tx;	/* pointer to next buffer ready to xmit */
	u_short txb_len[8];	/* buffered xmit buffer lengths */
	u_char  tx_page_start;	/* first page of TX buffer area */
	u_char  rec_page_start;	/* first page of RX ring-buffer */
	u_char  rec_page_stop;	/* last page of RX ring-buffer */
	u_char  next_packet;	/* pointer to next unread RX packet */
	struct	ifmib_iso_8802_3 mibdata; /* stuff for network mgmt */
};

void	ed_release_resources	__P((device_t));
int	ed_alloc_port		__P((device_t, int, int));
int	ed_alloc_memory		__P((device_t, int, int));
int	ed_alloc_irq		__P((device_t, int, int));

int	ed_probe_generic8390	__P((struct ed_softc *));
int	ed_probe_WD80x3		__P((device_t));
int	ed_probe_3Com		__P((device_t));
int	ed_probe_Novell		__P((device_t));
int	ed_probe_Novell_generic	__P((device_t, int, int));
int	ed_probe_HP_pclanp	__P((device_t));
int	ed_get_Linksys		__P((struct ed_softc *));
void	ed_ax88190_geteprom	__P((struct ed_softc *));

int	ed_attach		__P((struct ed_softc *, int, int));
void	ed_stop			__P((struct ed_softc *));

driver_intr_t	edintr;

