/*
 *	Slot structures for PC-CARD interface.
 *	Each slot has a controller specific structure
 *	attached to it. A slot number allows
 *	mapping from the character device to the
 *	slot structure. This is separate to the
 *	controller slot number to allow multiple controllers
 *	to be accessed.
 *-------------------------------------------------------------------------
 *
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 *
 * $FreeBSD$
 */

#ifndef _PCCARD_SLOT_H
#define _PCCARD_SLOT_H

/*
 * Normally we shouldn't include stuff here, but we're trying to be
 * compatible with the long, dark hand of the past.
 */
#include <sys/param.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#if __FreeBSD_version >= 500000
#include <sys/selinfo.h>
#else
#include <sys/select.h>
#endif

/*
 *	Controller data - Specific to each slot controller.
 */
struct slot;
struct slot_ctrl {
	void	(*mapirq)(struct slot *, int);
				/* Map irq */
	int	(*mapmem)(struct slot *, int);
				/* Map memory */
	int	(*mapio)(struct slot *, int);
				/* Map io */
	void	(*reset)(void *);
				/* init */
	void	(*disable)(struct slot *);
				/* Disable slot */
	int	(*power)(struct slot *);
				/* Set power values */
	int	(*ioctl)(struct slot *, int, caddr_t);
				/* ioctl to lower level */
	void	(*resume)(struct slot *);
				/* suspend/resume support */
	int	maxmem;		/* Number of allowed memory windows */
	int	maxio;		/* Number of allowed I/O windows */
};

/*
 *	Device structure for cards. Each card may have one
 *	or more pccard drivers attached to it; each driver is assumed
 *	to require at most one interrupt handler, one I/O block
 *	and one memory block. This structure is used to link the different
 *	devices together.
 */
struct pccard_devinfo {
	u_char	name[128];
	int running;			/* Current state of driver */
	u_char	misc[128];		/* For any random info */
	struct slot *slt;		/* Back pointer to slot */

	struct resource_list resources;
};

/*
 *	Per-slot structure.
 */
struct slot {
	int slotnum;			/* Slot number */
	int flags;			/* Slot flags (see below) */
	int rwmem;			/* Read/write flags */
	int irq;			/* IRQ allocated (0 = none) */

	/*
	 *	flags.
	 */
	unsigned int 	insert_seq;	/* Firing up under the card */
	struct callout_handle insert_ch;/* Insert event timeout handle */
	struct callout_handle poff_ch;	/* Power Off timeout handle */

	enum cardstate 	state, laststate; /* Current/last card states */
	struct selinfo	selp;		/* Info for select */
	struct mem_desc	mem[NUM_MEM_WINDOWS];	/* Memory windows */
	struct io_desc	io[NUM_IO_WINDOWS];	/* I/O windows */
	struct power	pwr;		/* Power values */
	struct slot_ctrl *ctrl;		/* Per-controller data */
	void		*cdata;		/* Controller specific data */
	int		pwr_off_pending;/* Power status of slot */
	device_t	dev;		/* Config system device. */
	dev_t		d;		/* fs device */
};

#define PCCARD_DEVICE2SOFTC(d)	((struct slot *) device_get_softc(d))
#define PCCARD_DEV2SOFTC(d)	((struct slot *) (d)->si_drv1)

enum card_event { card_removed, card_inserted, card_deactivated };

struct slot	*pccard_init_slot(device_t, struct slot_ctrl *);
void		 pccard_event(struct slot *, enum card_event);
int		 pccard_suspend(device_t);
int		 pccard_resume(device_t);

#endif /* !_PCCARD_SLOT_H */
