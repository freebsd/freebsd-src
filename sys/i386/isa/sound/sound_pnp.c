/*
 * sound/sound_pnp.c
 * 
 * PnP soundcard support (Linux spesific)
 * 
 * Copyright by Hannu Savolainen 1995
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 */
#include <i386/isa/sound/sound_config.h>

/*
 * XXX check what to use in place of CONFIG_PNP
 */
#if (NSND > 0) && defined(CONFIG_PNP)

#include <linux/pnp.h>

static struct pnp_sounddev *pnp_devs[20] = { NULL };

static int      max_pnpdevs = 20;
static int      nr_pnpdevs = 0;
static int      pnp_sig = 0;

void
install_pnp_sounddrv(struct pnp_sounddev * drv)
{
    if (nr_pnpdevs < max_pnpdevs) {
	pnp_devs[nr_pnpdevs++] = drv;
    } else
	printf("Sound: More than 20 PnP drivers defined\n");
}

void
cs4232_pnp(void *parm)
{
    struct pnp_dev *dev = (struct pnp_dev *) parm;
    char           *name;

    int             portmask = 0x00, irqmask = 0x01, dmamask = 0x03;
    int             opl3_driver, wss_driver;

    printf("CS4232 driver waking up\n");

    if (dev->card && dev->card->name)
	name = dev->card->name;
    else
	name = "PnP WSS";

    if ((wss_driver = sndtable_identify_card("AD1848")))
	portmask |= 0x01;	/* MSS */
    else
	printf("Sound: MSS/WSS device detected but no driver enabled\n");

    if ((opl3_driver = sndtable_identify_card("OPL3")))
	portmask |= 0x02;	/* OPL3 */
    else
	printf("Sound: OPL3/4 device detected but no driver enabled\n");

    printf("WSS driver %d, OPL3 driver %d\n", wss_driver, opl3_driver);

    if (!portmask)		/* No drivers available */
	return;

    if (!pnp_allocate_device(pnp_sig, dev, portmask, irqmask, dmamask, 0x00))
	printf("Device activation failed\n");
    else {
	struct address_info hw_config;
	int             wss_base, opl3_base;
	int             irq;
	int             dma1, dma2;

	printf("Device activation OK\n");
	wss_base = pnp_get_port(dev, 0);
	opl3_base = pnp_get_port(dev, 1);
	irq = pnp_get_irq(dev, 0);
	dma1 = pnp_get_dma(dev, 0);
	dma2 = pnp_get_dma(dev, 1);

	printf("I/O0 %03x\n", wss_base);
	printf("I/O1 %03x\n", opl3_base);
	printf("IRQ %d\n", irq);
	printf("DMA0 %d\n", dma1);
	printf("DMA1 %d\n", dma2);

	if (opl3_base && opl3_driver) {
	    hw_config.io_base = opl3_base;
	    hw_config.irq = 0;
	    hw_config.dma = -1;
	    hw_config.dma2 = -1;
	    hw_config.always_detect = 0;
	    hw_config.name = "";
	    hw_config.osp = NULL;
	    hw_config.card_subtype = 0;

	    if (sndtable_probe(opl3_driver, &hw_config))
		sndtable_init_card(opl3_driver, &hw_config);

	}
	if (wss_base && wss_driver) {
	    hw_config.io_base = wss_base;
	    hw_config.irq = irq;
	    hw_config.dma = dma1;
	    hw_config.dma2 = (dma2 == NO_DMA) ? dma1 : dma2;
	    hw_config.always_detect = 0;
	    hw_config.name = name;
	    hw_config.osp = NULL;
	    hw_config.card_subtype = 0;

	    if (sndtable_probe(wss_driver, &hw_config))
		sndtable_init_card(wss_driver, &hw_config);

	}
    }
}

static int
pnp_activate(int id, struct pnp_dev * dev)
{
    int             i;

    for (i = 0; i < nr_pnpdevs; i++)
	if (pnp_devs[i]->id == id) {

	    printf("PnP dev: %08x, %s\n", id, pnp_devid2asc(id));

	    pnp_devs[i]->setup((void *) dev);
	    return 1;
	}
    return 0;
}

void
sound_pnp_init(void)
{
    static struct pnp_sounddev cs4232_dev =
	{PNP_DEVID('C', 'S', 'C', 0x0000), cs4232_pnp, "CS4232"};

    struct pnp_dev *dev;

    install_pnp_sounddrv(&cs4232_dev);

    dev = NULL;

    if ((pnp_sig = pnp_connect("sound")) == -1) {
	printf("Sound: Can't connect to kernel PnP services.\n");
	return;
    }
    while ((dev = pnp_get_next_device(pnp_sig, dev)) != NULL) {
	if (!pnp_activate(dev->key, dev)) {
	    /* Scan all compatible devices */

	    int             i;

	    for (i = 0; i < dev->ncompat; i++)
		if (pnp_activate(dev->compat_keys[i], dev))
		    break;
	}
    }
}

void
sound_pnp_disconnect(void)
{
    pnp_disconnect(pnp_sig);
}
#endif
