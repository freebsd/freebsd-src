/*	$NecBSD: bsif.h,v 1.5 1997/10/23 20:52:34 honda Exp $	*/
/* $FreeBSD$ */
/*
 * Copyright (c) HONDA Naofumi, KATO Takenori, 1996.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
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

/***************************************************
 * misc device header in bs_softc
 ***************************************************/
#ifdef __NetBSD__
#define	OS_DEPEND_DEVICE_HEADER			\
	struct device sc_dev;			\
	void *sc_ih;

#define OS_DEPEND_SCSI_HEADER			\
	struct scsi_link sc_link;

#define	OS_DEPEND_MISC_HEADER			\
	pisa_device_handle_t sc_dh;		\
	bus_space_tag_t sc_iot;			\
	bus_space_tag_t sc_memt;		\
	bus_space_handle_t sc_ioh;		\
	bus_space_handle_t sc_delaybah;		\
	bus_space_handle_t sc_memh;		\
	bus_dma_tag_t sc_dmat;		

#endif	/* __NetBSD__ */
#ifdef __FreeBSD__
#define OS_DEPEND_DEVICE_HEADER			\
	int unit;

#define OS_DEPEND_SCSI_HEADER			\
	struct scsi_link sc_link;

#define	OS_DEPEND_MISC_HEADER			\
	struct callout_handle timeout_ch;
#endif	/* __FreeBSD__ */

#if	defined(__NetBSD__)
#define BSHW_NBPG	NBPG
#endif
#if	defined(__FreeBSD__)
#define BSHW_NBPG	PAGE_SIZE
#endif

/***************************************************
 * include
 ***************************************************/
/* (I) common include */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/disklabel.h>
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/errno.h>

#include <vm/vm.h>

/* (II) os depend include */
#ifdef	__NetBSD__
#include <sys/device.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>
#include <dev/isa/isadmavar.h>
#include <dev/isa/isadmareg.h>

#include <dev/cons.h>

#include <machine/cpufunc.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/dvcfg.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_disk.h>
#endif	/* __NetBSD__ */

#ifdef __FreeBSD__
#include <sys/conf.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/vmparam.h>
#include <machine/ipl.h>
#include <machine/dvcfg.h>

#include <cam/scsi/scsi_all.h>
#if 0
#include <cam/scsi/scsiconf.h>
#endif
#include <cam/scsi/scsi_da.h>

#include <pc98/pc98/pc98.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>
#endif	/* __FreeBSD__ */

/***************************************************
 * BUS IO MAPPINGS & BS specific inclusion
 ***************************************************/
#ifdef	__NetBSD__
#define	BUS_IO_DELAY ((void) bus_space_read_1(bsc->sc_iot, bsc->sc_delaybah, 0))
#define	BUS_IO_WEIGHT (bus_space_write_1(bsc->sc_iot, bsc->sc_delaybah, 0, 0))
#define	BUS_IOR(offs) (bus_space_read_1(bsc->sc_iot, bsc->sc_ioh, (offs)))
#define	BUS_IOW(offs, val) (bus_space_write_1(bsc->sc_iot, bsc->sc_ioh, (offs), (val)))

#include <dev/ic/wd33c93reg.h>
#include <dev/isa/ccbque.h>

#include <i386/Cbus/dev/scsi_dvcfg.h>
#include <i386/Cbus/dev/bs/bsvar.h>
#include <i386/Cbus/dev/bs/bshw.h>
#include <i386/Cbus/dev/bs/bsfunc.h>
#endif	/* __NetBSD__ */

#ifdef	__FreeBSD__
#define	BUS_IO_DELAY ((void) inb(0x5f))
#define	BUS_IO_WEIGHT (outb(0x5f, 0))
#define	BUS_IOR(offs) (BUS_IO_DELAY, inb(bsc->sc_iobase + (offs)))
#define	BUS_IOW(offs, val) (BUS_IO_DELAY, outb(bsc->sc_iobase + (offs), (val)))

#include <i386/isa/ic/wd33c93.h>
#include <i386/isa/ccbque.h>

#include <cam/scsi/scsi_dvcfg.h>
#include <i386/isa/bs/bsvar.h>
#include <i386/isa/bs/bshw.h>
#include <i386/isa/bs/bsfunc.h>
#endif	/* __FreeBSD__ */

/***************************************************
 * XS return type
 ***************************************************/
#ifdef	__NetBSD__
#define	XSBS_INT32T	int
#endif	/* __NetBSD__ */
#ifdef	__FreeBSD__
#define	XSBS_INT32T	int32_t
#endif	/* __FreeBSD__ */

/***************************************************
 * xs flags's abstraction (all currently used)
 ***************************************************/
#define	XSBS_ITSDONE	ITSDONE
#ifdef __NetBSD__
#define	XSBS_SCSI_NOSLEEP	SCSI_NOSLEEP
#define XSBS_SCSI_POLL	SCSI_POLL
#endif	/* __NetBSD__ */
#ifdef __FreeBSD__
#define XSBS_SCSI_POLL	SCSI_NOMASK
#endif	/* __FreeBSD__ */

/***************************************************
 * declare
 ***************************************************/
/* (I) common declare */
void bs_alloc_buf __P((struct targ_info *));
#ifdef __NetBSD__
XSBS_INT32T bs_target_open __P((struct scsi_link *, struct cfdata *));
XSBS_INT32T bs_scsi_cmd __P((struct scsi_xfer *));
#endif
#ifdef __FreeBSD__
void bs_scsi_cmd(struct cam_sim *sim, union ccb *ccb);
#endif
extern int delaycount;

/* (II) os depend declare */
#ifdef __NetBSD__
int bsintr __P((void *));
int bsprint __P((void *, const char *));
#endif	/* __NetBSD__ */

#ifdef __FreeBSD__
static BS_INLINE void memcopy __P((void *from, void *to, register size_t len));
u_int32_t bs_adapter_info __P((int));
#define delay(y) DELAY(y)
extern int dma_init_flag;
#ifdef SMP
#error XXX see comments in i386/isa/bs/bsif.h for details
/*
 * ipending is 'opaque' in SMP, and can't be accessed this way.
 * Since its my belief that this is PC98 code, and that PC98 and SMP
 * are mutually exclusive, the above compile-time error is the "fix".
 * Please inform smp@freebsd.org if this is NOT the case.
 */
#else
#define softintr(y) ipending |= (1 << y)
#endif /* SMP */

static BS_INLINE void
memcopy(from, to, len)
	void *from, *to;
	register size_t len;
{
	len >>= 2;
	__asm __volatile("			\n\
		cld				\n\
		rep				\n\
		movsl"				:
	    "=D" (to), "=c" (len), "=S" (from)	:
	    "0" (to), "1" (len), "2" (from)	:
	    "memory", "cc");
}
#endif	/* __FreeBSD__ */
