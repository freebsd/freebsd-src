/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)isa_device.h	7.1 (Berkeley) 5/9/91
 *	$Id: isa_device.h,v 1.32 1996/09/08 10:44:12 phk Exp $
 */

#ifndef _I386_ISA_ISA_DEVICE_H_
#define	_I386_ISA_ISA_DEVICE_H_

/*
 * ISA Bus Autoconfiguration
 */

#define	IDTVEC(name)	__CONCAT(X,name)

/*
 * Type of the first (asm) part of an interrupt handler.
 */
typedef void inthand_t __P((u_int cs, u_int ef, u_int esp, u_int ss));

/*
 * Usual type of the second (C) part of an interrupt handler.  Some bogus
 * ones need the arg to be the interrupt frame (and not a copy of it, which
 * is all that is possible in C).
 */
typedef void inthand2_t __P((int unit));

/*
 * Per device structure.
 *
 * XXX Note:  id_conflicts should either become an array of things we're
 * specifically allowed to conflict with or be subsumed into some
 * more powerful mechanism for detecting and dealing with multiple types
 * of non-fatal conflict.  -jkh XXX
 */
struct isa_device {
	int	id_id;		/* device id */
	struct	isa_driver *id_driver;
	int	id_iobase;	/* base i/o address */
	u_short	id_irq;		/* interrupt request */
	short	id_drq;		/* DMA request */
	caddr_t id_maddr;	/* physical i/o memory address on bus (if any)*/
	int	id_msize;	/* size of i/o memory */
	inthand2_t *id_intr;	/* interrupt interface routine */
	int	id_unit;	/* unit number */
	int	id_flags;	/* flags */
	int	id_scsiid;	/* scsi id if needed */
	int	id_alive;	/* device is present */
#define	RI_FAST		1		/* fast interrupt handler */
	u_int	id_ri_flags;	/* flags for register_intr() */
	int	id_reconfig;	/* hot eject device support (such as PCMCIA) */
	int	id_enabled;	/* is device enabled */
	int	id_conflicts;	/* we're allowed to conflict with things */
	struct isa_device *id_next; /* used in isa_devlist in userconfig() */
};

/*
 * Bits to specify the type and amount of conflict checking.
 */
#define CC_ATTACH       (1 << 0)
#define CC_DRQ          (1 << 1)
#define CC_IOADDR       (1 << 2)
#define CC_IRQ          (1 << 3)
#define CC_MEMADDR      (1 << 4)

/*
 * Per-driver structure.
 *
 * Each device driver defines entries for a set of routines
 * as well as an array of types which are acceptable to it.
 * These are used at boot time by the configuration program.
 */
struct isa_driver {
	int	(*probe) __P((struct isa_device *idp));
					/* test whether device is present */
	int	(*attach) __P((struct isa_device *idp));
					/* setup driver for a device */
	char	*name;			/* device name */
	int	sensitive_hw;		/* true if other probes confuse us */
};

#define ISA_EXTERNALLEN (sizeof(struct isa_device))

#ifdef KERNEL

extern char eintrnames[];	/* end of intrnames[] */
extern u_long intrcnt[];	/* counts for for each device and stray */
extern char intrnames[];	/* string table containing device names */
extern u_long *intr_countp[];	/* pointers into intrcnt[] */
extern inthand2_t *intr_handler[];	/* C entry points of intr handlers */
extern u_int intr_mask[];	/* sets of intrs masked during handling of 1 */
extern int intr_unit[];		/* cookies to pass to intr handlers */

extern struct isa_device isa_biotab_fdc[];
extern struct isa_device isa_biotab_wdc[];
extern struct isa_device isa_devtab_bio[];
extern struct isa_device isa_devtab_net[];
extern struct isa_device isa_devtab_null[];
extern struct isa_device isa_devtab_tty[];

inthand_t
	IDTVEC(fastintr0), IDTVEC(fastintr1),
	IDTVEC(fastintr2), IDTVEC(fastintr3),
	IDTVEC(fastintr4), IDTVEC(fastintr5),
	IDTVEC(fastintr6), IDTVEC(fastintr7),
	IDTVEC(fastintr8), IDTVEC(fastintr9),
	IDTVEC(fastintr10), IDTVEC(fastintr11),
	IDTVEC(fastintr12), IDTVEC(fastintr13),
	IDTVEC(fastintr14), IDTVEC(fastintr15);
inthand_t
	IDTVEC(intr0), IDTVEC(intr1), IDTVEC(intr2), IDTVEC(intr3),
	IDTVEC(intr4), IDTVEC(intr5), IDTVEC(intr6), IDTVEC(intr7),
	IDTVEC(intr8), IDTVEC(intr9), IDTVEC(intr10), IDTVEC(intr11),
	IDTVEC(intr12), IDTVEC(intr13), IDTVEC(intr14), IDTVEC(intr15);
struct isa_device *
	find_display __P((void));
struct isa_device *
	find_isadev __P((struct isa_device *table, struct isa_driver *driverp,
			 int unit));
int	haveseen_isadev __P((struct isa_device *dvp, u_int checkbits));
void	isa_configure __P((void));
void	isa_defaultirq __P((void));
void	isa_dmacascade __P((int chan));
void	isa_dmadone __P((int flags, caddr_t addr, int nbytes, int chan));
void	isa_dmainit __P((int chan, u_int bouncebufsize));
void	isa_dmastart __P((int flags, caddr_t addr, u_int nbytes, int chan));
int	isa_dma_acquire __P((int chan));
void	isa_dma_release __P((int chan));
int	isa_irq_pending __P((struct isa_device *dvp));
int	isa_nmi __P((int cd));
void	reconfig_isadev __P((struct isa_device *isdp, u_int *mp));
int	register_intr __P((int intr, int device_id, u_int flags,
			   inthand2_t *handler, u_int *maskptr, int unit));
int	unregister_intr __P((int intr, inthand2_t *handler));
int	update_intr_masks __P((void));

#endif /* KERNEL */

#endif /* !_I386_ISA_ISA_DEVICE_H_ */
