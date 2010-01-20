/*-
 * Copyright (c) 2000 Jonathan Lemon
 * Copyright (c) 1999 by Matthew N. Dodd <winter@jurai.net>
 * All Rights Reserved.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <sys/bio.h>
#include <sys/conf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <geom/geom_disk.h>

#include <dev/ida/idavar.h>
#include <dev/ida/idareg.h>

#include <dev/eisa/eisaconf.h>

#define	IDA_EISA_IOPORT_START	0x0c88
#define	IDA_EISA_IOPORT_LEN	0x0017

#define	IDA_EISA_IRQ_REG	0x0cc0
#define	IDA_EISA_IRQ_MASK	0xf0
#define	IDA_EISA_IRQ_15		0x80
#define	IDA_EISA_IRQ_14		0x40
#define	IDA_EISA_IRQ_11		0x10
#define	IDA_EISA_IRQ_10		0x20

static int
ida_v1_fifo_full(struct ida_softc *ida)
{
	u_int8_t status;

	status = ida_inb(ida, R_EISA_SYSTEM_DOORBELL);
	return ((status & EISA_CHANNEL_CLEAR) == 0);
}

static void
ida_v1_submit(struct ida_softc *ida, struct ida_qcb *qcb)
{
	u_int16_t size;

	/*
	 * On these cards, this location is actually for control flags.
	 * Set them to zero and pass in structure size via an I/O port.
	 */
	size = qcb->hwqcb->hdr.size << 2;
	qcb->hwqcb->hdr.size = 0;

	ida_outb(ida, R_EISA_SYSTEM_DOORBELL, EISA_CHANNEL_CLEAR);
	ida_outl(ida, R_EISA_LIST_ADDR, qcb->hwqcb_busaddr);
	ida_outw(ida, R_EISA_LIST_LEN, size);
	ida_outb(ida, R_EISA_LOCAL_DOORBELL, EISA_CHANNEL_BUSY);
}

static bus_addr_t
ida_v1_done(struct ida_softc *ida)
{
	struct ida_hardware_qcb *hwqcb;
	bus_addr_t completed;
	u_int8_t status;

	if ((ida_inb(ida, R_EISA_SYSTEM_DOORBELL) & EISA_CHANNEL_BUSY) == 0)
		return (0);

	ida_outb(ida, R_EISA_SYSTEM_DOORBELL, EISA_CHANNEL_BUSY);
	completed = ida_inl(ida, R_EISA_COMPLETE_ADDR);
	status = ida_inb(ida, R_EISA_LIST_STATUS);
	ida_outb(ida, R_EISA_LOCAL_DOORBELL, EISA_CHANNEL_CLEAR);

	if (completed != 0) {
		hwqcb = (struct ida_hardware_qcb *)
		    ((bus_addr_t)ida->hwqcbs +
		    ((completed & ~3) - ida->hwqcb_busaddr));
		hwqcb->req.error = status;
	}

	return (completed);
}

static int
ida_v1_int_pending(struct ida_softc *ida)
{
	return (ida_inb(ida, R_EISA_SYSTEM_DOORBELL) & EISA_CHANNEL_BUSY);
}

static void
ida_v1_int_enable(struct ida_softc *ida, int enable)
{
	if (enable) {
		ida_outb(ida, R_EISA_SYSTEM_DOORBELL, ~EISA_CHANNEL_CLEAR);
		ida_outb(ida, R_EISA_LOCAL_DOORBELL, EISA_CHANNEL_BUSY);
		ida_outb(ida, R_EISA_INT_MASK, INT_ENABLE);
		ida_outb(ida, R_EISA_SYSTEM_MASK, INT_ENABLE);
		ida->flags |= IDA_INTERRUPTS;
	} else {
		ida_outb(ida, R_EISA_SYSTEM_MASK, INT_DISABLE);
		ida->flags &= ~IDA_INTERRUPTS;
	}
}

static int
ida_v2_fifo_full(struct ida_softc *ida)
{
	return (ida_inl(ida, R_CMD_FIFO) == 0);
}

static void
ida_v2_submit(struct ida_softc *ida, struct ida_qcb *qcb)
{
	ida_outl(ida, R_CMD_FIFO, qcb->hwqcb_busaddr);
}

static bus_addr_t
ida_v2_done(struct ida_softc *ida)
{
	return (ida_inl(ida, R_DONE_FIFO));
}

static int
ida_v2_int_pending(struct ida_softc *ida)
{
	return (ida_inl(ida, R_INT_PENDING));
}

static void
ida_v2_int_enable(struct ida_softc *ida, int enable)
{
	if (enable)
		ida->flags |= IDA_INTERRUPTS;
	else
		ida->flags &= ~IDA_INTERRUPTS;
	ida_outl(ida, R_INT_MASK, enable ? INT_ENABLE : INT_DISABLE);
}

static struct ida_access ida_v1_access = {
	ida_v1_fifo_full,
	ida_v1_submit,
	ida_v1_done,
	ida_v1_int_pending,
	ida_v1_int_enable,
};

static struct ida_access ida_v2_access = {
	ida_v2_fifo_full,
	ida_v2_submit,
	ida_v2_done,
	ida_v2_int_pending,
	ida_v2_int_enable,
};

static struct ida_board board_id[] = {
	{ 0x0e114001, "Compaq IDA controller",
	    &ida_v1_access, 0 },
	{ 0x0e114002, "Compaq IDA-2 controller",
	    &ida_v1_access, 0 },	
	{ 0x0e114010, "Compaq IAES controller",
	    &ida_v1_access, 0 },
	{ 0x0e114020, "Compaq SMART array controller",
	    &ida_v1_access, 0 },
	{ 0x0e114030, "Compaq SMART-2/E array controller",
	    &ida_v2_access, 0 },

	{ 0, "", 0, 0 }
};

static struct 	ida_board *ida_eisa_match(eisa_id_t);
static int	ida_eisa_probe(device_t);
static int	ida_eisa_attach(device_t);

static device_method_t ida_eisa_methods[] = {
	DEVMETHOD(device_probe,		ida_eisa_probe),
	DEVMETHOD(device_attach,	ida_eisa_attach),
	DEVMETHOD(device_detach,	ida_detach),

	{ 0, 0 }
};

static driver_t ida_eisa_driver = {
	"ida",
	ida_eisa_methods,
	sizeof(struct ida_softc)
};

static devclass_t ida_devclass;

static struct ida_board *
ida_eisa_match(eisa_id_t id)
{
	int i;

	for (i = 0; board_id[i].board; i++)
		if (board_id[i].board == id)
			return (&board_id[i]);
	return (NULL);
}

static int
ida_eisa_probe(device_t dev)
{
	struct ida_board	*board;
	u_int32_t		io_base;
	u_int			irq = 0;

	board = ida_eisa_match(eisa_get_id(dev));
	if (board == NULL)
		return (ENXIO);
	device_set_desc(dev, board->desc);

	io_base = (eisa_get_slot(dev) * EISA_SLOT_SIZE);

	switch (IDA_EISA_IRQ_MASK & (inb(IDA_EISA_IRQ_REG + io_base))) {
	case IDA_EISA_IRQ_15:
		irq = 15;
		break;
	case IDA_EISA_IRQ_14:
		irq = 14;
		break;
	case IDA_EISA_IRQ_11:
		irq = 11;
		break;
	case IDA_EISA_IRQ_10:
		irq = 10;
		break;
	default:
		device_printf(dev, "slot %d, illegal irq setting.\n",
		    eisa_get_slot(dev));
		return (ENXIO);
	}

	eisa_add_iospace(dev, (io_base + IDA_EISA_IOPORT_START),
			 IDA_EISA_IOPORT_LEN, RESVADDR_NONE);

	eisa_add_intr(dev, irq, EISA_TRIGGER_LEVEL);		/* XXX ??? */

	return (0);
}

static int
ida_eisa_attach(device_t dev)
{
	struct ida_softc	*ida;
	struct ida_board	*board;
	int			error;
	int			rid;

	ida = device_get_softc(dev);
	ida->dev = dev;

	board = ida_eisa_match(eisa_get_id(dev));
	ida->cmd = *board->accessor;
	ida->flags = board->flags;

	ida->regs_res_type = SYS_RES_IOPORT;
	ida->regs_res_id = 0;
	ida->regs = bus_alloc_resource_any(dev, ida->regs_res_type,
	    &ida->regs_res_id, RF_ACTIVE);
	if (ida->regs == NULL) {
		device_printf(dev, "can't allocate register resources\n");
		return (ENOMEM);
	}

	error = bus_dma_tag_create(
		/* parent	*/	NULL,
		/* alignment	*/	0,
		/* boundary	*/	0,
		/* lowaddr	*/	BUS_SPACE_MAXADDR_32BIT,
		/* highaddr	*/	BUS_SPACE_MAXADDR,
		/* filter	*/	NULL,
		/* filterarg	*/	NULL,
		/* maxsize	*/	MAXBSIZE,
		/* nsegments	*/	IDA_NSEG,
		/* maxsegsize	*/	BUS_SPACE_MAXSIZE_32BIT,
		/* flags	*/	BUS_DMA_ALLOCNOW,
		/* lockfunc	*/	NULL,
		/* lockarg	*/	NULL,
		&ida->parent_dmat);

	if (error != 0) {
		device_printf(dev, "can't allocate DMA tag\n");
		ida_free(ida);
		return (ENOMEM);
	}

	rid = 0;
	ida->irq_res_type = SYS_RES_IRQ;
	ida->irq = bus_alloc_resource_any(dev, ida->irq_res_type, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (ida->irq == NULL) {
		ida_free(ida);
		return (ENOMEM);
	}

	error = bus_setup_intr(dev, ida->irq, INTR_TYPE_BIO | INTR_ENTROPY,
	    NULL, ida_intr, ida, &ida->ih);
	if (error) {
		device_printf(dev, "can't setup interrupt\n");
		ida_free(ida);
		return (ENOMEM);
	}

	error = ida_init(ida);
	if (error) {
		ida_free(ida);
		return (error);
	}

	ida_attach(ida);
	ida->flags |= IDA_ATTACHED;

	return (0);
}

DRIVER_MODULE(ida, eisa, ida_eisa_driver, ida_devclass, 0, 0);
