/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	from:	@(#)fd.c	7.4 (Berkeley) 5/25/91
 * $FreeBSD$
 *
 */

enum fdc_type
{
	FDC_NE765, FDC_I82077, FDC_NE72065, FDC_UNKNOWN = -1
};

enum fdc_states {
	DEVIDLE,
	FINDWORK,
	DOSEEK,
	SEEKCOMPLETE ,
	IOCOMPLETE,
	RECALCOMPLETE,
	STARTRECAL,
	RESETCTLR,
	SEEKWAIT,
	RECALWAIT,
	MOTORWAIT,
	IOTIMEDOUT,
	RESETCOMPLETE,
	PIOREAD
};

/***********************************************************************\
* Per controller structure.						*
\***********************************************************************/
struct fdc_data
{
	int	fdcu;		/* our unit number */
	int	dmachan;
	int	flags;
#define FDC_ATTACHED	0x01
#define FDC_STAT_VALID	0x08
#define FDC_HAS_FIFO	0x10
#define FDC_NEEDS_RESET	0x20
#define FDC_NODMA	0x40
#define FDC_ISPNP	0x80
#define FDC_ISPCMCIA	0x100
	struct	fd_data *fd;
	int	fdu;		/* the active drive	*/
	enum	fdc_states state;
	int	retry;
#ifndef PC98
	int	fdout;		/* mirror of the w/o digital output reg */
#endif
	u_int	status[7];	/* copy of the registers */
	enum	fdc_type fdct;	/* chip version of FDC */
	int	fdc_errs;	/* number of logged errors */
	int	dma_overruns;	/* number of DMA overruns */
	struct	bio_queue_head head;
	struct	bio *bp;	/* active buffer */
#ifdef PC98
	struct	resource *res_ioport, *res_fdsio, *res_fdemsio;
	struct	resource *res_irq, *res_drq;
	int	rid_ioport, rid_irq, rid_drq;
#else
	struct	resource *res_ioport, *res_ctl, *res_irq, *res_drq;
	int	rid_ioport, rid_ctl, rid_irq, rid_drq;
#endif
	int	port_off;
	bus_space_tag_t portt;
	bus_space_handle_t porth;
#ifdef PC98
        bus_space_tag_t		sc_fdsiot;
        bus_space_handle_t	sc_fdsioh;
        bus_space_tag_t		sc_fdemsiot;
        bus_space_handle_t	sc_fdemsioh;
#else
	bus_space_tag_t ctlt;
	bus_space_handle_t ctlh;
#endif
	void	*fdc_intr;
	struct	device *fdc_dev;
#ifndef PC98
	void	(*fdctl_wr)(struct fdc_data *fdc, u_int8_t v);
#endif
};

/***********************************************************************\
* Throughout this file the following conventions will be used:		*
* fd is a pointer to the fd_data struct for the drive in question	*
* fdc is a pointer to the fdc_data struct for the controller		*
* fdu is the floppy drive unit number					*
* fdcu is the floppy controller unit number				*
* fdsu is the floppy drive unit number on that controller. (sub-unit)	*
\***********************************************************************/
typedef int	fdu_t;
typedef int	fdcu_t;
typedef int	fdsu_t;
typedef	struct fd_data *fd_p;
typedef struct fdc_data *fdc_p;
typedef enum fdc_type fdc_t;

#define FDUNIT(s)	(((s) >> 6) & 3)
#define FDTYPE(s)	((s) & 0x3f)

/*
 * fdc maintains a set (1!) of ivars per child of each controller.
 */
enum fdc_device_ivars {
	FDC_IVAR_FDUNIT,
};

/*
 * Simple access macros for the ivars.
 */
#define FDC_ACCESSOR(A, B, T)						\
static __inline T fdc_get_ ## A(device_t dev)				\
{									\
	uintptr_t v;							\
	BUS_READ_IVAR(device_get_parent(dev), dev, FDC_IVAR_ ## B, &v);	\
	return (T) v;							\
}
FDC_ACCESSOR(fdunit,	FDUNIT,	int)
