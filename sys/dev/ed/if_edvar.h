/*-
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

#ifndef SYS_DEV_ED_IF_EDVAR_H
#define SYS_DEV_ED_IF_EDVAR_H
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
	device_t miibus;	/* MII bus for cards with MII. */
	void	(*mii_writebits)(struct ed_softc *, u_int, int);
	u_int	(*mii_readbits)(struct ed_softc *, int);
	struct callout_handle tick_ch; /* Callout handle for ed_tick */

	int	nic_offset;	/* NIC (DS8390) I/O bus address offset */
	int	asic_offset;	/* ASIC I/O bus address offset */

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
	caddr_t hpp_mem_start;	/* Memory-mapped IO register address */

	caddr_t mem_start;	/* NIC memory start address */
	caddr_t mem_end;		/* NIC memory end address */
	uint32_t mem_size;	/* total NIC memory size */
	caddr_t mem_ring;	/* start of RX ring-buffer (in NIC mem) */

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

#define	ed_nic_inb(sc, port) \
	bus_space_read_1(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), (sc)->nic_offset + (port))

#define	ed_nic_outb(sc, port, value) \
	bus_space_write_1(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), (sc)->nic_offset + (port), \
		(value))

#define	ed_nic_inw(sc, port) \
	bus_space_read_2(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), (sc)->nic_offset + (port))

#define	ed_nic_outw(sc, port, value) \
	bus_space_write_2(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), (sc)->nic_offset + (port), \
		(value))

#define	ed_nic_insb(sc, port, addr, count) \
	bus_space_read_multi_1(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), \
		(sc)->nic_offset + (port), (addr), (count))

#define	ed_nic_outsb(sc, port, addr, count) \
	bus_space_write_multi_1(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), \
		(sc)->nic_offset + (port), (addr), (count))

#define	ed_nic_insw(sc, port, addr, count) \
	bus_space_read_multi_2(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), \
		(sc)->nic_offset + (port), (uint16_t *)(addr), (count))

#define	ed_nic_outsw(sc, port, addr, count) \
	bus_space_write_multi_2(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), \
		(sc)->nic_offset + (port), (uint16_t *)(addr), (count))

#define	ed_nic_insl(sc, port, addr, count) \
	bus_space_read_multi_4(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), \
		(sc)->nic_offset + (port), (uint32_t *)(addr), (count))

#define	ed_nic_outsl(sc, port, addr, count) \
	bus_space_write_multi_4(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), \
		(sc)->nic_offset + (port), (uint32_t *)(addr), (count))

#define	ed_asic_inb(sc, port) \
	bus_space_read_1(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), (sc)->asic_offset + (port))

#define	ed_asic_outb(sc, port, value) \
	bus_space_write_1(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), (sc)->asic_offset + (port), \
		(value))

#define	ed_asic_inw(sc, port) \
	bus_space_read_2(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), (sc)->asic_offset + (port))

#define	ed_asic_outw(sc, port, value) \
	bus_space_write_2(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), (sc)->asic_offset + (port), \
		(value))

#define	ed_asic_insb(sc, port, addr, count) \
	bus_space_read_multi_1(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), \
		(sc)->asic_offset + (port), (addr), (count))

#define	ed_asic_outsb(sc, port, addr, count) \
	bus_space_write_multi_1(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), \
		(sc)->asic_offset + (port), (addr), (count))

#define	ed_asic_insw(sc, port, addr, count) \
	bus_space_read_multi_2(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), \
		(sc)->asic_offset + (port), (uint16_t *)(addr), (count))

#define	ed_asic_outsw(sc, port, addr, count) \
	bus_space_write_multi_2(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), \
		(sc)->asic_offset + (port), (uint16_t *)(addr), (count))

#define	ed_asic_insl(sc, port, addr, count) \
	bus_space_read_multi_4(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), \
		(sc)->asic_offset + (port), (uint32_t *)(addr), (count))

#define	ed_asic_outsl(sc, port, addr, count) \
	bus_space_write_multi_4(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), \
		(sc)->asic_offset + (port), (uint32_t *)(addr), (count))

void	ed_release_resources(device_t);
int	ed_alloc_port(device_t, int, int);
int	ed_alloc_memory(device_t, int, int);
int	ed_alloc_irq(device_t, int, int);

int	ed_probe_generic8390(struct ed_softc *);
int	ed_probe_WD80x3(device_t, int, int);
int	ed_probe_WD80x3_generic(device_t, int, uint16_t *[]);
int	ed_probe_3Com(device_t, int, int);
int	ed_probe_SIC(device_t, int, int);
int	ed_probe_Novell(device_t, int, int);
int	ed_probe_Novell_generic(device_t, int);
int	ed_probe_HP_pclanp(device_t, int, int);

int	ed_attach(device_t);
int	ed_detach(device_t);
int	ed_clear_memory(device_t);
void	ed_stop(struct ed_softc *);
void	ed_pio_readmem(struct ed_softc *, long, uint8_t *, uint16_t);
void	ed_pio_writemem(struct ed_softc *, uint8_t *, uint16_t, uint16_t);
#ifndef ED_NO_MIIBUS
int	ed_miibus_readreg(device_t, int, int);
void	ed_miibus_writereg(device_t, int, int, int);
int	ed_ifmedia_upd(struct ifnet *);
void	ed_ifmedia_sts(struct ifnet *, struct ifmediareq *);
void	ed_child_detached(device_t, device_t);
#endif

driver_intr_t	edintr;

extern devclass_t ed_devclass;
#endif /* SYS_DEV_ED_IF_EDVAR_H */
