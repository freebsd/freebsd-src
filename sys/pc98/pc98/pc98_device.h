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
 *	$Id: isa_device.h,v 1.29 1996/04/08 19:38:57 smpatel Exp $
 */

#ifndef _PC98_PC98_PC98_DEVICE_H_
#define _PC98_PC98_PC98_DEVICE_H_

/*
 * PC98 Bus Autoconfiguration
 */
/*
 * modified for PC9801 by A.Kojima F.Ukai M.Ishii 
 *			Kyoto University Microcomputer Club (KMC)
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
 * Bits to specify the type and amount of conflict checking.
 */
#define CC_ATTACH       (1 << 0)
#define CC_DRQ          (1 << 1)
#define CC_IOADDR       (1 << 2)
#define CC_IRQ          (1 << 3)
#define CC_MEMADDR      (1 << 4)

/*
 * Per device structure.
 *
 * XXX Note:  id_conflicts should either become an array of things we're
 * specifically allowed to conflict with or be subsumed into some
 * more powerful mechanism for detecting and dealing with multiple types
 * of non-fatal conflict.  -jkh XXX
 */
struct pc98_device {
	int	id_id;		/* device id */
	struct	pc98_driver *id_driver;
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
	struct pc98_device *id_next; /* used in isa_devlist in userconfig() */
};

/*
 * Per-driver structure.
 *
 * Each device driver defines entries for a set of routines
 * as well as an array of types which are acceptable to it.
 * These are used at boot time by the configuration program.
 */
struct pc98_driver {
	int	(*probe) __P((struct pc98_device *idp));
					/* test whether device is present */
	int	(*attach) __P((struct pc98_device *idp));
					/* setup driver for a device */
	char	*name;			/* device name */
	int	sensitive_hw;		/* true if other probes confuse us */
};

#define PC98_EXTERNALLEN (sizeof(struct pc98_device))

#ifdef KERNEL

extern char eintrnames[];	/* end of intrnames[] */
extern u_long intrcnt[];	/* counts for for each device and stray */
extern char intrnames[];	/* string table containing device names */
extern u_long *intr_countp[];	/* pointers into intrcnt[] */
extern inthand2_t *intr_handler[];	/* C entry points of intr handlers */
extern u_int intr_mask[];	/* sets of intrs masked during handling of 1 */
extern int intr_unit[];		/* cookies to pass to intr handlers */

extern struct pc98_device pc98_biotab_fdc[];
extern struct pc98_device pc98_biotab_wdc[];
extern struct pc98_device pc98_devtab_bio[];
extern struct pc98_device pc98_devtab_net[];
extern struct pc98_device pc98_devtab_null[];
extern struct pc98_device pc98_devtab_tty[];
extern struct kern_devconf kdc_nec0;

struct kern_devconf;
struct sysctl_req;

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

struct pc98_device *
	find_display __P((void));
struct pc98_device *
	find_pc98dev __P((struct pc98_device *table, struct pc98_driver *driverp,
			 int unit));
int	haveseen_pc98dev __P((struct pc98_device *dvp, u_int checkbits));
void	pc98_configure __P((void));
void	pc98_defaultirq __P((void));
void	pc98_dmacascade __P((int chan));
void	pc98_dmadone __P((int flags, caddr_t addr, int nbytes, int chan));
void	pc98_dmainit __P((int chan, u_int bouncebufsize));
void	pc98_dmastart __P((int flags, caddr_t addr, u_int nbytes, int chan));
int	pc98_dma_acquire __P((int chan));
void	pc98_dma_release __P((int chan));
int	pc98_externalize __P((struct pc98_device *id, struct sysctl_req *req));
int	pc98_generic_externalize __P((struct kern_devconf *kdc,
				     struct sysctl_req *req));
int	pc98_internalize __P((struct pc98_device *id, struct sysctl_req *req));
int	pc98_irq_pending __P((struct pc98_device *dvp));
int	pc98_nmi __P((int cd));
void	reconfig_pc98dev __P((struct pc98_device *isdp, u_int *mp));
int	register_intr __P((int intr, int device_id, u_int flags,
			   inthand2_t *handler, u_int *maskptr, int unit));
int	unregister_intr __P((int intr, inthand2_t *handler));
int	update_intr_masks __P((void));

#endif /* KERNEL */

#ifdef PC98
#if 1
#define PC98_VECTOR_SIZE			(0x400)
#define PC98_SYSTEM_PARAMETER_SIZE		(0x230)

#define PC98_SAVE_AREA(highreso_flag)	(0xa1000)
#define PC98_SAVE_AREA_ADDRESS		(0x10)

#define OFS_BOOT_boothowto 0x210
#define OFS_BOOT_bootdev   0x214
#define OFS_BOOT_cyloffset 0x218
#define OFS_WD_BIOS_SECSIZE(i)	(0x200+(i)*6)
#define OFS_WD_BIOS_NCYL(i) (0x202+(i)*6)
#define OFS_WD_BIOS_HEAD(i) (0x205+(i)*6)
#define OFS_WD_BIOS_SEC(i) (0x204+(i)*6)
#define OFS_pc98_machine_type 0x220
#define OFS_epson_machine_id	0x224
#define OFS_epson_bios_id	0x225
#define OFS_epson_system_type	0x226

#define	M_NEC_PC98	0x0001
#define	M_EPSON_PC98	0x0002
#define	M_NOT_H98	0x0010
#define	M_H98		0x0020
#define	M_NOTE		0x0040
#define	M_NORMAL	0x1000
#define	M_HIGHRESO	0x2000
#define	M_8M		0x8000

# ifdef KERNEL

extern unsigned char	pc98_system_parameter[]; /* in locore.c */

#define PC98_SYSTEM_PARAMETER(x)	pc98_system_parameter[(x)-0x400]
#define BOOT_boothowto (*(unsigned long*)(&pc98_system_parameter[OFS_BOOT_boothowto]))
#define BOOT_bootdev   (*(unsigned long*)(&pc98_system_parameter[OFS_BOOT_bootdev]))
#define BOOT_cyloffset (*(unsigned long*)(&pc98_system_parameter[OFS_BOOT_cyloffset]))
#define WD_BIOS_SECSIZE(i) (*(unsigned short*)(&pc98_system_parameter[OFS_WD_BIOS_SECSIZE(i)]))
#define WD_BIOS_NCYL(i) (*(unsigned short*)(&pc98_system_parameter[OFS_WD_BIOS_NCYL(i)]))
#define WD_BIOS_HEAD(i) (pc98_system_parameter[OFS_WD_BIOS_HEAD(i)])
#define WD_BIOS_SEC(i) (pc98_system_parameter[OFS_WD_BIOS_SEC(i)])
#define pc98_machine_type (*(unsigned long*)&pc98_system_parameter[OFS_pc98_machine_type])
#define epson_machine_id	(pc98_system_parameter[OFS_epson_machine_id])
#define epson_bios_id		(pc98_system_parameter[OFS_epson_bios_id])
#define epson_system_type	(pc98_system_parameter[OFS_epson_system_type])

# define PC98_TYPE_CHECK(x)	((pc98_machine_type & (x)) == (x))

# endif /* KERNEL */

extern u_char	hireso;

#else
	/* OLD:386bsd-0.1-pc98-a&b */
extern unsigned char	pc98_system_parameter[0x214]; /* in pc98.c */
#define WD_BIOS_NCYL(i) (*(unsigned short*)(&pc98_system_parameter[0x206+(i)*6]))
#define WD_BIOS_HEAD(i) (pc98_system_parameter[0x209+(i)*6])
#define WD_BIOS_SEC(i) (pc98_system_parameter[0x208+(i)*6])
#define PC98_SYSTEM_PARAMETER(x)	pc98_system_parameter[(x)-0x400]
#define	pc98_machine_type	(pc98_system_parameter[0x210])
#define	NEC_PC98	1
#define	EPSON_PC98	2
#define epson_machine_id	(pc98_system_parameter[0x211])
#endif
#endif

#endif /* !_PC98_PC98_PC98_DEVICE_H_ */
