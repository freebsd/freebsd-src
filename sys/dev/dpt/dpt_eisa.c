/*-
 * Copyright (c) 1997, 2000 Matthew N. Dodd <winter@jurai.net>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/eisa/eisaconf.h>

#include <cam/scsi/scsi_all.h>

#include <dev/dpt/dpt.h>

#define DPT_EISA_IOSIZE			0x100
#define DPT_EISA_SLOT_OFFSET		0x0c00
#define DPT_EISA_EATA_REG_OFFSET	0x0088

#define	DPT_EISA_DPT2402	0x12142402	/* DPT PM2012A/9X	*/
#define	DPT_EISA_DPTA401	0x1214A401	/* DPT PM2012B/9X	*/
#define	DPT_EISA_DPTA402	0x1214A402	/* DPT PM2012B2/9X	*/
#define	DPT_EISA_DPTA410	0x1214A410	/* DPT PM2x22A/9X	*/
#define	DPT_EISA_DPTA411	0x1214A411	/* DPT Spectre		*/
#define	DPT_EISA_DPTA412	0x1214A412	/* DPT PM2021A/9X	*/
#define	DPT_EISA_DPTA420	0x1214A420	/* DPT Smart Cache IV (PM2042) */
#define	DPT_EISA_DPTA501	0x1214A501	/* DPT PM2012B1/9X"	*/
#define	DPT_EISA_DPTA502	0x1214A502	/* DPT PM2012Bx/9X	*/
#define	DPT_EISA_DPTA701	0x1214A701	/* DPT PM2011B1/9X	*/
#define	DPT_EISA_DPTBC01	0x1214BC01	/* DPT PM3011/7X ESDI	*/ 
#define	DPT_EISA_DPT8200	0x12148200	/* NEC EATA SCSI	*/
#define	DPT_EISA_DPT2408	0x12142408	/* ATT EATA SCSI	*/

/* Function Prototypes */

static const char *	dpt_eisa_match	(eisa_id_t);
static int		dpt_eisa_probe	(device_t);
static int		dpt_eisa_attach	(device_t);


static int
dpt_eisa_probe (device_t dev)
{
	const char *	desc;
	u_int32_t	io_base;
	dpt_conf_t *	conf;

	desc = dpt_eisa_match(eisa_get_id(dev));
	if (!desc)
		return (ENXIO);
	device_set_desc(dev, desc);

	io_base = (eisa_get_slot(dev) * EISA_SLOT_SIZE) + DPT_EISA_SLOT_OFFSET;

	conf = dpt_pio_get_conf(io_base + DPT_EISA_EATA_REG_OFFSET);
	if (!conf) {
		printf("dpt: dpt_pio_get_conf() failed.\n");
		return (ENXIO);
	}

	eisa_add_iospace(dev, io_base, DPT_EISA_IOSIZE, RESVADDR_NONE);
	eisa_add_intr(dev, conf->IRQ,
		      (conf->IRQ_TR ? EISA_TRIGGER_LEVEL : EISA_TRIGGER_EDGE));

	return 0;
}

static int
dpt_eisa_attach (device_t dev)
{
	dpt_softc_t * dpt;
	struct resource *io = 0;
	struct resource *irq = 0;
	int		s;
	int		rid;
	void *		ih;
	int		error = 0;

	rid = 0;
	io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);
	if (!io) {
		device_printf(dev, "No I/O space?!\n");
		error = ENOMEM;
		goto bad;
	}

	rid = 0;
	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1, RF_ACTIVE);
	if (!irq) {
		device_printf(dev, "No irq?!\n");
		error = ENOMEM;
		goto bad;
	}

	dpt = dpt_alloc(dev, rman_get_bustag(io),
			rman_get_bushandle(io) + DPT_EISA_EATA_REG_OFFSET);
	if (dpt == NULL) {
		error = ENOMEM;
		goto bad;
	}

	/* Allocate a dmatag representing the capabilities of this attachment */
	/* XXX Should be a child of the EISA bus dma tag */
	if (bus_dma_tag_create(	/* parent    */	NULL,
				/* alignemnt */	1,
				/* boundary  */	0,
				/* lowaddr   */	BUS_SPACE_MAXADDR_32BIT,
				/* highaddr  */	BUS_SPACE_MAXADDR,
				/* filter    */	NULL,
				/* filterarg */	NULL,
				/* maxsize   */	BUS_SPACE_MAXSIZE_32BIT,
				/* nsegments */	~0,
				/* maxsegsz  */	BUS_SPACE_MAXSIZE_32BIT,
				/* flags     */0,
				&dpt->parent_dmat) != 0) {
		dpt_free(dpt);
		error = ENXIO;
		goto bad;
	}

	s = splcam();

	if (dpt_init(dpt) != 0) {
		dpt_free(dpt);
		error = ENXIO;
		goto bad;
	}

	/* Register with the XPT */
	dpt_attach(dpt);

	splx(s);

	if (bus_setup_intr(dev, irq, INTR_TYPE_CAM | INTR_ENTROPY, dpt_intr,
			   dpt, &ih)) {
		device_printf(dev, "Unable to register interrupt handler\n");
		error = ENXIO;
		goto bad;
	}

	return (error);

 bad:
	if (io)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, io);
	if (irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, irq);

	return (error);
}

static const char	*
dpt_eisa_match(type)
	eisa_id_t	type;
{
	switch (type) {
		case DPT_EISA_DPT2402:
		case DPT_EISA_DPTA401:
		case DPT_EISA_DPTA402:
		case DPT_EISA_DPTA410:
		case DPT_EISA_DPTA411:
		case DPT_EISA_DPTA412:
		case DPT_EISA_DPTA420:
		case DPT_EISA_DPTA501:
		case DPT_EISA_DPTA502:
		case DPT_EISA_DPTA701:
		case DPT_EISA_DPTBC01:
		case DPT_EISA_DPT8200:
		case DPT_EISA_DPT2408:
			return ("DPT SCSI Host Bus Adapter");
			break;
		default:
			break;
	}
	
	return (NULL);
}

static device_method_t dpt_eisa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dpt_eisa_probe),
	DEVMETHOD(device_attach,	dpt_eisa_attach),

	{ 0, 0 }
};

static driver_t dpt_eisa_driver = {
	"dpt",
	dpt_eisa_methods,
	sizeof(dpt_softc_t),
};

static devclass_t dpt_devclass;

DRIVER_MODULE(dpt, eisa, dpt_eisa_driver, dpt_devclass, 0, 0);
