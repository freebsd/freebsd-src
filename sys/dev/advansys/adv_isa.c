/*
 * Device probe and attach routines for the following
 * Advanced Systems Inc. SCSI controllers:
 *
 * Connectivity Products:
 *	ABP5140 - Bus-Master PnP ISA 16 CDB
 *
 * Single Channel Products:
 *	ABP542  - Bus-Master ISA 240 CDB
 *	ABP5150 - Bus-Master ISA 240 CDB (shipped by HP with the 4020i CD-R drive)
 *	ABP842  - Bus-Master VL 240 CDB
 *
 * Dual Channel Products:
 *	ABP852 - Dual Channel Bus-Master VL 240 CDB Per Channel
 *
 * Copyright (c) 1996 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
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
 *
 *      $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h> 

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>

#include <i386/scsi/advansys.h>

#define ADV_ISA_MAX_DMA_ADDR    (0x00FFFFFFL)
#define ADV_ISA_MAX_DMA_COUNT   (0x00FFFFFFL)

#define ADV_VL_MAX_DMA_ADDR     (0x07FFFFFFL)
#define ADV_VL_MAX_DMA_COUNT    (0x07FFFFFFL)

/* Possible port addresses an ISA or VL adapter can live at */
u_int16_t adv_isa_ioports[] =
{
	0x100,
	0x110,	/* First selection in BIOS setup */
	0x120,
	0x130,	/* Second selection in BIOS setup */
	0x140,
	0x150,	/* Third selection in BIOS setup */
	0x190,	/* Fourth selection in BIOS setup */
	0x210,	/* Fifth selection in BIOS setup */
	0x230,	/* Sixth selection in BIOS setup */
	0x250,	/* Seventh selection in BIOS setup */
	0x330 	/* Eighth and default selection in BIOS setup */
};

#define MAX_ISA_IOPORT_INDEX (sizeof(adv_isa_ioports)/sizeof(u_short) - 1)

static	int	advisaprobe __P((struct isa_device *id));
static  int	advisaattach __P((struct isa_device *id));
static	void	adv_set_isapnp_wait_for_key __P((void));
static	int	adv_find_signature __P((u_int16_t iobase));

void	adv_isa_intr __P((int unit));

struct isa_driver advdriver =
{
	advisaprobe,
	advisaattach,
	"adv"
};

static int
advisaprobe(id)
	struct isa_device *id;
{
	int	port_index;
	int	max_port_index;

	/*
	 * Default to scanning all possible device locations.
	 */
	port_index = 0;
	max_port_index = MAX_ISA_IOPORT_INDEX;

	if (id->id_iobase > 0) {
		for (;port_index <= max_port_index; port_index++)
			if (id->id_iobase >= adv_isa_ioports[port_index])
				break;
		if ((port_index > max_port_index)
		 || (id->id_iobase != adv_isa_ioports[port_index])) {
			printf("adv%d: Invalid baseport of 0x%x specified. "
				"Neerest valid baseport is 0x%x.  Failing "
				"probe.\n", id->id_unit, id->id_iobase,
				(port_index <= max_port_index) ?
					adv_isa_ioports[port_index] :
					adv_isa_ioports[max_port_index]);
			return 0;
		}
		max_port_index = port_index;
	}

	/* Perform the actual probing */
	adv_set_isapnp_wait_for_key();
	for (;port_index <= max_port_index; port_index++) {
		u_int16_t port_addr = adv_isa_ioports[port_index];
		if (port_addr == 0)
			/* Already been attached */
			continue;
		if (adv_find_signature(port_addr)) {
			/*
			 * Got one.  Now allocate our softc
			 * and see if we can initialize the card.
			 */
			struct adv_softc *adv;
			adv = adv_alloc(id->id_unit, port_addr);
			if (adv == NULL)
				return (0);

			id->id_iobase = adv->iobase;
			/*
			 * Determine the chip version.
			 */
			adv->chip_version = ADV_INB(adv,
						    ADV_NONEISA_CHIP_REVISION);
			
			if (adv_init(adv) != 0) {
				adv_free(adv);
				return (0);
			}
			switch (adv->type) {
			case ADV_ISAPNP:
				if (adv->chip_version == ADV_CHIP_VER_ASYN_BUG)
					adv->needs_async_bug_fix = TARGET_BIT_VECTOR_SET;
				/* Fall Through */
			case ADV_ISA:
				adv->max_dma_count = ADV_ISA_MAX_DMA_COUNT;
				break;

			case ADV_VL:
				adv->max_dma_count = ADV_VL_MAX_DMA_COUNT;
				break;
			}
			
			if ((adv->type & ADV_ISAPNP) == ADV_ISAPNP) {
			}

			/* Determine our IRQ */
			if (id->id_irq == 0 /* irq ? */)
				id->id_irq = 1 << adv_get_chip_irq(adv);
			else
				adv_set_chip_irq(adv, ffs(id->id_irq) - 1);
			
			/* Mark as probed */
			adv_isa_ioports[port_index] = 0;
			break;
		}
	}

	return 1;
}

static int
advisaattach(id)
	struct isa_device *id;
{
	struct adv_softc *adv;

	adv = advsoftcs[id->id_unit];
	return (adv_attach(adv));
}

static void
adv_set_isapnp_wait_for_key(void)
{
	static	int isapnp_wait_set = 0;
	if (isapnp_wait_set == 0) {
		outb(ADV_ISA_PNP_PORT_ADDR, 0x02);
		outb(ADV_ISA_PNP_PORT_WRITE, 0x02);
		isapnp_wait_set++;
	}
	return;                 
}

/*
 * Determine if there is a board at "iobase" by looking
 * for the AdvanSys signatures.  Return 1 if a board is
 * found, 0 otherwise.
 */
static int                         
adv_find_signature(iobase)
	u_int16_t iobase;
{                            
	u_int16_t signature;
   
	if (inb(iobase + ADV_SIGNATURE_BYTE) == ADV_1000_ID1B) {
		signature = inw(iobase + ADV_SIGNATURE_WORD );
		if ((signature == ADV_1000_ID0W)
		 || (signature == ADV_1000_ID0W_FIX))
			return (1);
	}
	return (0);
}


/*
 * Handle an ISA interrupt.
 * XXX should go away as soon as ISA interrupt handlers
 * take a (void *) arg.
 */
void
adv_isa_intr(unit)
	int	unit;
{
	struct adv_softc *arg = advsoftcs[unit];
	adv_intr((void *)arg);
}
