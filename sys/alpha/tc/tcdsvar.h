/* $FreeBSD$ */
/*	$NetBSD: tcdsvar.h,v 1.3.4.1 1996/09/10 17:28:20 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

struct tcds_slotconfig {
	/*
	 * Bookkeeping information
	 */
	int	sc_slot;
	struct tcds_softc *sc_tcds;		/* to frob TCDS regs */
	struct esp_softc *sc_esp;		/* to frob child's regs */
	void	(*sc_intrhand)(void *);	/* intr. handler */
	void	*sc_intrarg;			/* intr. handler arg. */

	/*
	 * Sets of bits in TCDS CIR and IMER that enable/check
	 * various things.
	 */
	u_int32_t sc_resetbits;
	u_int32_t sc_intrmaskbits;
	u_int32_t sc_intrbits;
	u_int32_t sc_dmabits;
	u_int32_t sc_errorbits;

	/*
	 * Pointers to slot-specific DMA resources.
	 */
	volatile u_int32_t *sc_sda;
	volatile u_int32_t *sc_dic;
	volatile u_int32_t *sc_dud0;
	volatile u_int32_t *sc_dud1;

	/*
	 * DMA bookkeeping information.
	 */
	int	sc_active;                      /* DMA active ? */
	int	sc_iswrite;			/* DMA into main memory? */
	size_t	sc_dmasize;
	caddr_t	*sc_dmaaddr;
	size_t	*sc_dmalen;
};

struct tcdsdev_attach_args {
	struct tc_attach_args tcdsda_ta;
	struct tcds_slotconfig *tcdsda_sc;
	u_int	tcdsda_id;
	u_int	tcdsda_freq;
};
#define	tcdsda_modname	tcdsda_ta.ta_modname
#define	tcdsda_slot	tcdsda_ta.ta_slot
#define	tcdsda_offset	tcdsda_ta.ta_offset
#define	tcdsda_addr	tcdsda_ta.ta_addr
#define	tcdsda_cookie	tcdsda_ta.ta_cookie

#define	TCDS_REG(base, off) \
    (volatile u_int32_t *)TC_DENSE_TO_SPARSE((base) + (off))

/*
 * TCDS functions.
 */
void	tcds_intr_establish(device_t, void *, tc_intrlevel_t,
	    int (*)(void *), void *);
void	tcds_intr_disestablish(device_t, void *);
void	tcds_dma_enable(struct tcds_slotconfig *, int);
void	tcds_scsi_enable(struct tcds_slotconfig *, int);
int	tcds_scsi_isintr(struct tcds_slotconfig *, int);
void	tcds_scsi_reset(struct tcds_slotconfig *);
int	tcds_scsi_iserr(struct tcds_slotconfig *sc);

/*
 * TCDS DMA functions (used the the 53c94 driver)
 */
int	tcds_dma_isintr	(struct tcds_slotconfig *);
void	tcds_dma_reset(struct tcds_slotconfig *);
int	tcds_dma_intr(struct tcds_slotconfig *);
int	tcds_dma_setup(struct tcds_slotconfig *, caddr_t *, size_t *,
	    int, size_t *);
void	tcds_dma_go(struct tcds_slotconfig *);
int	tcds_dma_isactive(struct tcds_slotconfig *);

/*
 * The TCDS (bus) cfdriver, so that subdevices can more
 * easily tell what bus they're on.
 */
extern struct cfdriver tcds_cd;
