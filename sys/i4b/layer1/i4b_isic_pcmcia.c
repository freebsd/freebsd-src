/*
 *   Copyright (c) 1998 Matthias Apitz. All rights reserved.
 *
 *   Copyright (c) 1998, 1999 Hellmuth Michaelis. All rights reserved. 
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b_isic_pcmcia.c - i4b FreeBSD PCMCIA support
 *	----------------------------------------------
 *
 * $FreeBSD: src/sys/i4b/layer1/i4b_isic_pcmcia.c,v 1.10 1999/10/29 17:28:09 imp Exp $
 *
 *      last edit-date: [Mon Apr 26 10:52:57 1999]
 *
 *---------------------------------------------------------------------------*/

#ifdef __FreeBSD__
#include "isic.h"
#include "opt_i4b.h"
#include "card.h"
#undef NCARD
#define NCARD 0

#if (NISIC > 0) && (NCARD > 0)
 
#include "apm.h"
#include <sys/types.h>
#include <sys/select.h>
#include <sys/param.h>
#include <i386/isa/isa_device.h>

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif

#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>
#include <machine/clock.h>
#include <i386/isa/isa_device.h>

#include <pccard/cardinfo.h>
#include <pccard/driver.h>
#include <pccard/slot.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_isac.h>
#include <i4b/layer1/i4b_hscx.h>

#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_global.h>

#ifdef __FreeBSD__

#if !(defined(__FreeBSD_version)) || (defined(__FreeBSD_version) && __FreeBSD_version >= 300006)
void isicintr ( int unit );
#else
extern void isicintr(int unit);
#endif

#endif
 
extern int isicattach(struct isa_device *dev);

/*  
 * PC-Card (PCMCIA) specific code.
 */
static int  isic_pccard_init    __P((struct pccard_devinfo *));
static void isic_unload         __P((struct pccard_devinfo *));
static int  isic_card_intr      __P((struct pccard_devinfo *));
    
#if defined(__FreeBSD__) && __FreeBSD__ < 3
static struct pccard_device isic_info = {
    "isic",
    isic_pccard_init,
    isic_unload,
    isic_card_intr,
    0,                      /* Attributes - presently unused */
    &net_imask
};      
  
DATA_SET(pccarddrv_set, isic_info);
#else
PCCARD_MODULE(isic, isic_pccard_init, isic_unload, isic_card_intr, 0,net_imask);
#endif


/*
 * Initialize the device - called from Slot manager.
 */

static int opened = 0;			/* our cards status */

static int isic_pccard_init(devi)
struct pccard_devinfo *devi;
{   
#ifdef AVM_A1_PCMCIA
    	struct isa_device *is = &devi->isahd;

	if ((1 << is->id_unit) & opened)
		return(EBUSY);

	opened |= 1 << is->id_unit;
	printf("isic%d: PCMCIA init, irqmask = 0x%x (%d), iobase = 0x%x\n",
                is->id_unit, is->id_irq, devi->slt->irq, is->id_iobase);

	/* 
	 * look if there is really an AVM PCMCIA Fritz!Card and
	 * setup the card specific stuff
	 */
	isic_probe_avma1_pcmcia(is);

	/* ap:
	 * XXX what's to do with the return value?
	 */

	/*
	 * try to attach the PCMCIA card as a normal A1 card
	 */

	isic_realattach(is, 0);

#endif
	return(0);
}

static void isic_unload(devi)
struct pccard_devinfo *devi;
{   
    	struct isa_device *is = &devi->isahd;
	printf("isic%d: unloaded\n", is->id_unit);
	opened &= ~(1 << is->id_unit);
}

/*
 * card_intr - Shared interrupt called from
 * front end of PC-Card handler.
 */
static int isic_card_intr(devi)
struct pccard_devinfo *devi;
{   
	isicintr(devi->isahd.id_unit);
	return(1);
}   

#endif /* (NISIC > 0) && (NCARD > 0) */
#endif /* __FreeBSD__ */
