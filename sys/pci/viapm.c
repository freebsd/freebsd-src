/*-
 * Copyright (c) 2001 Alcove - Nicolas Souchu
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/uio.h>

#include <machine/bus.h>
#include <machine/clock.h>		/* for DELAY */
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include <dev/smbus/smbconf.h>
#include <dev/smbus/smbus.h>

#include "iicbb_if.h"
#include "smbus_if.h"

#define VIAPM_DEBUG(x)	if (viapm_debug) (x)

#ifdef DEBUG
static int viapm_debug = 1;
#else
static int viapm_debug = 0;
#endif

#define VIA_586B_PMU_ID		0x30401106
#define VIA_596A_PMU_ID		0x30501106
#define VIA_596B_PMU_ID		0x30511106
#define VIA_686A_PMU_ID		0x30571106
#define VIA_8233_PMU_ID		0x30741106
#define	VIA_8233A_PMU_ID	0x31471106

#define VIAPM_INB(port) \
	((u_char)bus_space_read_1(viapm->st, viapm->sh, port))
#define VIAPM_OUTB(port,val) \
	(bus_space_write_1(viapm->st, viapm->sh, port, (u_char)(val)))

#define VIAPM_TYP_UNKNOWN	0
#define VIAPM_TYP_586B_3040E	1
#define VIAPM_TYP_586B_3040F	2
#define VIAPM_TYP_596B		3
#define VIAPM_TYP_686A		4
#define VIAPM_TYP_8233		5

struct viapm_softc {
	int type;
	u_int32_t base;
        bus_space_tag_t st;
        bus_space_handle_t sh;
	int iorid;
	int irqrid;
	struct resource *iores;
	struct resource *irqres;
	void *irqih;

	device_t iicbb;
	device_t smbus;
};

static devclass_t viapm_devclass;
static devclass_t viapropm_devclass;

/*
 * VT82C586B definitions
 */

#define VIAPM_586B_REVID	0x08

#define VIAPM_586B_3040E_BASE	0x20
#define VIAPM_586B_3040E_ACTIV	0x4		/* 16 bits */

#define VIAPM_586B_3040F_BASE	0x48
#define VIAPM_586B_3040F_ACTIV	0x41		/* 8 bits */

#define VIAPM_586B_OEM_REV_E	0x00
#define VIAPM_586B_OEM_REV_F	0x01
#define VIAPM_586B_PROD_REV_A	0x10

#define VIAPM_586B_BA_MASK	0x0000ff00

#define GPIO_DIR	0x40
#define GPIO_VAL	0x42
#define EXTSMI_VAL	0x44

#define VIAPM_SCL	0x02			/* GPIO1_VAL */
#define VIAPM_SDA	0x04			/* GPIO2_VAL */

/*
 * VIAPRO common definitions
 */

#define VIAPM_PRO_BA_MASK	0x0000fff0
#define VIAPM_PRO_SMBCTRL	0xd2
#define VIAPM_PRO_REVID		0xd6

/*
 * VT82C686A definitions
 */

#define VIAPM_PRO_BASE		0x90

#define SMBHST			0x0
#define SMBHSL			0x1
#define SMBHCTRL		0x2
#define SMBHCMD			0x3
#define SMBHADDR		0x4
#define SMBHDATA0		0x5
#define SMBHDATA1		0x6
#define SMBHBLOCK		0x7

#define SMBSST			0x1
#define SMBSCTRL		0x8
#define SMBSSDWCMD		0x9
#define SMBSEVENT		0xa
#define SMBSDATA		0xc

#define SMBHST_RESERVED		0xef	/* reserved bits */
#define SMBHST_FAILED		0x10	/* failed bus transaction */
#define SMBHST_COLLID		0x08	/* bus collision */
#define SMBHST_ERROR		0x04	/* device error */
#define SMBHST_INTR		0x02	/* command completed */
#define SMBHST_BUSY		0x01	/* host busy */

#define SMBHCTRL_START		0x40	/* start command */
#define SMBHCTRL_PROTO		0x1c	/* command protocol mask */
#define SMBHCTRL_QUICK		0x00
#define SMBHCTRL_SENDRECV	0x04
#define SMBHCTRL_BYTE		0x08
#define SMBHCTRL_WORD		0x0c
#define SMBHCTRL_BLOCK		0x14
#define SMBHCTRL_KILL		0x02	/* stop the current transaction */
#define SMBHCTRL_ENABLE		0x01	/* enable interrupts */

#define SMBSCTRL_ENABLE		0x01	/* enable slave */


/*
 * VIA8233 definitions
 */

#define VIAPM_8233_BASE		0xD0

static int
viapm_586b_probe(device_t dev)
{
	struct viapm_softc *viapm = (struct viapm_softc *)device_get_softc(dev);
	u_int32_t l;
	u_int16_t s;
	u_int8_t c;

	switch (pci_get_devid(dev)) {
	case VIA_586B_PMU_ID:

		bzero(viapm, sizeof(struct viapm_softc));

		l = pci_read_config(dev, VIAPM_586B_REVID, 1);
		switch (l) {
		case VIAPM_586B_OEM_REV_E:
			viapm->type = VIAPM_TYP_586B_3040E;
			viapm->iorid = VIAPM_586B_3040E_BASE;

			/* Activate IO block access */
			s = pci_read_config(dev, VIAPM_586B_3040E_ACTIV, 2);
			pci_write_config(dev, VIAPM_586B_3040E_ACTIV, s | 0x1, 2);
			break;

		case VIAPM_586B_OEM_REV_F:
		case VIAPM_586B_PROD_REV_A:
		default:
			viapm->type = VIAPM_TYP_586B_3040F;
			viapm->iorid = VIAPM_586B_3040F_BASE;

			/* Activate IO block access */
			c = pci_read_config(dev, VIAPM_586B_3040F_ACTIV, 1);
			pci_write_config(dev, VIAPM_586B_3040F_ACTIV, c | 0x80, 1);
			break;
		}

		viapm->base = pci_read_config(dev, viapm->iorid, 4) &
				VIAPM_586B_BA_MASK;

		/*
		 * We have to set the I/O resources by hand because it is
		 * described outside the viapmope of the traditional maps
		 */
		if (bus_set_resource(dev, SYS_RES_IOPORT, viapm->iorid,
							viapm->base, 256)) {
			device_printf(dev, "could not set bus resource\n");
			return ENXIO;
		}
		device_set_desc(dev, "VIA VT82C586B Power Management Unit");
		return (BUS_PROBE_DEFAULT);

	default:
		break;
	}

	return ENXIO;
}


static int
viapm_pro_probe(device_t dev)
{
	struct viapm_softc *viapm = (struct viapm_softc *)device_get_softc(dev);
#ifdef VIAPM_BASE_ADDR
	u_int32_t l;
#endif
	u_int32_t base_cfgreg;
	char *desc;

	switch (pci_get_devid(dev)) {
	case VIA_596A_PMU_ID:
		desc = "VIA VT82C596A Power Management Unit";
		viapm->type = VIAPM_TYP_596B;
		base_cfgreg = VIAPM_PRO_BASE;
		goto viapro;

	case VIA_596B_PMU_ID:
		desc = "VIA VT82C596B Power Management Unit";
		viapm->type = VIAPM_TYP_596B;
		base_cfgreg = VIAPM_PRO_BASE;
		goto viapro;

	case VIA_686A_PMU_ID:
		desc = "VIA VT82C686A Power Management Unit";
		viapm->type = VIAPM_TYP_686A;
		base_cfgreg = VIAPM_PRO_BASE;
		goto viapro;

	case VIA_8233_PMU_ID:
	case VIA_8233A_PMU_ID:
		desc = "VIA VT8233 Power Management Unit";
		viapm->type = VIAPM_TYP_UNKNOWN;
		base_cfgreg = VIAPM_8233_BASE;
		goto viapro;

	viapro:

#ifdef VIAPM_BASE_ADDR
		/* force VIAPM I/O base address */

		/* enable the SMBus controller function */
		l = pci_read_config(dev, VIAPM_PRO_SMBCTRL, 1);
		pci_write_config(dev, VIAPM_PRO_SMBCTRL, l | 1, 1);

		/* write the base address */
		pci_write_config(dev, base_cfgreg,
				 VIAPM_BASE_ADDR & VIAPM_PRO_BA_MASK, 4);
#endif

		viapm->base = pci_read_config(dev, base_cfgreg, 4) & VIAPM_PRO_BA_MASK;

		/*
		 * We have to set the I/O resources by hand because it is
		 * described outside the viapmope of the traditional maps
		 */
		viapm->iorid = base_cfgreg;
		if (bus_set_resource(dev, SYS_RES_IOPORT, viapm->iorid,
				     viapm->base, 16)) {
			device_printf(dev, "could not set bus resource 0x%x\n",
					viapm->base);
			return ENXIO;
		}

		if (1 || bootverbose) {
			device_printf(dev, "SMBus I/O base at 0x%x\n", viapm->base);
		}

		device_set_desc(dev, desc);
		return (BUS_PROBE_DEFAULT);

	default:
		break;
	}

	return ENXIO;
}

static int
viapm_pro_attach(device_t dev)
{
	struct viapm_softc *viapm = (struct viapm_softc *)device_get_softc(dev);
	u_int32_t l;

	if (!(viapm->iores = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
		&viapm->iorid, RF_ACTIVE))) {
		device_printf(dev, "could not allocate bus space\n");
		goto error;
	}
	viapm->st = rman_get_bustag(viapm->iores);
	viapm->sh = rman_get_bushandle(viapm->iores);

#if notyet
	/* force irq 9 */
	l = pci_read_config(dev, VIAPM_PRO_SMBCTRL, 1);
	pci_write_config(dev, VIAPM_PRO_SMBCTRL, l | 0x80, 1);

	viapm->irqrid = 0;
	if (!(viapm->irqres = bus_alloc_resource(dev, SYS_RES_IRQ,
				&viapm->irqrid, 9, 9, 1,
				RF_SHAREABLE | RF_ACTIVE))) {
		device_printf(dev, "could not allocate irq\n");
		goto error;
	}

	if (bus_setup_intr(dev, viapm->irqres, INTR_TYPE_MISC,
			(driver_intr_t *) viasmb_intr, viapm, &viapm->irqih)) {
		device_printf(dev, "could not setup irq\n");
		goto error;
	}
#endif

	if (1 | bootverbose) {
		l = pci_read_config(dev, VIAPM_PRO_REVID, 1);
		device_printf(dev, "SMBus revision code 0x%x\n", l);
	}

	viapm->smbus = device_add_child(dev, "smbus", -1);

	/* probe and attach the smbus */
	bus_generic_attach(dev);

	/* disable slave function */
	VIAPM_OUTB(SMBSCTRL, VIAPM_INB(SMBSCTRL) & ~SMBSCTRL_ENABLE);

	/* enable the SMBus controller function */
	l = pci_read_config(dev, VIAPM_PRO_SMBCTRL, 1);
	pci_write_config(dev, VIAPM_PRO_SMBCTRL, l | 1, 1);

#if notyet
	/* enable interrupts */
	VIAPM_OUTB(SMBHCTRL, VIAPM_INB(SMBHCTRL) | SMBHCTRL_ENABLE);
#endif

	return 0;

error:
	if (viapm->iores)
		bus_release_resource(dev, SYS_RES_IOPORT, viapm->iorid, viapm->iores);
#if notyet
	if (viapm->irqres)
		bus_release_resource(dev, SYS_RES_IRQ, viapm->irqrid, viapm->irqres);
#endif

	return ENXIO;
}

static int
viapm_586b_attach(device_t dev)
{
	struct viapm_softc *viapm = (struct viapm_softc *)device_get_softc(dev);
	
	if (!(viapm->iores = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
		&viapm->iorid, RF_ACTIVE | RF_SHAREABLE))) {
		device_printf(dev, "could not allocate bus resource\n");
		return ENXIO;
	}
	viapm->st = rman_get_bustag(viapm->iores);
	viapm->sh = rman_get_bushandle(viapm->iores);

	VIAPM_OUTB(GPIO_DIR, VIAPM_INB(GPIO_DIR) | VIAPM_SCL | VIAPM_SDA);

	/* add generic bit-banging code */
	if (!(viapm->iicbb = device_add_child(dev, "iicbb", -1)))
		goto error;

	bus_generic_attach(dev);

	return 0;

error:
	if (viapm->iores)
		bus_release_resource(dev, SYS_RES_IOPORT,
					viapm->iorid, viapm->iores);
	return ENXIO;
}

static int
viapm_586b_detach(device_t dev)
{
	struct viapm_softc *viapm = (struct viapm_softc *)device_get_softc(dev);
	int error;

	bus_generic_detach(dev);
	if (viapm->iicbb) {
		device_delete_child(dev, viapm->iicbb);
	}

	if (viapm->iores && (error = bus_release_resource(dev, SYS_RES_IOPORT,
						viapm->iorid, viapm->iores)))
		return (error);

	return 0;
}

static int
viapm_pro_detach(device_t dev)
{
	struct viapm_softc *viapm = (struct viapm_softc *)device_get_softc(dev);
	int error;

	bus_generic_detach(dev);
	if (viapm->smbus) {
		device_delete_child(dev, viapm->smbus);
	}

	if ((error = bus_release_resource(dev, SYS_RES_IOPORT,
				viapm->iorid, viapm->iores)))
		return (error);

#if notyet
	if ((error = bus_release_resource(dev, SYS_RES_IRQ,
					viapm->irqrid, viapm->irqres))
		return (error);
#endif

	return 0;
}

static int
viabb_callback(device_t dev, int index, caddr_t *data)
{
	return 0;
}

static void
viabb_setscl(device_t dev, int ctrl)
{
	struct viapm_softc *viapm = device_get_softc(dev);
	u_char val;

	val = VIAPM_INB(GPIO_VAL);

	if (ctrl)
		val |= VIAPM_SCL;
	else
		val &= ~VIAPM_SCL;

	VIAPM_OUTB(GPIO_VAL, val);

	return;
}

static void
viabb_setsda(device_t dev, int data)
{
	struct viapm_softc *viapm = device_get_softc(dev);
	u_char val;

	val = VIAPM_INB(GPIO_VAL);

	if (data)
		val |= VIAPM_SDA;
	else
		val &= ~VIAPM_SDA;

	VIAPM_OUTB(GPIO_VAL, val);

	return;
}
	
static int
viabb_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	/* reset bus */
	viabb_setsda(dev, 1);
	viabb_setscl(dev, 1);

	return (IIC_ENOADDR);
}

static int
viabb_getscl(device_t dev)
{
	struct viapm_softc *viapm = device_get_softc(dev);

	return ((VIAPM_INB(EXTSMI_VAL) & VIAPM_SCL) != 0);
}

static int
viabb_getsda(device_t dev)
{
	struct viapm_softc *viapm = device_get_softc(dev);

	return ((VIAPM_INB(EXTSMI_VAL) & VIAPM_SDA) != 0);
}

static int
viapm_abort(struct viapm_softc *viapm)
{
	VIAPM_OUTB(SMBHCTRL, SMBHCTRL_KILL);
	DELAY(10);

	return (0);
}

static int
viapm_clear(struct viapm_softc *viapm)
{
	VIAPM_OUTB(SMBHST, SMBHST_FAILED | SMBHST_COLLID |
		SMBHST_ERROR | SMBHST_INTR);
	DELAY(10);

	return (0);
}

static int
viapm_busy(struct viapm_softc *viapm)
{
	u_char sts;

	sts = VIAPM_INB(SMBHST);

	VIAPM_DEBUG(printf("viapm: idle? STS=0x%x\n", sts));

	return (sts & SMBHST_BUSY);
}

/*
 * Poll the SMBus controller
 */
static int
viapm_wait(struct viapm_softc *viapm)
{
	int count = 10000;
	u_char sts = 0;
	int error;

	/* wait for command to complete and SMBus controller is idle */
	while(count--) {
		DELAY(10);
		sts = VIAPM_INB(SMBHST);

		/* check if the controller is processing a command */
		if (!(sts & SMBHST_BUSY) && (sts & SMBHST_INTR))
			break;
	}

	VIAPM_DEBUG(printf("viapm: SMBHST=0x%x\n", sts));

	error = SMB_ENOERR;

	if (!count)
		error |= SMB_ETIMEOUT;

	if (sts & SMBHST_FAILED)
		error |= SMB_EABORT;

	if (sts & SMBHST_COLLID)
		error |= SMB_ENOACK;

	if (sts & SMBHST_ERROR)
		error |= SMB_EBUSERR;

	if (error != SMB_ENOERR)
		viapm_abort(viapm);

	viapm_clear(viapm);

	return (error);
}

static int
viasmb_callback(device_t dev, int index, caddr_t *data)
{
	int error = 0;

	switch (index) {
	case SMB_REQUEST_BUS:
	case SMB_RELEASE_BUS:
		/* ok, bus allocation accepted */
		break;
	default:
		error = EINVAL;
	}

	return (error);
}

static int
viasmb_quick(device_t dev, u_char slave, int how)
{
	struct viapm_softc *viapm = (struct viapm_softc *)device_get_softc(dev);
	int error;

	viapm_clear(viapm);
	if (viapm_busy(viapm))
		return (EBUSY);

	switch (how) {
	case SMB_QWRITE:
		VIAPM_DEBUG(printf("viapm: QWRITE to 0x%x", slave));
		VIAPM_OUTB(SMBHADDR, slave & ~LSB);
		break;
	case SMB_QREAD:
		VIAPM_DEBUG(printf("viapm: QREAD to 0x%x", slave));
		VIAPM_OUTB(SMBHADDR, slave | LSB);
		break;
	default:
		panic("%s: unknown QUICK command (%x)!", __func__, how);
	}

	VIAPM_OUTB(SMBHCTRL, SMBHCTRL_START | SMBHCTRL_QUICK);

	error = viapm_wait(viapm);

	return (error);
}

static int
viasmb_sendb(device_t dev, u_char slave, char byte)
{
	struct viapm_softc *viapm = (struct viapm_softc *)device_get_softc(dev);
	int error;

	viapm_clear(viapm);
	if (viapm_busy(viapm))
		return (EBUSY);

	VIAPM_OUTB(SMBHADDR, slave & ~ LSB);
	VIAPM_OUTB(SMBHCMD, byte);

	VIAPM_OUTB(SMBHCTRL, SMBHCTRL_START | SMBHCTRL_SENDRECV);

	error = viapm_wait(viapm);

	VIAPM_DEBUG(printf("viapm: SENDB to 0x%x, byte=0x%x, error=0x%x\n", slave, byte, error));

	return (error);
}

static int
viasmb_recvb(device_t dev, u_char slave, char *byte)
{
	struct viapm_softc *viapm = (struct viapm_softc *)device_get_softc(dev);
	int error;

	viapm_clear(viapm);
	if (viapm_busy(viapm))
		return (EBUSY);

	VIAPM_OUTB(SMBHADDR, slave | LSB);

	VIAPM_OUTB(SMBHCTRL, SMBHCTRL_START | SMBHCTRL_SENDRECV);

	if ((error = viapm_wait(viapm)) == SMB_ENOERR)
		*byte = VIAPM_INB(SMBHDATA0);

	VIAPM_DEBUG(printf("viapm: RECVB from 0x%x, byte=0x%x, error=0x%x\n", slave, *byte, error));

	return (error);
}

static int
viasmb_writeb(device_t dev, u_char slave, char cmd, char byte)
{
	struct viapm_softc *viapm = (struct viapm_softc *)device_get_softc(dev);
	int error;

	viapm_clear(viapm);
	if (viapm_busy(viapm))
		return (EBUSY);

	VIAPM_OUTB(SMBHADDR, slave & ~ LSB);
	VIAPM_OUTB(SMBHCMD, cmd);
	VIAPM_OUTB(SMBHDATA0, byte);

	VIAPM_OUTB(SMBHCTRL, SMBHCTRL_START | SMBHCTRL_BYTE);

	error = viapm_wait(viapm);

	VIAPM_DEBUG(printf("viapm: WRITEB to 0x%x, cmd=0x%x, byte=0x%x, error=0x%x\n", slave, cmd, byte, error));

	return (error);
}

static int
viasmb_readb(device_t dev, u_char slave, char cmd, char *byte)
{
	struct viapm_softc *viapm = (struct viapm_softc *)device_get_softc(dev);
	int error;

	viapm_clear(viapm);
	if (viapm_busy(viapm))
		return (EBUSY);

	VIAPM_OUTB(SMBHADDR, slave | LSB);
	VIAPM_OUTB(SMBHCMD, cmd);

	VIAPM_OUTB(SMBHCTRL, SMBHCTRL_START | SMBHCTRL_BYTE);

	if ((error = viapm_wait(viapm)) == SMB_ENOERR)
		*byte = VIAPM_INB(SMBHDATA0);

	VIAPM_DEBUG(printf("viapm: READB from 0x%x, cmd=0x%x, byte=0x%x, error=0x%x\n", slave, cmd, *byte, error));

	return (error);
}

static int
viasmb_writew(device_t dev, u_char slave, char cmd, short word)
{
	struct viapm_softc *viapm = (struct viapm_softc *)device_get_softc(dev);
	int error;

	viapm_clear(viapm);
	if (viapm_busy(viapm))
		return (EBUSY);

	VIAPM_OUTB(SMBHADDR, slave & ~ LSB);
	VIAPM_OUTB(SMBHCMD, cmd);
	VIAPM_OUTB(SMBHDATA0, word & 0x00ff);
	VIAPM_OUTB(SMBHDATA1, (word & 0xff00) >> 8);

	VIAPM_OUTB(SMBHCTRL, SMBHCTRL_START | SMBHCTRL_WORD);

	error = viapm_wait(viapm);

	VIAPM_DEBUG(printf("viapm: WRITEW to 0x%x, cmd=0x%x, word=0x%x, error=0x%x\n", slave, cmd, word, error));

	return (error);
}

static int
viasmb_readw(device_t dev, u_char slave, char cmd, short *word)
{
	struct viapm_softc *viapm = (struct viapm_softc *)device_get_softc(dev);
	int error;
	u_char high, low;

	viapm_clear(viapm);
	if (viapm_busy(viapm))
		return (EBUSY);

	VIAPM_OUTB(SMBHADDR, slave | LSB);
	VIAPM_OUTB(SMBHCMD, cmd);

	VIAPM_OUTB(SMBHCTRL, SMBHCTRL_START | SMBHCTRL_WORD);

	if ((error = viapm_wait(viapm)) == SMB_ENOERR) {
		low = VIAPM_INB(SMBHDATA0);
		high = VIAPM_INB(SMBHDATA1);

		*word = ((high & 0xff) << 8) | (low & 0xff);
	}

	VIAPM_DEBUG(printf("viapm: READW from 0x%x, cmd=0x%x, word=0x%x, error=0x%x\n", slave, cmd, *word, error));

	return (error);
}

static int
viasmb_bwrite(device_t dev, u_char slave, char cmd, u_char count, char *buf)
{
	struct viapm_softc *viapm = (struct viapm_softc *)device_get_softc(dev);
	u_char remain, len, i;
	int error = SMB_ENOERR;

	viapm_clear(viapm);
	if (viapm_busy(viapm))
		return (EBUSY);

	remain = count;
	while (remain) {
		len = min(remain, 32);

		VIAPM_OUTB(SMBHADDR, slave & ~LSB);
		VIAPM_OUTB(SMBHCMD, cmd);
		VIAPM_OUTB(SMBHDATA0, len);
		i = VIAPM_INB(SMBHCTRL);

		/* fill the 32-byte internal buffer */
		for (i=0; i<len; i++) {
			VIAPM_OUTB(SMBHBLOCK, buf[count-remain+i]);
			DELAY(2);
		}
		VIAPM_OUTB(SMBHCMD, cmd);
		VIAPM_OUTB(SMBHCTRL, SMBHCTRL_START | SMBHCTRL_BLOCK);

		if ((error = viapm_wait(viapm)) != SMB_ENOERR)
			goto error;

		remain -= len;
	}

error:
	VIAPM_DEBUG(printf("viapm: WRITEBLK to 0x%x, count=0x%x, cmd=0x%x, error=0x%x", slave, count, cmd, error));

	return (error);

}

static int
viasmb_bread(device_t dev, u_char slave, char cmd, u_char count, char *buf)
{
	struct viapm_softc *viapm = (struct viapm_softc *)device_get_softc(dev);
	u_char remain, len, i;
	int error = SMB_ENOERR;

	viapm_clear(viapm);
	if (viapm_busy(viapm))
		return (EBUSY);

	remain = count;
	while (remain) {
		VIAPM_OUTB(SMBHADDR, slave | LSB);
		VIAPM_OUTB(SMBHCMD, cmd);
		VIAPM_OUTB(SMBHCTRL, SMBHCTRL_START | SMBHCTRL_BLOCK);

		if ((error = viapm_wait(viapm)) != SMB_ENOERR)
			goto error;

		len = VIAPM_INB(SMBHDATA0);
		i = VIAPM_INB(SMBHCTRL); 		/* reset counter */

		len = min(len, remain);

		/* read the 32-byte internal buffer */
		for (i=0; i<len; i++) {
			buf[count-remain+i] = VIAPM_INB(SMBHBLOCK);
			DELAY(2);
		}

		remain -= len;
	}
error:
	VIAPM_DEBUG(printf("viapm: READBLK to 0x%x, count=0x%x, cmd=0x%x, error=0x%x", slave, count, cmd, error));

	return (error);
}

static device_method_t viapm_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		viapm_586b_probe),
	DEVMETHOD(device_attach,	viapm_586b_attach),
	DEVMETHOD(device_detach,	viapm_586b_detach),

	/* iicbb interface */
	DEVMETHOD(iicbb_callback,	viabb_callback),
	DEVMETHOD(iicbb_setscl,		viabb_setscl),
	DEVMETHOD(iicbb_setsda,		viabb_setsda),
	DEVMETHOD(iicbb_getscl,		viabb_getscl),
	DEVMETHOD(iicbb_getsda,		viabb_getsda),
	DEVMETHOD(iicbb_reset,		viabb_reset),

	{ 0, 0 }
};

static driver_t viapm_driver = {
	"viapm",
	viapm_methods,
	sizeof(struct viapm_softc),
};

static device_method_t viapropm_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		viapm_pro_probe),
	DEVMETHOD(device_attach,	viapm_pro_attach),
	DEVMETHOD(device_detach,	viapm_pro_detach),

	/* smbus interface */
	DEVMETHOD(smbus_callback,	viasmb_callback),
	DEVMETHOD(smbus_quick,		viasmb_quick),
	DEVMETHOD(smbus_sendb,		viasmb_sendb),
	DEVMETHOD(smbus_recvb,		viasmb_recvb),
	DEVMETHOD(smbus_writeb,		viasmb_writeb),
	DEVMETHOD(smbus_readb,		viasmb_readb),
	DEVMETHOD(smbus_writew,		viasmb_writew),
	DEVMETHOD(smbus_readw,		viasmb_readw),
	DEVMETHOD(smbus_bwrite,		viasmb_bwrite),
	DEVMETHOD(smbus_bread,		viasmb_bread),
	
	{ 0, 0 }
};

static driver_t viapropm_driver = {
	"viapropm",
	viapropm_methods,
	sizeof(struct viapm_softc),
};

DRIVER_MODULE(viapm, pci, viapm_driver, viapm_devclass, 0, 0);
DRIVER_MODULE(viapropm, pci, viapropm_driver, viapropm_devclass, 0, 0);

MODULE_DEPEND(viapm, pci, 1, 1, 1);
MODULE_DEPEND(viaprom, pci, 1, 1, 1);
MODULE_DEPEND(viapm, iicbb, IICBB_MINVER, IICBB_PREFVER, IICBB_MAXVER);
MODULE_DEPEND(viapropm, smbus, SMBUS_MINVER, SMBUS_PREFVER, SMBUS_MAXVER);
MODULE_VERSION(viapm, 1);
