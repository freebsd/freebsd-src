/*-
 * Copyright (c) 2004 M. Warner Losh.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/pc98/cbus/fdcvar.h,v 1.3.24.1 2008/11/25 02:59:29 kensmith Exp $
 */

/* XXX should audit this file to see if additional copyrights needed */

enum fdc_type
{
	FDC_NE765, FDC_ENHANCED, FDC_UNKNOWN = -1
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

#ifdef	FDC_DEBUG
static char const * const fdstates[] = {
	"DEVIDLE",
	"FINDWORK",
	"DOSEEK",
	"SEEKCOMPLETE",
	"IOCOMPLETE",
	"RECALCOMPLETE",
	"STARTRECAL",
	"RESETCTLR",
	"SEEKWAIT",
	"RECALWAIT",
	"MOTORWAIT",
	"IOTIMEDOUT",
	"RESETCOMPLETE",
	"PIOREAD"
};
#endif

/*
 * Per controller structure (softc).
 */
struct fdc_data
{
	int	fdcu;		/* our unit number */
	int	dmacnt;
	int	dmachan;
	int	flags;
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

typedef int	fdu_t;
typedef int	fdcu_t;
typedef int	fdsu_t;
typedef	struct fd_data *fd_p;
typedef struct fdc_data *fdc_p;
typedef enum fdc_type fdc_t;

/* error returns for fd_cmd() */
#define FD_FAILED -1
#define FD_NOT_VALID -2
#define FDC_ERRMAX	100	/* do not log more */

extern devclass_t fdc_devclass;

enum fdc_device_ivars {
	FDC_IVAR_FDUNIT,
	FDC_IVAR_FDTYPE,
};

__BUS_ACCESSOR(fdc, fdunit, FDC, FDUNIT, int);
__BUS_ACCESSOR(fdc, fdtype, FDC, FDTYPE, int);

int fdc_alloc_resources(struct fdc_data *);
#ifndef PC98
void fdout_wr(fdc_p, u_int8_t);
#endif
int fd_cmd(struct fdc_data *, int, ...);
void fdc_release_resources(struct fdc_data *);
int fdc_attach(device_t);
int fdc_hints_probe(device_t);
int fdc_detach(device_t dev);
device_t fdc_add_child(device_t, const char *, int);
int fdc_initial_reset(struct fdc_data *);
int fdc_print_child(device_t, device_t);
int fdc_read_ivar(device_t, device_t, int, uintptr_t *);
int fdc_write_ivar(device_t, device_t, int, uintptr_t);
#ifndef PC98
int fdc_isa_alloc_resources(device_t, struct fdc_data *);
#endif
