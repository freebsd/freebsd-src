/*
 *       Copyright (c) 1997 by Matthew N. Dodd <winter@jurai.net>
 *       All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * Credits:  Based on and part of the DPT driver for FreeBSD written and
 *           maintained by Simon Shapiro <shimon@simon-shapiro.org>
 */

/*
 * $FreeBSD$
 */

#include "eisa.h"
#if NEISA > 0
#include "opt_dpt.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/kernel.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <cam/scsi/scsi_all.h>

#include <dev/dpt/dpt.h>

#include <i386/eisa/eisaconf.h>
#include <i386/eisa/dpt_eisa.h>

#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

/* Function Prototypes */

static int dpt_eisa_probe(void);
static int dpt_eisa_attach(struct eisa_device*);

static const char	*dpt_eisa_match(eisa_id_t);

static struct eisa_driver dpt_eisa_driver = 
{
	"dpt",
	dpt_eisa_probe,
	dpt_eisa_attach,
	NULL,
	&dpt_unit
};

DATA_SET (eisadriver_set, dpt_eisa_driver);

static int
dpt_eisa_probe(void)
{
	struct eisa_device *e_dev = NULL;
        int		    count;
	u_int32_t	    io_base;
	u_int		    intdef;
	u_int		    irq;

	e_dev = NULL;
	count = 0;
	while ((e_dev = eisa_match_dev(e_dev, dpt_eisa_match))) {
		io_base = (e_dev->ioconf.slot * EISA_SLOT_SIZE)
			 + DPT_EISA_SLOT_OFFSET;

		eisa_add_iospace(e_dev, io_base, 
				 DPT_EISA_IOSIZE, RESVADDR_NONE);
	
		intdef =  inb(DPT_EISA_INTDEF + io_base);

		irq = intdef & DPT_EISA_INT_NUM_MASK;
		switch (irq) {
		case DPT_EISA_INT_NUM_11:
			irq = 11;
			break;
		case DPT_EISA_INT_NUM_15:
			irq = 15;
			break;
		case DPT_EISA_INT_NUM_14:
			irq = 14;
			break;
		default:
			printf("dpt at slot %d: illegal irq setting %d\n",
			       e_dev->ioconf.slot, irq);
			irq = 0;
			break;
		}
		if (irq == 0)
			continue;

		eisa_add_intr(e_dev, irq);
		eisa_registerdev(e_dev, &dpt_eisa_driver);
		count++;
	}
	return count;
}

int
dpt_eisa_attach(e_dev)
	struct eisa_device	*e_dev;
{
	dpt_softc_t	*dpt;
	resvaddr_t	*io_space;
        int		unit = e_dev->unit;
        int		irq;
	int		shared;
	int		s;

	if (TAILQ_FIRST(&e_dev->ioconf.irqs) == NULL) {
		printf("dpt%d: Can't retrieve irq from EISA config struct.\n", 
			unit);
		return -1;
	}

	irq = TAILQ_FIRST(&e_dev->ioconf.irqs)->irq_no;
	io_space = e_dev->ioconf.ioaddrs.lh_first;

	if (!io_space) {
		printf("dpt%d: No I/O space?!\n", unit);
		return -1;
	}

	shared = inb(DPT_EISA_INTDEF + io_space->addr) & DPT_EISA_INT_LEVEL;

	dpt = dpt_alloc(unit, I386_BUS_SPACE_IO,
			io_space->addr + DPT_EISA_EATA_REG_OFFSET);
	if (dpt == NULL)
		return -1;

	/* Allocate a dmatag representing the capabilities of this attachment */
	/* XXX Should be a child of the EISA bus dma tag */
	if (bus_dma_tag_create(/*parent*/NULL, /*alignemnt*/1, /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       /*maxsize*/BUS_SPACE_MAXSIZE_32BIT,
			       /*nsegments*/BUS_SPACE_UNRESTRICTED,
			       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       /*flags*/0, &dpt->parent_dmat) != 0) {
		dpt_free(dpt);
		return -1;
	}

	if (eisa_reg_intr(e_dev, irq, dpt_intr, (void *)dpt, &cam_imask,  
			  shared)) {
                printf("dpt%d: eisa_reg_intr() failed.\n", unit);
		dpt_free(dpt);
		return -1;
        }
        eisa_reg_end(e_dev);
        
        /* Enable our interrupt handler. */
        if (eisa_enable_intr(e_dev, irq)) {
#ifdef DPT_DEBUG_ERROR
                printf("dpt%d: eisa_enable_intr() failed.\n", unit);
#endif          
                free(dpt, M_DEVBUF);
                eisa_release_intr(e_dev, irq, dpt_intr);
                return -1;
        }
	/*
	 * Enable our interrupt handler.
	 */
	if (eisa_enable_intr(e_dev, irq)) {
		dpt_free(dpt);
		eisa_release_intr(e_dev, irq, dpt_intr);
		return -1;
	}

	s = splcam();
	if (dpt_init(dpt) != 0) {
		dpt_free(dpt);
		return -1;
	}

	/* Register with the XPT */
	dpt_attach(dpt);
	splx(s);

	return 0;
}

static const char	*
dpt_eisa_match(type)
	eisa_id_t	type;
{
	switch (type) {
		case DPT_EISA_DPT2402 :
			return ("DPT PM2012A/9X");
			break;
		case DPT_EISA_DPTA401 :
			return ("DPT PM2012B/9X");
			break;
		case DPT_EISA_DPTA402 :
			return ("DPT PM2012B2/9X");
			break;
		case DPT_EISA_DPTA410 :
			return ("DPT PM2x22A/9X");
			break;
		case DPT_EISA_DPTA411 :
			return ("DPT Spectre");
			break;
		case DPT_EISA_DPTA412 :
			return ("DPT PM2021A/9X");
			break;
		case DPT_EISA_DPTA420 :
			return ("DPT Smart Cache IV (PM2042)");
			break;
		case DPT_EISA_DPTA501 :
			return ("DPT PM2012B1/9X");
			break;
		case DPT_EISA_DPTA502 :
			return ("DPT PM2012Bx/9X");
			break;
		case DPT_EISA_DPTA701 :
			return ("DPT PM2011B1/9X");
			break;
		case DPT_EISA_DPTBC01 :
			return ("DPT PM3011/7X ESDI");
			break;
		case DPT_EISA_NEC8200 :
			return ("NEC EATA SCSI");
			break;
		case DPT_EISA_ATT2408 :
			return ("ATT EATA SCSI");
			break;
		default:
			break;
	}
	
	return (NULL);
}

#endif /* NEISA > 0 */
