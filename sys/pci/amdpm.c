/*-
 * Copyright (c) 2000 Matthew C. Forman
 *
 * Based (heavily) on alpm.c which is:
 *
 * Copyright (c) 1998, 1999 Nicolas Souchu
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
 *
 */

/*
 * Power management function/SMBus function support for the AMD 756 chip.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/uio.h>

#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include <dev/iicbus/iiconf.h>
#include <dev/smbus/smbconf.h>
#include "smbus_if.h"

#define AMDPM_DEBUG(x)	if (amdpm_debug) (x)

#ifdef DEBUG
static int amdpm_debug = 1;
#else
static int amdpm_debug = 0;
#endif

#define AMDPM_VENDORID_AMD 0x1022
#define AMDPM_DEVICEID_AMD756PM 0x740b

/* PCI Configuration space registers */
#define AMDPCI_PMBASE 0x58

#define AMDPCI_GEN_CONFIG_PM 0x41
#define AMDPCI_PMIOEN (1<<7)

#define AMDPCI_SCIINT_CONFIG_PM 0x42
#define AMDPCI_SCISEL_IRQ11 11

#define AMDPCI_REVID 0x08

/*
 * I/O registers.
 * Base address programmed via AMDPCI_PMBASE.
 */
#define AMDSMB_GLOBAL_STATUS 0xE0
#define AMDSMB_GS_TO_STS (1<<5)
#define AMDSMB_GS_HCYC_STS (1<<4)
#define AMDSMB_GS_HST_STS (1<<3)
#define AMDSMB_GS_PRERR_STS (1<<2)
#define AMDSMB_GS_COL_STS (1<<1)
#define AMDSMB_GS_ABRT_STS (1<<0)
#define AMDSMB_GS_CLEAR_STS (AMDSMB_GS_TO_STS|AMDSMB_GS_HCYC_STS|AMDSMB_GS_PRERR_STS|AMDSMB_GS_COL_STS|AMDSMB_GS_ABRT_STS)

#define AMDSMB_GLOBAL_ENABLE 0xE2
#define AMDSMB_GE_ABORT (1<<5)
#define AMDSMB_GE_HCYC_EN (1<<4)
#define AMDSMB_GE_HOST_STC (1<<3)
#define AMDSMB_GE_CYC_QUICK 0
#define AMDSMB_GE_CYC_BYTE 1
#define AMDSMB_GE_CYC_BDATA 2
#define AMDSMB_GE_CYC_WDATA 3
#define AMDSMB_GE_CYC_PROCCALL 4
#define AMDSMB_GE_CYC_BLOCK 5

#define AMDSMB_HSTADDR 0xE4
#define AMDSMB_HSTDATA 0xE6
#define AMDSMB_HSTCMD 0xE8
#define AMDSMB_HSTDFIFO 0xE9
#define AMDSMB_HSLVDATA 0xEA
#define AMDSMB_HSLVDA 0xEC
#define AMDSMB_HSLVDDR 0xEE
#define AMDSMB_SNPADDR 0xEF

struct amdpm_softc {
	int base;
	int rid;
	struct resource *res;
        bus_space_tag_t smbst;
        bus_space_handle_t smbsh;
};

struct amdsmb_softc {
	int base;
	device_t smbus;
	struct amdpm_softc *amdpm;
};

#define AMDPM_SMBINB(amdsmb,register) \
	(bus_space_read_1(amdsmb->amdpm->smbst, amdsmb->amdpm->smbsh, register))
#define AMDPM_SMBOUTB(amdsmb,register,value) \
	(bus_space_write_1(amdsmb->amdpm->smbst, amdsmb->amdpm->smbsh, register, value))
#define AMDPM_SMBINW(amdsmb,register) \
	(bus_space_read_2(amdsmb->amdpm->smbst, amdsmb->amdpm->smbsh, register))
#define AMDPM_SMBOUTW(amdsmb,register,value) \
	(bus_space_write_2(amdsmb->amdpm->smbst, amdsmb->amdpm->smbsh, register, value))

static int amdsmb_probe(device_t);
static int amdsmb_attach(device_t);
static int amdsmb_smb_callback(device_t, int, caddr_t *);
static int amdsmb_smb_quick(device_t dev, u_char slave, int how);
static int amdsmb_smb_sendb(device_t dev, u_char slave, char byte);
static int amdsmb_smb_recvb(device_t dev, u_char slave, char *byte);
static int amdsmb_smb_writeb(device_t dev, u_char slave, char cmd, char byte);
static int amdsmb_smb_readb(device_t dev, u_char slave, char cmd, char *byte);
static int amdsmb_smb_writew(device_t dev, u_char slave, char cmd, short word);
static int amdsmb_smb_readw(device_t dev, u_char slave, char cmd, short *word);
static int amdsmb_smb_bwrite(device_t dev, u_char slave, char cmd, u_char count, char *buf);
static int amdsmb_smb_bread(device_t dev, u_char slave, char cmd, u_char count, char *byte);

static int amdpm_probe(device_t);
static int amdpm_attach(device_t);


static int
amdpm_probe(device_t dev)
{
	u_long base;
	
	if ((pci_get_vendor(dev) == AMDPM_VENDORID_AMD) &&
	    (pci_get_device(dev) == AMDPM_DEVICEID_AMD756PM)) {
	      device_set_desc(dev, "AMD 756 Power Management Controller");
	      
	      /* 
	       * We have to do this, since the BIOS won't give us the
	       * resource info (not mine, anyway).
	       */
	      base = pci_read_config(dev, AMDPCI_PMBASE, 4);
	      base &= 0xff00;
	      bus_set_resource(dev, SYS_RES_IOPORT, AMDPCI_PMBASE, base, 256);
	      return (0);
	}
	return ENXIO;
}

static int
amdpm_attach(device_t dev)
{
	struct amdpm_softc *amdpm_sc = device_get_softc(dev);
	u_char val_b;
	int unit = device_get_unit(dev);
	device_t smbinterface;
	
	/* Enable I/O block access */
	val_b = pci_read_config(dev, AMDPCI_GEN_CONFIG_PM, 1);
	pci_write_config(dev, AMDPCI_GEN_CONFIG_PM, val_b | AMDPCI_PMIOEN, 1);

	/* Allocate I/O space */
	amdpm_sc->rid = AMDPCI_PMBASE;
	amdpm_sc->res = bus_alloc_resource(dev, SYS_RES_IOPORT, &amdpm_sc->rid, 0, ~0, 1, RF_ACTIVE);
	
	if (amdpm_sc->res == NULL) {
		device_printf(dev, "could not map i/o space\n");
		return (ENXIO);
	}	     

	amdpm_sc->smbst = rman_get_bustag(amdpm_sc->res);
	amdpm_sc->smbsh = rman_get_bushandle(amdpm_sc->res);
	
	smbinterface = device_add_child(dev, "amdsmb", unit);
	if (!smbinterface)
		device_printf(dev, "could not add SMBus device\n");
	else
		device_probe_and_attach(smbinterface);

	return (0);
}

static int
amdsmb_probe(device_t dev)
{
	struct amdsmb_softc *amdsmb_sc = (struct amdsmb_softc *)device_get_softc(dev);

	device_set_desc(dev, "AMD 756 SMBus interface");
	device_printf(dev, "AMD 756 SMBus interface\n");

	/* Allocate a new smbus device */
	amdsmb_sc->smbus = device_add_child(dev, "smbus", -1);
	if (!amdsmb_sc->smbus)
		return (EINVAL);

	bus_generic_attach(dev);

	return (0);
}

static int
amdsmb_attach(device_t dev)
{
	struct amdsmb_softc *amdsmb_sc = (struct amdsmb_softc *)device_get_softc(dev);

	amdsmb_sc->amdpm = device_get_softc(device_get_parent(dev));
	
	/* Probe and attach the smbus */
	device_probe_and_attach(amdsmb_sc->smbus);

	return (0);
}

static int
amdsmb_smb_callback(device_t dev, int index, caddr_t *data)
{
	int error = 0;

	switch (index) {
	case SMB_REQUEST_BUS:
	case SMB_RELEASE_BUS:
		break;
	default:
		error = EINVAL;
	}

	return (error);
}

static int
amdsmb_clear(struct amdsmb_softc *sc)
{
	AMDPM_SMBOUTW(sc, AMDSMB_GLOBAL_STATUS, AMDSMB_GS_CLEAR_STS);
	DELAY(10);

	return (0);
}

#if 0
static int
amdsmb_abort(struct amdsmb_softc *sc)
{
	u_short l;
	
	l = AMDPM_SMBINW(sc, AMDSMB_GLOBAL_ENABLE);
	AMDPM_SMBOUTW(sc, AMDSMB_GLOBAL_ENABLE, l | AMDSMB_GE_ABORT);

	return (0);
}
#endif

static int
amdsmb_idle(struct amdsmb_softc *sc)
{
	u_short sts;

	sts = AMDPM_SMBINW(sc, AMDSMB_GLOBAL_STATUS);

	AMDPM_DEBUG(printf("amdpm: busy? STS=0x%x\n", sts));

	return (~(sts & AMDSMB_GS_HST_STS));
}

/*
 * Poll the SMBus controller
 */
static int
amdsmb_wait(struct amdsmb_softc *sc)
{
	int count = 10000;
	u_short sts = 0;
	int error;

	/* Wait for command to complete (SMBus controller is idle) */
	while(count--) {
		DELAY(10);
		sts = AMDPM_SMBINW(sc, AMDSMB_GLOBAL_STATUS);
		if (!(sts & AMDSMB_GS_HST_STS))
			break;
	}

	AMDPM_DEBUG(printf("amdpm: STS=0x%x (count=%d)\n", sts, count));

	error = SMB_ENOERR;

	if (!count)
		error |= SMB_ETIMEOUT;

	if (sts & AMDSMB_GS_ABRT_STS)
		error |= SMB_EABORT;

	if (sts & AMDSMB_GS_COL_STS)
		error |= SMB_ENOACK;

	if (sts & AMDSMB_GS_PRERR_STS)
		error |= SMB_EBUSERR;

	if (error != SMB_ENOERR)
		amdsmb_clear(sc);

	return (error);
}

static int
amdsmb_smb_quick(device_t dev, u_char slave, int how)
{
	struct amdsmb_softc *sc = (struct amdsmb_softc *)device_get_softc(dev);
	int error;
	u_short l;

	amdsmb_clear(sc);
	if (!amdsmb_idle(sc))
		return (EBUSY);

	switch (how) {
	case SMB_QWRITE:
		AMDPM_DEBUG(printf("amdpm: QWRITE to 0x%x", slave));
		AMDPM_SMBOUTW(sc, AMDSMB_HSTADDR, slave & ~LSB);
		break;
	case SMB_QREAD:
		AMDPM_DEBUG(printf("amdpm: QREAD to 0x%x", slave));
		AMDPM_SMBOUTW(sc, AMDSMB_HSTADDR, slave | LSB);
		break;
	default:
		panic("%s: unknown QUICK command (%x)!", __func__, how);
	}
	l = AMDPM_SMBINW(sc, AMDSMB_GLOBAL_ENABLE);
	AMDPM_SMBOUTW(sc, AMDSMB_GLOBAL_ENABLE, (l & 0xfff8) | AMDSMB_GE_CYC_QUICK | AMDSMB_GE_HOST_STC);

	error = amdsmb_wait(sc);

	AMDPM_DEBUG(printf(", error=0x%x\n", error));

	return (error);
}

static int
amdsmb_smb_sendb(device_t dev, u_char slave, char byte)
{
	struct amdsmb_softc *sc = (struct amdsmb_softc *)device_get_softc(dev);
	int error;
	u_short l;

	amdsmb_clear(sc);
	if (!amdsmb_idle(sc))
		return (SMB_EBUSY);

	AMDPM_SMBOUTW(sc, AMDSMB_HSTADDR, slave & ~LSB);
	AMDPM_SMBOUTW(sc, AMDSMB_HSTDATA, byte);
	l = AMDPM_SMBINW(sc, AMDSMB_GLOBAL_ENABLE);
	AMDPM_SMBOUTW(sc, AMDSMB_GLOBAL_ENABLE, (l & 0xfff8) | AMDSMB_GE_CYC_BYTE | AMDSMB_GE_HOST_STC);

	error = amdsmb_wait(sc);

	AMDPM_DEBUG(printf("amdpm: SENDB to 0x%x, byte=0x%x, error=0x%x\n", slave, byte, error));

	return (error);
}

static int
amdsmb_smb_recvb(device_t dev, u_char slave, char *byte)
{
	struct amdsmb_softc *sc = (struct amdsmb_softc *)device_get_softc(dev);
	int error;
	u_short l;

	amdsmb_clear(sc);
	if (!amdsmb_idle(sc))
		return (SMB_EBUSY);

	AMDPM_SMBOUTW(sc, AMDSMB_HSTADDR, slave | LSB);
	l = AMDPM_SMBINW(sc, AMDSMB_GLOBAL_ENABLE);
	AMDPM_SMBOUTW(sc, AMDSMB_GLOBAL_ENABLE, (l & 0xfff8) | AMDSMB_GE_CYC_BYTE | AMDSMB_GE_HOST_STC);

	if ((error = amdsmb_wait(sc)) == SMB_ENOERR)
		*byte = AMDPM_SMBINW(sc, AMDSMB_HSTDATA);

	AMDPM_DEBUG(printf("amdpm: RECVB from 0x%x, byte=0x%x, error=0x%x\n", slave, *byte, error));

	return (error);
}

static int
amdsmb_smb_writeb(device_t dev, u_char slave, char cmd, char byte)
{
	struct amdsmb_softc *sc = (struct amdsmb_softc *)device_get_softc(dev);
	int error;
	u_short l;

	amdsmb_clear(sc);
	if (!amdsmb_idle(sc))
		return (SMB_EBUSY);

	AMDPM_SMBOUTW(sc, AMDSMB_HSTADDR, slave & ~LSB);
	AMDPM_SMBOUTW(sc, AMDSMB_HSTDATA, byte);
	AMDPM_SMBOUTB(sc, AMDSMB_HSTCMD, cmd);
	l = AMDPM_SMBINW(sc, AMDSMB_GLOBAL_ENABLE);
	AMDPM_SMBOUTW(sc, AMDSMB_GLOBAL_ENABLE, (l & 0xfff8) | AMDSMB_GE_CYC_BDATA | AMDSMB_GE_HOST_STC);

	error = amdsmb_wait(sc);

	AMDPM_DEBUG(printf("amdpm: WRITEB to 0x%x, cmd=0x%x, byte=0x%x, error=0x%x\n", slave, cmd, byte, error));

	return (error);
}

static int
amdsmb_smb_readb(device_t dev, u_char slave, char cmd, char *byte)
{
	struct amdsmb_softc *sc = (struct amdsmb_softc *)device_get_softc(dev);
	int error;
	u_short l;

	amdsmb_clear(sc);
	if (!amdsmb_idle(sc))
		return (SMB_EBUSY);

	AMDPM_SMBOUTW(sc, AMDSMB_HSTADDR, slave | LSB);
	AMDPM_SMBOUTB(sc, AMDSMB_HSTCMD, cmd);
	l = AMDPM_SMBINW(sc, AMDSMB_GLOBAL_ENABLE);
	AMDPM_SMBOUTW(sc, AMDSMB_GLOBAL_ENABLE, (l & 0xfff8) | AMDSMB_GE_CYC_BDATA | AMDSMB_GE_HOST_STC);

	if ((error = amdsmb_wait(sc)) == SMB_ENOERR)
		*byte = AMDPM_SMBINW(sc, AMDSMB_HSTDATA);

	AMDPM_DEBUG(printf("amdpm: READB from 0x%x, cmd=0x%x, byte=0x%x, error=0x%x\n", slave, cmd, *byte, error));

	return (error);
}

static int
amdsmb_smb_writew(device_t dev, u_char slave, char cmd, short word)
{
	struct amdsmb_softc *sc = (struct amdsmb_softc *)device_get_softc(dev);
	int error;
	u_short l;

	amdsmb_clear(sc);
	if (!amdsmb_idle(sc))
		return (SMB_EBUSY);

	AMDPM_SMBOUTW(sc, AMDSMB_HSTADDR, slave & ~LSB);
	AMDPM_SMBOUTW(sc, AMDSMB_HSTDATA, word);
	AMDPM_SMBOUTB(sc, AMDSMB_HSTCMD, cmd);
	l = AMDPM_SMBINW(sc, AMDSMB_GLOBAL_ENABLE);
	AMDPM_SMBOUTW(sc, AMDSMB_GLOBAL_ENABLE, (l & 0xfff8) | AMDSMB_GE_CYC_WDATA | AMDSMB_GE_HOST_STC);

	error = amdsmb_wait(sc);

	AMDPM_DEBUG(printf("amdpm: WRITEW to 0x%x, cmd=0x%x, word=0x%x, error=0x%x\n", slave, cmd, word, error));

	return (error);
}

static int
amdsmb_smb_readw(device_t dev, u_char slave, char cmd, short *word)
{
	struct amdsmb_softc *sc = (struct amdsmb_softc *)device_get_softc(dev);
	int error;
	u_short l;

	amdsmb_clear(sc);
	if (!amdsmb_idle(sc))
		return (SMB_EBUSY);

	AMDPM_SMBOUTW(sc, AMDSMB_HSTADDR, slave | LSB);
	AMDPM_SMBOUTB(sc, AMDSMB_HSTCMD, cmd);
	l = AMDPM_SMBINW(sc, AMDSMB_GLOBAL_ENABLE);
	AMDPM_SMBOUTW(sc, AMDSMB_GLOBAL_ENABLE, (l & 0xfff8) | AMDSMB_GE_CYC_WDATA | AMDSMB_GE_HOST_STC);

	if ((error = amdsmb_wait(sc)) == SMB_ENOERR)
		*word = AMDPM_SMBINW(sc, AMDSMB_HSTDATA);

	AMDPM_DEBUG(printf("amdpm: READW from 0x%x, cmd=0x%x, word=0x%x, error=0x%x\n", slave, cmd, *word, error));

	return (error);
}

static int
amdsmb_smb_bwrite(device_t dev, u_char slave, char cmd, u_char count, char *buf)
{
	struct amdsmb_softc *sc = (struct amdsmb_softc *)device_get_softc(dev);
	u_char remain, len, i;
	int error = SMB_ENOERR;
	u_short l;

	amdsmb_clear(sc);
	if(!amdsmb_idle(sc))
		return (SMB_EBUSY);

	remain = count;
	while (remain) {
		len = min(remain, 32);

		AMDPM_SMBOUTW(sc, AMDSMB_HSTADDR, slave & ~LSB);
	
		/*
		 * Do we have to reset the internal 32-byte buffer?
		 * Can't see how to do this from the data sheet.
		 */

		AMDPM_SMBOUTW(sc, AMDSMB_HSTDATA, len);

		/* Fill the 32-byte internal buffer */
		for (i=0; i<len; i++) {
			AMDPM_SMBOUTB(sc, AMDSMB_HSTDFIFO, buf[count-remain+i]);
			DELAY(2);
		}
		AMDPM_SMBOUTB(sc, AMDSMB_HSTCMD, cmd);
		l = AMDPM_SMBINW(sc, AMDSMB_GLOBAL_ENABLE);
		AMDPM_SMBOUTW(sc, AMDSMB_GLOBAL_ENABLE, (l & 0xfff8) | AMDSMB_GE_CYC_BLOCK | AMDSMB_GE_HOST_STC);

		if ((error = amdsmb_wait(sc)) != SMB_ENOERR)
			goto error;

		remain -= len;
	}

error:
	AMDPM_DEBUG(printf("amdpm: WRITEBLK to 0x%x, count=0x%x, cmd=0x%x, error=0x%x", slave, count, cmd, error));

	return (error);
}

static int
amdsmb_smb_bread(device_t dev, u_char slave, char cmd, u_char count, char *buf)
{
	struct amdsmb_softc *sc = (struct amdsmb_softc *)device_get_softc(dev);
	u_char remain, len, i;
	int error = SMB_ENOERR;
	u_short l;

	amdsmb_clear(sc);
	if (!amdsmb_idle(sc))
		return (SMB_EBUSY);

	remain = count;
	while (remain) {
		AMDPM_SMBOUTW(sc, AMDSMB_HSTADDR, slave | LSB);
	
		AMDPM_SMBOUTB(sc, AMDSMB_HSTCMD, cmd);

		l = AMDPM_SMBINW(sc, AMDSMB_GLOBAL_ENABLE);
		AMDPM_SMBOUTW(sc, AMDSMB_GLOBAL_ENABLE, (l & 0xfff8) | AMDSMB_GE_CYC_BLOCK | AMDSMB_GE_HOST_STC);
		
		if ((error = amdsmb_wait(sc)) != SMB_ENOERR)
			goto error;

		len = AMDPM_SMBINW(sc, AMDSMB_HSTDATA);

		/* Read the 32-byte internal buffer */
		for (i=0; i<len; i++) {
			buf[count-remain+i] = AMDPM_SMBINB(sc, AMDSMB_HSTDFIFO);
			DELAY(2);
		}

		remain -= len;
	}
error:
	AMDPM_DEBUG(printf("amdpm: READBLK to 0x%x, count=0x%x, cmd=0x%x, error=0x%x", slave, count, cmd, error));

	return (error);
}

static devclass_t amdpm_devclass;

static device_method_t amdpm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		amdpm_probe),
	DEVMETHOD(device_attach,	amdpm_attach),
	
	{ 0, 0 }
};

static driver_t amdpm_driver = {
	"amdpm",
	amdpm_methods,
	sizeof(struct amdpm_softc),
};

static devclass_t amdsmb_devclass;

static device_method_t amdsmb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		amdsmb_probe),
	DEVMETHOD(device_attach,	amdsmb_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	
	/* SMBus interface */
	DEVMETHOD(smbus_callback,	amdsmb_smb_callback),
	DEVMETHOD(smbus_quick,		amdsmb_smb_quick),
	DEVMETHOD(smbus_sendb,		amdsmb_smb_sendb),
	DEVMETHOD(smbus_recvb,		amdsmb_smb_recvb),
	DEVMETHOD(smbus_writeb,		amdsmb_smb_writeb),
	DEVMETHOD(smbus_readb,		amdsmb_smb_readb),
	DEVMETHOD(smbus_writew,		amdsmb_smb_writew),
	DEVMETHOD(smbus_readw,		amdsmb_smb_readw),
	DEVMETHOD(smbus_bwrite,		amdsmb_smb_bwrite),
	DEVMETHOD(smbus_bread,		amdsmb_smb_bread),
	
	{ 0, 0 }
};

static driver_t amdsmb_driver = {
	"amdsmb",
	amdsmb_methods,
	sizeof(struct amdsmb_softc),
};

DRIVER_MODULE(amdpm, pci, amdpm_driver, amdpm_devclass, 0, 0);
DRIVER_MODULE(amdsmb, amdpm, amdsmb_driver, amdsmb_devclass, 0, 0);
MODULE_DEPEND(amdpm, smbus, SMBUS_MINVER, SMBUS_PREFVER, SMBUS_MAXVER);
MODULE_VERSION(amdpm, 1);
