/*
 * Copyright (c) 2001 M. Warner Losh.  All Rights Reserved.
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
 *
 * $FreeBSD$
 */

/*
 *	Per-slot data table.
 */
struct pcic_slot {
	int offset;			/* Offset value for index */
	char controller;		/* Device type */
	char revision;			/* Device Revision */
	struct slot *slt;		/* Back ptr to slot */
	struct pcic_softc *sc;		/* Back pointer to softc */
	u_int8_t (*getb)(struct pcic_slot *, int);
	void   (*putb)(struct pcic_slot *, int, u_int8_t);
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	driver_intr_t *intr;
	void *argp;
};

enum pcic_irq_type { isa_parallel = 1, isa_serial, pci_parallel, pci_serial };

struct pcic_softc 
{
	u_int32_t		slotmask;/* Mask of valid slots */
	u_int32_t		flags;	/* Interesting flags */
#define PCIC_AB_POWER	0x00000001	/* Use old A/B step power */
#define PCIC_DF_POWER	0x00000002	/* Uses DF step regs  */
#define PCIC_PD_POWER	0x00000004	/* Uses CL-PD regs  */
#define	PCIC_VG_POWER	0x00000008	/* Uses VG power regs */
#define PCIC_KING_POWER	0x00000010	/* Uses IBM KING regs  */
#define PCIC_RICOH_POWER 0x0000020	/* Uses the ricoh power regs */
	enum pcic_irq_type	csc_route; /* How to route csc interrupts */
	enum pcic_irq_type	func_route; /* How to route function ints */
	int			iorid;	/* Rid of I/O region */
	struct resource 	*iores;	/* resource for I/O region */
	int			memrid;	/* Memory rid */
	struct resource		*memres;/* Resource for memory mapped regs */
	int			irqrid;	/* Irq rid */
	struct resource		*irqres;/* Irq resource */
	void			*ih;	/* Our interrupt handler. */
	int			irq;
	device_t		dev;	/* Our device */
	void (*slot_poll)(void *);
	struct callout_handle	timeout_ch;
	struct pcic_slot	slots[PCIC_MAX_SLOTS];
	int			cd_pending; /* debounce timeout active */
	int			cd_present; /* debounced card-present state */
	struct callout_handle	cd_ch;	/* handle for pcic_cd_insert */
};

extern devclass_t	pcic_devclass;
extern int		pcic_override_irq;

int pcic_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r);
struct resource *pcic_alloc_resource(device_t dev, device_t child, int type,
    int *rid, u_long start, u_long end, u_long count, u_int flags);
int pcic_attach(device_t dev);
void pcic_clrb(struct pcic_slot *sp, int reg, unsigned char mask);
int pcic_deactivate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r);
void pcic_dealloc(device_t dev);
void pcic_do_stat_delta(struct pcic_slot *sp);
int pcic_get_memory_offset(device_t bus, device_t child, int rid,
    u_int32_t *offset);
int pcic_get_res_flags(device_t bus, device_t child, int restype, int rid,
    u_long *value);
unsigned char pcic_getb_io(struct pcic_slot *sp, int reg);
driver_intr_t	pcic_isa_intr;
int		pcic_isa_intr1(void *);
void pcic_putb_io(struct pcic_slot *sp, int reg, unsigned char val);
int pcic_set_memory_offset(device_t bus, device_t child, int rid,
    u_int32_t offset
#if __FreeBSD_version >= 500000
    , u_int32_t *deltap
#endif
    );
int pcic_set_res_flags(device_t bus, device_t child, int restype, int rid,
    u_long value);
void pcic_setb(struct pcic_slot *sp, int reg, unsigned char mask);
int pcic_setup_intr(device_t dev, device_t child, struct resource *irq,
    int flags, driver_intr_t *intr, void *arg, void **cookiep);
int pcic_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie);
timeout_t 	pcic_timeout;
