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
 *	$Id: isa_device.h,v 1.50 1998/06/17 16:54:22 bde Exp $
 */

#ifndef _I386_ISA_ISA_DEVICE_H_
#define	_I386_ISA_ISA_DEVICE_H_

/*
 * ISA Bus Autoconfiguration
 */

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
	u_int	id_irq;		/* interrupt request */
	int	id_drq;		/* DMA request */
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

#ifdef KERNEL

extern struct isa_device isa_biotab_fdc[];
extern struct isa_device isa_biotab_wdc[];
extern struct isa_device isa_devtab_bio[];
extern struct isa_device isa_devtab_net[];
extern struct isa_device isa_devtab_cam[];
extern struct isa_device isa_devtab_null[];
extern struct isa_device isa_devtab_tty[];

struct isa_device *
	find_display __P((void));
struct isa_device *
	find_isadev __P((struct isa_device *table, struct isa_driver *driverp,
			 int unit));
int	haveseen_isadev __P((struct isa_device *dvp, u_int checkbits));
void	isa_configure __P((void));
void	isa_dmacascade __P((int chan));
void	isa_dmadone __P((int flags, caddr_t addr, int nbytes, int chan));
void	isa_dmainit __P((int chan, u_int bouncebufsize));
void	isa_dmastart __P((int flags, caddr_t addr, u_int nbytes, int chan));
int	isa_dma_acquire __P((int chan));
void	isa_dma_release __P((int chan));
int	isa_dmastatus __P((int chan));
int	isa_dmastop __P((int chan));
void	reconfig_isadev __P((struct isa_device *isdp, u_int *mp));

typedef	void	ointhand2_t __P((int unit));

/*
 * The "old" interrupt handlers really have type ointhand2_t although they
 * appear to be declared as having type inthand2_t.  However, if if this
 * header is included by ioconf.c, pretend that the handlers really have
 * type inthand_t.  Assume that `C' is defined only by ioconf.c.
 */
#ifndef C
#define	inthand2_t	ointhand2_t
#endif

inthand2_t	adintr;
inthand2_t	ahaintr;
inthand2_t	aicintr;
inthand2_t	alogintr;
inthand2_t	arintr;
inthand2_t	ascintr;
#ifdef PC98
inthand2_t	bsintr;
#endif
inthand2_t	bt_isa_intr;
inthand2_t	cxintr;
inthand2_t	cyintr;
inthand2_t	edintr;
inthand2_t	egintr;
inthand2_t	elintr;
inthand2_t	epintr;
inthand2_t	exintr;
inthand2_t	fdintr;
inthand2_t	feintr;
inthand2_t	gusintr;
inthand2_t	ieintr;
inthand2_t	labpcintr;
inthand2_t	le_intr;
inthand2_t	lncintr;
inthand2_t	loranintr;
inthand2_t	lptintr;
inthand2_t	m6850intr;
inthand2_t	mcdintr;
inthand2_t	mseintr;
inthand2_t	ncaintr;
inthand2_t	npxintr;
inthand2_t	pasintr;
inthand2_t	pcmintr;
inthand2_t	pcrint;
inthand2_t	ppcintr;
inthand2_t	psmintr;
inthand2_t	rcintr;
inthand2_t	sbintr;
inthand2_t	scintr;
inthand2_t	seaintr;
inthand2_t	siointr;
inthand2_t	sndintr;
inthand2_t	spigintr;
inthand2_t	srintr;
inthand2_t	sscapeintr;
inthand2_t	stlintr;
inthand2_t	twintr;
inthand2_t	uhaintr;
inthand2_t	wdintr;
inthand2_t	wdsintr;
inthand2_t	wlintr;
inthand2_t	wtintr;
inthand2_t	zeintr;
inthand2_t	zpintr;

#undef inthand2_t

#endif /* KERNEL */

#endif /* !_I386_ISA_ISA_DEVICE_H_ */
