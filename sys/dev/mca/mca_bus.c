/*-
 * Copyright (c) 1999 Matthew N. Dodd <winter@jurai.net>
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
 * $FreeBSD$
 */

/*
 * References:
 *		The CMU Mach3 microkernel
 *		NetBSD MCA patches by Scott Telford
 *		Linux MCA code.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
 
#include <machine/limits.h>
#include <machine/bus.h>	      
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/mca/mca_busreg.h>
#include <dev/mca/mca_busvar.h>

#include <sys/interrupt.h>

#define MAX_COL	 79

static void	mca_reg_print	(device_t, char *, char *, int *);

struct mca_device {
	struct resource_list rl;	/* Resources */

	mca_id_t	id;
	u_int8_t	slot;
	u_int8_t	enabled;
	u_int8_t	pos[8];		/* Programable Option Select Regs. */
};

/* Not supposed to use this function! */
void
mca_pos_set (dev, reg, data)
	device_t	dev;
	u_int8_t	reg;
	u_int8_t	data;
{
	struct mca_device *	m_dev = device_get_ivars(dev);
	u_int8_t		slot = mca_get_slot(dev);

	if ((slot > MCA_MAX_ADAPTERS) || (reg > MCA_POS7))
		return;

	/* Disable motherboard setup */
	outb(MCA_MB_SETUP_REG, MCA_MB_SETUP_DIS);

	/* Select adapter setup regs */
	outb(MCA_ADAP_SETUP_REG, ((slot & 0x0f) | MCA_ADAP_SET));

	/* Write the register */
	outb(MCA_POS_REG(reg), data); 

	/* Disable adapter setup */
	outb(MCA_ADAP_SETUP_REG, MCA_ADAP_SETUP_DIS);

	/* Update the IVAR copy */
	m_dev->pos[reg] = data;

	return;
}

u_int8_t
mca_pos_get (dev, reg)
	device_t	dev;
	u_int8_t	reg;
{
	u_int8_t	slot = mca_get_slot(dev);
	u_int8_t	data = 0;

	if ((slot > MCA_MAX_ADAPTERS) || (reg > MCA_POS7))
		return (0);

	/* Disable motherboard setup */
	outb(MCA_MB_SETUP_REG, MCA_MB_SETUP_DIS);

	switch (slot) {
		case MCA_MB_SCSI_SLOT:

			/* Disable adapter setup */
			outb(MCA_ADAP_SETUP_REG, MCA_ADAP_SETUP_DIS);

			/* Select motherboard video setup regs */
			outb(MCA_MB_SETUP_REG, MCA_MB_SETUP_SCSI);

			/* read the register */
			data = inb(MCA_POS_REG(reg));

			/* Disable motherboard setup */
			outb(MCA_MB_SETUP_REG, MCA_MB_SETUP_DIS);

			break;
		case MCA_MB_VIDEO_SLOT:
			/* Disable adapter setup */
			outb(MCA_ADAP_SETUP_REG, MCA_ADAP_SETUP_DIS);

			/* Select motherboard scsi setup regs */
			outb(MCA_MB_SETUP_REG, MCA_MB_SETUP_VIDEO);

			/* read the register */
			data = inb(MCA_POS_REG(reg));

			/* Disable motherboard setup */
			outb(MCA_MB_SETUP_REG, MCA_MB_SETUP_DIS);
			break;
		default:

			/* Select adapter setup regs */
			outb(MCA_ADAP_SETUP_REG,
			     ((slot & 0x0f) | MCA_ADAP_SET));

			/* read the register */
			data = inb(MCA_POS_REG(reg));

			/* Disable adapter setup */
			outb(MCA_ADAP_SETUP_REG, MCA_ADAP_SETUP_DIS);
			break;
	}

	return (data);
}

const char *
mca_match_id (id, mca_devs)
	u_int16_t		id;
	struct mca_ident *	mca_devs;
{
	struct mca_ident *	m = mca_devs;
	while(m->name != NULL) {
		if (id == m->id)
			return (m->name);
		m++;
	}
	return (NULL);
}

u_int8_t
mca_pos_read (dev, reg)
	device_t		dev;
	u_int8_t		reg;
{
	struct mca_device *	m_dev = device_get_ivars(dev);

	if (reg > MCA_POS7)
		return (0);

	return (m_dev->pos[reg]);
}

void
mca_add_irq (dev, irq)
	device_t		dev;
	int			irq;
{
	struct mca_device *	m_dev = device_get_ivars(dev);
	int			rid = 0;

	while (resource_list_find(&(m_dev->rl), SYS_RES_IRQ, rid)) rid++;
	resource_list_add(&(m_dev->rl), SYS_RES_IRQ, rid, irq, irq, 1);

	return;
}

void
mca_add_drq (dev, drq)
	device_t		dev;
	int			drq;
{
	struct mca_device *	m_dev = device_get_ivars(dev);
	int			rid = 0;

	while (resource_list_find(&(m_dev->rl), SYS_RES_DRQ, rid)) rid++;
	resource_list_add(&(m_dev->rl), SYS_RES_DRQ, rid, drq, drq, 1);

	return;
}

void
mca_add_mspace (dev, mbase, msize) 
	device_t		dev;
	u_long			mbase;
	u_long			msize;
{
	struct mca_device *	m_dev = device_get_ivars(dev);
	int			rid = 0;

	while (resource_list_find(&(m_dev->rl), SYS_RES_MEMORY, rid)) rid++;
	resource_list_add(&(m_dev->rl), SYS_RES_MEMORY, rid,
		mbase, (mbase + msize), msize);

	return;
}

void
mca_add_iospace (dev, iobase, iosize) 
	device_t		dev;
	u_long			iobase;
	u_long			iosize;
{
	struct mca_device *	m_dev = device_get_ivars(dev);
	int			rid = 0;

	while (resource_list_find(&(m_dev->rl), SYS_RES_IOPORT, rid)) rid++;
	resource_list_add(&(m_dev->rl), SYS_RES_IOPORT, rid,
		iobase, (iobase + iosize), iosize);

	return;
}

static int
mca_probe (device_t dev)
{
	device_t		child;
	struct mca_device *	m_dev = NULL;
	int			devices_found = 0;
	u_int8_t		slot;
	u_int8_t		reg;

	device_set_desc(dev, "MCA bus");

	/* Disable adapter setup */
	outb(MCA_ADAP_SETUP_REG, MCA_ADAP_SETUP_DIS);
	/* Disable motherboard setup */
	outb(MCA_MB_SETUP_REG, MCA_MB_SETUP_DIS);

	if (bootverbose) {
		printf("POS REG     00 01 02 03 04 05 06 07\n");
		printf("-----------------------------------\n");
	}

	for (slot = 0; slot < MCA_MAX_SLOTS; slot++) {

		if (!m_dev) {
			m_dev = (struct mca_device *)malloc(sizeof(*m_dev),
		 					    M_DEVBUF, M_NOWAIT);
			if (!m_dev) {
				device_printf(dev, "cannot malloc mca_device");
				break;
			}
		}
		bzero(m_dev, sizeof(*m_dev));

		/* Select adapter setup regs */
		outb(MCA_ADAP_SETUP_REG, ((slot & 0x0f) | MCA_ADAP_SET));

		/* Read the POS registers */
		for (reg = MCA_POS0; reg <= MCA_POS7; reg++) {
			m_dev->pos[reg] = inb(MCA_POS_REG(reg));
		}

		/* Disable adapter setup */
		outb(MCA_ADAP_SETUP_REG, MCA_ADAP_SETUP_DIS);

		if (bootverbose) {
			printf("mca slot %d:", slot + 1);	
			for (reg = MCA_POS0; reg <= MCA_POS7; reg++) {
				printf(" %02x", m_dev->pos[reg]);
			}
			printf("\n");
		}

		m_dev->id = (u_int16_t)m_dev->pos[MCA_POS0] |
			    ((u_int16_t)m_dev->pos[MCA_POS1] << 8);

		if (m_dev->id == 0xffff) {
			continue;
		}

		devices_found++;

		m_dev->enabled = (m_dev->pos[MCA_POS2] & MCA_POS2_ENABLE);
		m_dev->slot = slot;

		resource_list_init(&(m_dev->rl));

		child = device_add_child(dev, NULL, -1);
		device_set_ivars(child, m_dev);

		m_dev = NULL;
	}

	if (m_dev) {
		free(m_dev, M_DEVBUF);
	}

	return (devices_found ? 0 : ENXIO);
}

static void
mca_reg_print (dev, string, separator, column)
	device_t	dev;
	char *		string;
	char *		separator;
	int *		column;
{
	int		length = strlen(string);

	length += (separator ? 2 : 1);

	if (((*column) + length) >= MAX_COL) {
		printf("\n");
		(*column) = 0;
	} else if ((*column) != 0) {
		if (separator) {
			printf("%c", *separator);
			(*column)++;
		}
		printf(" ");
		(*column)++;
	}

	if ((*column) == 0) {
		(*column) += device_printf(dev, "%s", string);
	} else {
		(*column) += printf("%s", string);
	}

	return;
}

static int
mca_print_child (device_t dev, device_t child)
{
	char				buf[MAX_COL+1];
	struct mca_device *		m_dev = device_get_ivars(child);
	int				rid;
	struct resource_list_entry *	rle;
	char				separator = ',';
	int				column = 0;
	int				retval = 0;

	if (device_get_desc(child)) {
		snprintf(buf, sizeof(buf), "<%s>", device_get_desc(child));
		mca_reg_print(child, buf, NULL, &column);
	}

	rid = 0;
	while ((rle = resource_list_find(&(m_dev->rl), SYS_RES_IOPORT, rid++))) {
		if (rle->count == 1) {
			snprintf(buf, sizeof(buf), "%s%lx",
				((rid == 1) ? "io 0x" : "0x"),
				rle->start);
		} else {
			snprintf(buf, sizeof(buf), "%s%lx-0x%lx",
				((rid == 1) ? "io 0x" : "0x"),
				rle->start,
				(rle->start + rle->count));
		}
		mca_reg_print(child, buf,
			((rid == 2) ? &separator : NULL), &column);
	}

	rid = 0;
	while ((rle = resource_list_find(&(m_dev->rl), SYS_RES_MEMORY, rid++))) {
		if (rle->count == 1) {
			snprintf(buf, sizeof(buf), "%s%lx",
				((rid == 1) ? "mem 0x" : "0x"),
				rle->start);
		} else {
			snprintf(buf, sizeof(buf), "%s%lx-0x%lx",
				((rid == 1) ? "mem 0x" : "0x"),
				rle->start,
				(rle->start + rle->count));
		}
		mca_reg_print(child, buf,
			((rid == 2) ? &separator : NULL), &column);
	}

	rid = 0;
	while ((rle = resource_list_find(&(m_dev->rl), SYS_RES_IRQ, rid++))) {
		snprintf(buf, sizeof(buf), "irq %ld", rle->start);
		mca_reg_print(child, buf,
			((rid == 1) ? &separator : NULL), &column);
	}

	rid = 0;
	while ((rle = resource_list_find(&(m_dev->rl), SYS_RES_DRQ, rid++))) {
		snprintf(buf, sizeof(buf), "drq %lx", rle->start);
		mca_reg_print(child, buf,
			((rid == 1) ? &separator : NULL), &column);
	}

	snprintf(buf, sizeof(buf), "on %s id %04x slot %d\n",
		device_get_nameunit(dev),
		mca_get_id(child), mca_get_slot(child)+1);
	mca_reg_print(child, buf, NULL, &column);

	return (retval);
}

static void
mca_probe_nomatch (device_t dev, device_t child)
{
	mca_id_t	mca_id = mca_get_id(child);
	u_int8_t	slot = mca_get_slot(child);
	u_int8_t	enabled = mca_get_enabled(child);

	device_printf(dev, "unknown card (id 0x%04x, %s) at slot %d\n",
		mca_id,
		(enabled ? "enabled" : "disabled"),
		slot + 1);

	return;
}

static int
mca_read_ivar (device_t dev, device_t child, int which, u_long * result)
{
	struct mca_device *		m_dev = device_get_ivars(child);

	switch (which) {
		case MCA_IVAR_SLOT:
			*result = m_dev->slot;
			break;
		case MCA_IVAR_ID:
			*result = m_dev->id;
			break;
		case MCA_IVAR_ENABLED:
			*result = m_dev->enabled;
			break;
		default:
			return (ENOENT);
			break;
	}

	return (0);
}

static int
mca_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	return (EINVAL);
}

static struct resource *
mca_alloc_resource (device_t dev, device_t child, int type, int *rid,
		    u_long start, u_long end, u_long count, u_int flags)
{
	struct mca_device *		m_dev = device_get_ivars(child);
	struct resource_list_entry *	rle;
	int				isdefault;
	int				passthrough;

	isdefault = (start == 0UL && end == ~0UL);
	passthrough = (device_get_parent(child) != dev);

	if (!passthrough && !isdefault) {
		rle = resource_list_find(&(m_dev->rl), type, *rid);
		if (!rle) {
			resource_list_add(&(m_dev->rl), type, *rid,
					  start, end, count);
		}
	}

	if (type == SYS_RES_IRQ) {
		flags |= RF_SHAREABLE;
	}

	return (resource_list_alloc(&(m_dev->rl), dev, child, type, rid,
				    start, end, count, flags));
}

static int
mca_release_resource (device_t dev, device_t child, int type, int rid,
		      struct resource * r)
{
	struct mca_device *		m_dev = device_get_ivars(child);

	return (resource_list_release(&(m_dev->rl), dev, child, type, rid, r));
}

static int
mca_get_resource(device_t dev, device_t child, int type, int rid,
		 u_long *startp, u_long *countp)
{
	struct mca_device *		m_dev = device_get_ivars(child);
	struct resource_list *		rl = &(m_dev->rl);
	struct resource_list_entry *	rle;

	rle = resource_list_find(rl, type, rid);
	if (!rle)
		return ENOENT;
	
	*startp = rle->start;
	*countp = rle->count;

	return (0);
}

static int
mca_set_resource(device_t dev, device_t child, int type, int rid,
		 u_long start, u_long count)
{
	struct mca_device *		m_dev = device_get_ivars(child);
	struct resource_list *		rl = &(m_dev->rl);

	resource_list_add(rl, type, rid, start, start + count - 1, count);
	return (0);
}

static void
mca_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct mca_device *		m_dev = device_get_ivars(child);
	struct resource_list *		rl = &(m_dev->rl);

	resource_list_delete(rl, type, rid);
}

static device_method_t mca_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mca_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_shutdown,      bus_generic_shutdown),
	DEVMETHOD(device_suspend,       bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	mca_print_child),
	DEVMETHOD(bus_probe_nomatch,	mca_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	mca_read_ivar),
	DEVMETHOD(bus_write_ivar,	mca_write_ivar),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	DEVMETHOD(bus_alloc_resource,	mca_alloc_resource),
	DEVMETHOD(bus_release_resource,	mca_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),     

	DEVMETHOD(bus_set_resource,     mca_set_resource),
	DEVMETHOD(bus_get_resource,     mca_get_resource),
	DEVMETHOD(bus_delete_resource,  mca_delete_resource),

	{ 0, 0 }
};

static driver_t mca_driver = {       
	"mca",
	mca_methods,
	1,		/* no softc */
};

static devclass_t mca_devclass;

DRIVER_MODULE(mca, nexus, mca_driver, mca_devclass, 0, 0);
