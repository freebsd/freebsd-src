/*
 * Product specific probe and attach routines for:
 *      Adaptec 154x.
 *
 * Derived from code written by:
 *
 * Copyright (c) 1998 Justin T. Gibbs
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
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
 *	$Id: aha_isa.c,v 1.7 1999/04/16 21:22:19 peter Exp $
 */

#include "pnp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <i386/isa/isa_device.h>
#include <dev/aha/ahareg.h>

#include <cam/scsi/scsi_all.h>

#if NPNP > 0
#include <i386/isa/pnp.h>
#endif

static	int aha_isa_probe(struct isa_device *dev);
static	int aha_isa_attach(struct isa_device *dev);
static	void aha_isa_intr(void *unit);

struct isa_driver ahadriver =
{
    aha_isa_probe,
    aha_isa_attach,
    "aha"
};

/*
 * Check if the device can be found at the port given
 * and if so, set it up ready for further work
 * as an argument, takes the isa_device structure from
 * autoconf.c
 */
static int
aha_isa_probe(dev)
	struct isa_device *dev;
{
	/*
	 * find unit and check we have that many defined
	 */
	struct	aha_softc *aha;
	int	port_index;
	int	max_port_index;

	aha = NULL;

	/*
	 * Bound our board search if the user has
	 * specified an exact port.
	 */
	aha_find_probe_range(dev->id_iobase, &port_index, &max_port_index);

	if (port_index < 0)
		return 0;

	/* Attempt to find an adapter */
	for (;port_index <= max_port_index; port_index++) {
		config_data_t config_data;
		u_int ioport;
		int error;

		ioport = aha_iop_from_bio(port_index);

		/*
		 * Ensure this port has not already been claimed already
		 * by a PCI, EISA or ISA adapter.
		 */
		if (aha_check_probed_iop(ioport) != 0)
			continue;
		dev->id_iobase = ioport;
		if (haveseen_iobase(dev, AHA_NREGS))
			continue;

		/* Allocate a softc for use during probing */
		aha = aha_alloc(dev->id_unit, I386_BUS_SPACE_IO, ioport);

		if (aha == NULL)
			break;

		/* We're going to attempt to probe it now, so mark it probed */
		aha_mark_probed_bio(port_index);

		/* See if there is really a card present */
		if (aha_probe(aha) || aha_fetch_adapter_info(aha)) {
			aha_free(aha);
			continue;
		}

		/*
		 * Determine our IRQ, and DMA settings and
		 * export them to the configuration system.
		 */
		error = aha_cmd(aha, AOP_INQUIRE_CONFIG, NULL, /*parmlen*/0,
			       (u_int8_t*)&config_data, sizeof(config_data),
			       DEFAULT_CMD_TIMEOUT);
		if (error != 0) {
			printf("aha_isa_probe: Could not determine IRQ or DMA "
			       "settings for adapter at 0x%x.  Failing probe\n",
			       ioport);
			aha_free(aha);
			continue;
		}

		switch (config_data.dma_chan) {
		case DMA_CHAN_5:
			dev->id_drq = 5;
			break;
		case DMA_CHAN_6:
			dev->id_drq = 6;
			break;
		case DMA_CHAN_7:
			dev->id_drq = 7;
			break;
		default:
			printf("aha_isa_probe: Invalid DMA setting "
				"detected for adapter at 0x%x.  "
				"Failing probe\n", ioport);
			return (0);
		}
		dev->id_irq = (config_data.irq << 9);
		dev->id_intr = aha_isa_intr;
		aha_unit++;
		return (AHA_NREGS);
	}

	return (0);
}

/*
 * Attach all the sub-devices we can find
 */
static int
aha_isa_attach(dev)
	struct isa_device *dev;
{
	struct	aha_softc *aha;
	bus_dma_filter_t *filter;
	void		 *filter_arg;
	bus_addr_t	 lowaddr;

	aha = aha_softcs[dev->id_unit];
	if (dev->id_drq != -1)
		isa_dmacascade(dev->id_drq);

	/* Allocate our parent dmatag */
	filter = NULL;
	filter_arg = NULL;
	lowaddr = BUS_SPACE_MAXADDR_24BIT;

	if (bus_dma_tag_create(/*parent*/NULL, /*alignemnt*/0, /*boundary*/0,
                               lowaddr, /*highaddr*/BUS_SPACE_MAXADDR,
                               filter, filter_arg,
                               /*maxsize*/BUS_SPACE_MAXSIZE_24BIT,
                               /*nsegments*/BUS_SPACE_UNRESTRICTED,
                               /*maxsegsz*/BUS_SPACE_MAXSIZE_24BIT,
                               /*flags*/0, &aha->parent_dmat) != 0) {
                aha_free(aha);
                return (-1);
        }                              

        if (aha_init(aha)) {
		printf("aha init failed\n");
                aha_free(aha);
                return (-1);
        }

	return (aha_attach(aha));
}

/*
 * Handle an ISA interrupt.
 * XXX should go away as soon as ISA interrupt handlers
 * take a (void *) arg.
 */
static void
aha_isa_intr(void *unit)
{
	struct aha_softc* arg = aha_softcs[(int)unit];
	aha_intr((void *)arg);
}

/*
 * support PnP cards if we are using 'em
 */

#if NPNP > 0

static char *ahapnp_probe(u_long csn, u_long vend_id);
static void ahapnp_attach(u_long csn, u_long vend_id, char *name,
	struct isa_device *dev);
static u_long nahapnp = NAHA;

static struct pnp_device ahapnp = {
	"ahapnp",
	ahapnp_probe,
	ahapnp_attach,
	&nahapnp,
	&bio_imask
};
DATA_SET (pnpdevice_set, ahapnp);

static char *
ahapnp_probe(u_long csn, u_long vend_id)
{
	struct pnp_cinfo d;
	char *s = NULL;

	if (vend_id != AHA1542_PNP && vend_id != AHA1542_PNPCOMPAT)
		return (NULL);

	read_pnp_parms(&d, 0);
	if (d.enable == 0 || d.flags & 1) {
		printf("CSN %lu is disabled.\n", csn);
		return (NULL);
	}
	s = "Adaptec 1542CP";

	return (s);
}

static void
ahapnp_attach(u_long csn, u_long vend_id, char *name, struct isa_device *dev)
{
	struct pnp_cinfo d;

	if (dev->id_unit >= NAHATOT)
		return;

	if (read_pnp_parms(&d, 0) == 0) {
		printf("failed to read pnp parms\n");
		return;
	}

	write_pnp_parms(&d, 0);

	enable_pnp_card();

	dev->id_iobase = d.port[0];
	dev->id_irq = (1 << d.irq[0]);
	dev->id_intr = aha_intr;
	dev->id_drq = d.drq[0];

	if (dev->id_driver == NULL) {
		dev->id_driver = &ahadriver;
		dev->id_id = isa_compat_nextid();
	}

	if ((dev->id_alive = aha_isa_probe(dev)) != 0)
		aha_isa_attach(dev);
	else
		printf("aha%d: probe failed\n", dev->id_unit);
}
#endif
