/*	$NetBSD: i82365var.h,v 1.8 1999/10/15 06:07:27 haya Exp $	*/
/* $FreeBSD$ */

/*
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <dev/pccard/pccardreg.h>

#include <dev/pcic/i82365reg.h>

struct proc;

struct pcic_event {
	STAILQ_ENTRY(pcic_event) pe_q;
	int pe_type;
};

/* pe_type */
#define	PCIC_EVENT_INSERTION	0
#define	PCIC_EVENT_REMOVAL	1

struct proc;

struct pcic_handle {
	void *sc;
	device_t dev;
	bus_space_tag_t ph_bus_t;	/* I/O or MEM?  I don't mind */
	bus_space_handle_t ph_bus_h;
	u_int8_t (*ph_read)(struct pcic_handle*, int);
	void (*ph_write)(struct pcic_handle *, int, u_int8_t);

	int	vendor;
	int	sock;
	int	flags;
	int	laststate;
	int	memalloc;
	struct pccard_mem_handle mem[PCIC_MEM_WINS];	/* XXX BAD XXX */
	int	ioalloc;
	struct pccard_io_handle io[PCIC_IO_WINS];	/* XXX BAD XXX */
	int	ih_irq;

	int shutdown;
	struct proc *event_thread;
	STAILQ_HEAD(, pcic_event) events;
};

#define	PCIC_FLAG_SOCKETP	0x0001
#define	PCIC_FLAG_CARDP		0x0002

#define PCIC_LASTSTATE_PRESENT	0x0002
#define PCIC_LASTSTATE_HALF		0x0001
#define PCIC_LASTSTATE_EMPTY	0x0000

#define	C0SA PCIC_CHIP0_BASE+PCIC_SOCKETA_INDEX
#define	C0SB PCIC_CHIP0_BASE+PCIC_SOCKETB_INDEX
#define	C1SA PCIC_CHIP1_BASE+PCIC_SOCKETA_INDEX
#define	C1SB PCIC_CHIP1_BASE+PCIC_SOCKETB_INDEX

/*
 * This is sort of arbitrary.  It merely needs to be "enough". It can be
 * overridden in the conf file, anyway.
 */

#define	PCIC_MEM_PAGES	4
#define	PCIC_MEMSIZE	PCIC_MEM_PAGES*PCIC_MEM_PAGESIZE

#define	PCIC_NSLOTS	4

struct pcic_softc {
	device_t dev;

	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	void		*intrhand;
	struct resource *irq_res;
	int		irq_rid;
	struct resource *mem_res;
	int		mem_rid;
	struct resource	*port_res;
	int		port_rid;

#define PCIC_MAX_MEM_PAGES	(8 * sizeof(int))

	/* used by memory window mapping functions */
	bus_addr_t membase;

	/*
	 * used by io window mapping functions.  These can actually overlap
	 * with another pcic, since the underlying extent mapper will deal
	 * with individual allocations.  This is here to deal with the fact
	 * that different busses have different real widths (different pc
	 * hardware seems to use 10 or 12 bits for the I/O bus).
	 */
	bus_addr_t iobase;
	bus_addr_t iosize;

	int	irq;
	void	*ih;

	struct pcic_handle handle[PCIC_NSLOTS];
};


int	pcic_ident_ok(int);
int	pcic_vendor(struct pcic_handle *);
char	*pcic_vendor_to_string(int);

int	pcic_attach(device_t dev);

#define pcic_read(h, idx) 	(*(h)->ph_read)((h), (idx))
#define pcic_write(h, idx, data) (*(h)->ph_write)((h), (idx), (data))

/*
 * bus/device/etc routines
 */
int pcic_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r);
struct resource *pcic_alloc_resource(device_t dev, device_t child, int type,
    int *rid, u_long start, u_long end, u_long count, u_int flags);
void pcic_deactivate(device_t dev);
int pcic_deactivate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r);
int pcic_detach(device_t dev);
int pcic_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r);
int pcic_setup_intr(device_t dev, device_t child, struct resource *irq,
    int flags, driver_intr_t intr, void *arg, void **cookiep);
int pcic_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookiep);
int pcic_suspend(device_t dev);
int pcic_resume(device_t dev);
int pcic_enable_socket(device_t dev, device_t child);
int pcic_disable_socket(device_t dev, device_t child);
int pcic_set_res_flags(device_t dev, device_t child, int type, int rid, 
    u_int32_t flags);
int pcic_set_memory_offset(device_t dev, device_t child, int rid,
    u_int32_t offset);

#define PCIC_SOFTC(d) (struct pcic_softc *) device_get_softc(d)
