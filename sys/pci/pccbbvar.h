/*
 * Copyright (c) 1999 HAYAKAWA Koichi.  All rights reserved.
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
 *	This product includes software developed by HAYAKAWA Koichi.
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
/* $FreeBSD: src/sys/pci/pccbbvar.h,v 1.1 1999/11/18 07:14:54 imp Exp $ */

struct pccbb_softc;

struct cbb_pcic_handle {
    struct pccbb_softc *sc;
    bus_space_tag_t ph_iot;
    bus_space_handle_t ph_ioh;
    u_int8_t (* ph_read) __P((struct cbb_pcic_handle *, int));
    void (* ph_write) __P((struct cbb_pcic_handle *, int, u_int8_t));
    int	sock;

    int	vendor;
    int	flags;
    int	memalloc;
    struct {
	bus_addr_t	addr;
	bus_size_t	size;
	long	offset;
	int		kind;
    } mem[PCIC_MEM_WINS];
    int	ioalloc;
    struct {
	bus_addr_t	addr;
	bus_size_t	size;
	int		width;
    } io[PCIC_IO_WINS];
    int	ih_irq;
    struct device *pcmcia;

    int shutdown;
    struct proc *event_thread;
    SIMPLEQ_HEAD(, pcic_event) events;
};

struct pccbb_softc {
    struct device sc_dev;
    bus_space_tag_t sc_iot;
    bus_space_tag_t sc_memt;

    bus_space_tag_t sc_base_memt;
    bus_space_handle_t sc_base_memh;
    void *sc_ih;			/* interrupt handler */
    int sc_intrline;		/* interrupt line */
    pcitag_t sc_intrtag;		/* copy of pa->pa_intrtag */
    pci_intr_pin_t sc_intrpin;	/* copy of pa->pa_intrpin */
    int sc_function;
    u_int32_t sc_flags;
#define CBB_CARDEXIST 0x01
#define CBB_CARDSTATUS_BUSY 0x01000000
    pci_chipset_tag_t sc_pc;
    pcitag_t sc_tag;
    int sc_chipset;		/* chipset id */

    bus_addr_t sc_mem_start;	/* CardBus/PCMCIA memory start */
    bus_addr_t sc_mem_end;	/* CardBus/PCMCIA memory end */

    /* CardBus stuff */
    struct cardbus_softc *sc_csc;
    struct device *sc_cbdev; /* Device which attached to the slot XXX mfc */
    /* pcmcia stuff */
    struct cbb_pcic_handle sc_pcmcia_h;
    pcmcia_chipset_tag_t sc_pct;
    int sc_pcmcia_flags;
#define PCCBB_PCMCIA_IO_RELOC   0x01 /* IO address relocatable stuff exists */
#define PCCBB_PCMCIA_MEM_32     0x02 /* 32-bit memory address ready */
    /* kthread staff */
    int sc_queued;
    struct proc *event_thread;
    SIMPLEQ_HEAD(, pcic_event) events;
};
