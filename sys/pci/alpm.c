/*-
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
 * Power Management support for the Acer M15x3 chipsets
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>

#include <machine/clock.h>

#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include <dev/iicbus/iiconf.h>
#include <dev/smbus/smbconf.h>
#include "smbus_if.h"

#include "alpm.h"

#define ALPM_DEBUG(x)	if (alpm_debug) (x)

#ifdef DEBUG
static int alpm_debug = 1;
#else
static int alpm_debug = 0;
#endif

#define ACER_M1543_PMU_ID	0x710110b9

/* Uncomment this line to force another I/O base address for SMB */
/* #define ALPM_SMBIO_BASE_ADDR	0x3a80 */

/* I/O registers offsets - the base address is programmed via the
 * SMBBA PCI configuration register
 */
#define SMBSTS		0x0	/* SMBus host/slave status register */
#define SMBCMD		0x1	/* SMBus host/slave command register */
#define SMBSTART	0x2	/* start to generate programmed cycle */
#define SMBHADDR	0x3	/* host address register */
#define SMBHDATA	0x4	/* data A register for host controller */
#define SMBHDATB	0x5	/* data B register for host controller */
#define SMBHBLOCK	0x6	/* block register for host controller */
#define SMBHCMD		0x7	/* command register for host controller */

/* SMBSTS masks */
#define TERMINATE	0x80
#define BUS_COLLI	0x40
#define DEVICE_ERR	0x20
#define SMI_I_STS	0x10
#define HST_BSY		0x08
#define IDL_STS		0x04
#define HSTSLV_STS	0x02
#define HSTSLV_BSY	0x01

/* SMBCMD masks */
#define SMB_BLK_CLR	0x80
#define T_OUT_CMD	0x08
#define ABORT_HOST	0x04

/* SMBus commands */
#define SMBQUICK	0x00
#define SMBSRBYTE	0x10		/* send/receive byte */
#define SMBWRBYTE	0x20		/* write/read byte */
#define SMBWRWORD	0x30		/* write/read word */
#define SMBWRBLOCK	0x40		/* write/read block */

/* PCI configuration registers and masks
 */
#define COM		0x4
#define COM_ENABLE_IO	0x1

#define SMBBA		0x14

#define ATPC		0x5b
#define ATPC_SMBCTRL	0x04

#define SMBHSI		0xe0
#define SMBHSI_SLAVE	0x2
#define SMBHSI_HOST	0x1

#define SMBHCBC		0xe2
#define SMBHCBC_CLOCK	0x70

#define SMBCLOCK_149K	0x0
#define SMBCLOCK_74K	0x20
#define SMBCLOCK_37K	0x40
#define SMBCLOCK_223K	0x80
#define SMBCLOCK_111K	0xa0
#define SMBCLOCK_55K	0xc0

struct alpm_data {
	int base;
        bus_space_tag_t smbst;
        bus_space_handle_t smbsh;
	pcici_t tag;
};
struct alpm_data alpmdata[NALPM];

struct alsmb_softc {
	int base;
	device_t smbus;
	struct alpm_data *alpm;
};

#define ALPM_SMBINB(alsmb,register) \
	(bus_space_read_1(alsmb->alpm->smbst, alsmb->alpm->smbsh, register))
#define ALPM_SMBOUTB(alsmb,register,value) \
	(bus_space_write_1(alsmb->alpm->smbst, alsmb->alpm->smbsh, register, value))

static int alsmb_probe(device_t);
static int alsmb_attach(device_t);
static int alsmb_smb_callback(device_t, int, caddr_t *);
static int alsmb_smb_quick(device_t dev, u_char slave, int how);
static int alsmb_smb_sendb(device_t dev, u_char slave, char byte);
static int alsmb_smb_recvb(device_t dev, u_char slave, char *byte);
static int alsmb_smb_writeb(device_t dev, u_char slave, char cmd, char byte);
static int alsmb_smb_readb(device_t dev, u_char slave, char cmd, char *byte);
static int alsmb_smb_writew(device_t dev, u_char slave, char cmd, short word);
static int alsmb_smb_readw(device_t dev, u_char slave, char cmd, short *word);
static int alsmb_smb_bwrite(device_t dev, u_char slave, char cmd, u_char count, char *buf);
static int alsmb_smb_bread(device_t dev, u_char slave, char cmd, u_char count, char *byte);

static devclass_t alsmb_devclass;

static device_method_t alsmb_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		alsmb_probe),
	DEVMETHOD(device_attach,	alsmb_attach),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	
	/* smbus interface */
	DEVMETHOD(smbus_callback,	alsmb_smb_callback),
	DEVMETHOD(smbus_quick,		alsmb_smb_quick),
	DEVMETHOD(smbus_sendb,		alsmb_smb_sendb),
	DEVMETHOD(smbus_recvb,		alsmb_smb_recvb),
	DEVMETHOD(smbus_writeb,		alsmb_smb_writeb),
	DEVMETHOD(smbus_readb,		alsmb_smb_readb),
	DEVMETHOD(smbus_writew,		alsmb_smb_writew),
	DEVMETHOD(smbus_readw,		alsmb_smb_readw),
	DEVMETHOD(smbus_bwrite,		alsmb_smb_bwrite),
	DEVMETHOD(smbus_bread,		alsmb_smb_bread),
	
	{ 0, 0 }
};

static driver_t alsmb_driver = {
	"alsmb",
	alsmb_methods,
	sizeof(struct alsmb_softc),
};

static const char* alpm_pci_probe(pcici_t tag, pcidi_t type);
static void alpm_pci_attach(pcici_t tag, int unit);

static u_long	alpm_count;

static struct	pci_device alpm_device = {
	"alpm",
	alpm_pci_probe,
	alpm_pci_attach,
	&alpm_count
};

COMPAT_PCI_DRIVER (alpm, alpm_device);

static const char*
alpm_pci_probe(pcici_t tag, pcidi_t type)
{
	if (type == ACER_M1543_PMU_ID)
		return ("AcerLabs M15x3 Power Management Unit");

	return ((char *)0);
}

static void
alpm_pci_attach(pcici_t tag, int unit)
{
	struct alpm_data *alpm;
	u_long l;

	if (unit >= NALPM) {
		printf("alpm%d: attach: only %d units configured.\n",
		        unit, NALPM);
		return;
	}
	alpm = &alpmdata[unit];

	alpm->tag = tag;

	/* Unlock SMBIO base register access */
	l = pci_cfgread(tag, ATPC, 1);
	pci_cfgwrite(tag, ATPC, l & ~ATPC_SMBCTRL, 1);

	if (bootverbose) {
		l = pci_cfgread(tag, SMBHSI, 1);
		printf("alsmb%d: %s/%s", unit,
			(l & SMBHSI_HOST) ? "host":"nohost",
			(l & SMBHSI_SLAVE) ? "slave":"noslave");

		l = pci_cfgread(tag, SMBHCBC, 1);
		switch (l & SMBHCBC_CLOCK) {
		case SMBCLOCK_149K:
			printf(" 149K");
			break;
		case SMBCLOCK_74K:
			printf(" 74K");
			break;
		case SMBCLOCK_37K:
			printf(" 37K");
			break;
		case SMBCLOCK_223K:
			printf(" 223K");
			break;
		case SMBCLOCK_111K:
			printf(" 111K");
			break;
		case SMBCLOCK_55K:
			printf(" 55K");
			break;
		}
	}

	alpm->smbst = I386_BUS_SPACE_IO;

#ifdef ALPM_SMBIO_BASE_ADDR
	/* disable I/O */
	l = pci_cfgread(tag, COM, 2);
	pci_cfgwrite(tag, COM, l & ~COM_ENABLE_IO, 2);

	/* set the I/O base address */
	pci_cfgwrite(tag, SMBBA, ALPM_SMBIO_BASE_ADDR | 0x1, 4);

	/* enable I/O */
	pci_cfgwrite(tag, COM, l | COM_ENABLE_IO, 2);

	alpm->smbsh = ALPM_SMBIO_BASE_ADDR;
#else
	alpm->smbsh = pci_cfgread(tag, SMBBA, 4) & ~0x1;
#endif
	if (bootverbose)
		printf(" at 0x%x\n", alpm->smbsh);

	/* XXX add the I2C interface to the root_bus until pcibus is ready */
	device_add_child(root_bus, "alsmb", unit);

	return;
}

/*
 * Not a real probe, we know the device exists since the device has
 * been added after the successfull pci probe.
 */
static int
alsmb_probe(device_t dev)
{
	struct alsmb_softc *sc = (struct alsmb_softc *)device_get_softc(dev);

	sc->alpm = &alpmdata[device_get_unit(dev)];

	device_set_desc(dev, "Aladdin IV/V/Pro2 SMBus controller");

	return (0);
}

static int
alsmb_attach(device_t dev)
{
	struct alsmb_softc *sc = (struct alsmb_softc *)device_get_softc(dev);

	/* allocate a new smbus device */
	sc->smbus = smbus_alloc_bus(dev);

	/* probe and attach the smbus */
	device_probe_and_attach(sc->smbus);

	return (0);
}

static int
alsmb_smb_callback(device_t dev, int index, caddr_t *data)
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
alsmb_clear(struct alsmb_softc *sc)
{
	ALPM_SMBOUTB(sc, SMBSTS, 0xff);
	DELAY(10);

	return (0);
}

#if 0
static int
alsmb_abort(struct alsmb_softc *sc)
{
	ALPM_SMBOUTB(sc, SMBCMD, T_OUT_CMD | ABORT_HOST);

	return (0);
}
#endif

static int
alsmb_idle(struct alsmb_softc *sc)
{
	u_char sts;

	sts = ALPM_SMBINB(sc, SMBSTS);

	ALPM_DEBUG(printf("alpm: idle? STS=0x%x\n", sts));

	return (sts & IDL_STS);
}

/*
 * Poll the SMBus controller
 */
static int
alsmb_wait(struct alsmb_softc *sc)
{
	int count = 10000;
	u_char sts;
	int error;

	/* wait for command to complete and SMBus controller is idle */
	while(count--) {
		DELAY(10);
		sts = ALPM_SMBINB(sc, SMBSTS);
		if (sts & SMI_I_STS)
			break;
	}

	ALPM_DEBUG(printf("alpm: STS=0x%x\n", sts));

	error = SMB_ENOERR;

	if (!count)
		error |= SMB_ETIMEOUT;

	if (sts & TERMINATE)
		error |= SMB_EABORT;

	if (sts & BUS_COLLI)
		error |= SMB_ENOACK;

	if (sts & DEVICE_ERR)
		error |= SMB_EBUSERR;

	if (error != SMB_ENOERR)
		alsmb_clear(sc);

	return (error);
}

static int
alsmb_smb_quick(device_t dev, u_char slave, int how)
{
	struct alsmb_softc *sc = (struct alsmb_softc *)device_get_softc(dev);
	int error;

	alsmb_clear(sc);
	if (!alsmb_idle(sc))
		return (EBUSY);

	switch (how) {
	case SMB_QWRITE:
		ALPM_DEBUG(printf("alpm: QWRITE to 0x%x", slave));
		ALPM_SMBOUTB(sc, SMBHADDR, slave & ~LSB);
		break;
	case SMB_QREAD:
		ALPM_DEBUG(printf("alpm: QREAD to 0x%x", slave));
		ALPM_SMBOUTB(sc, SMBHADDR, slave | LSB);
		break;
	default:
		panic("%s: unknown QUICK command (%x)!", __FUNCTION__,
			how);
	}
	ALPM_SMBOUTB(sc, SMBCMD, SMBQUICK);
	ALPM_SMBOUTB(sc, SMBSTART, 0xff);

	error = alsmb_wait(sc);

	ALPM_DEBUG(printf(", error=0x%x\n", error));

	return (error);
}

static int
alsmb_smb_sendb(device_t dev, u_char slave, char byte)
{
	struct alsmb_softc *sc = (struct alsmb_softc *)device_get_softc(dev);
	int error;

	alsmb_clear(sc);
	if (!alsmb_idle(sc))
		return (SMB_EBUSY);

	ALPM_SMBOUTB(sc, SMBHADDR, slave & ~LSB);
	ALPM_SMBOUTB(sc, SMBCMD, SMBSRBYTE);
	ALPM_SMBOUTB(sc, SMBHDATA, byte);
	ALPM_SMBOUTB(sc, SMBSTART, 0xff);

	error = alsmb_wait(sc);

	ALPM_DEBUG(printf("alpm: SENDB to 0x%x, byte=0x%x, error=0x%x\n", slave, byte, error));

	return (error);
}

static int
alsmb_smb_recvb(device_t dev, u_char slave, char *byte)
{
	struct alsmb_softc *sc = (struct alsmb_softc *)device_get_softc(dev);
	int error;

	alsmb_clear(sc);
	if (!alsmb_idle(sc))
		return (SMB_EBUSY);

	ALPM_SMBOUTB(sc, SMBHADDR, slave | LSB);
	ALPM_SMBOUTB(sc, SMBCMD, SMBSRBYTE);
	ALPM_SMBOUTB(sc, SMBSTART, 0xff);

	if ((error = alsmb_wait(sc)) == SMB_ENOERR)
		*byte = ALPM_SMBINB(sc, SMBHDATA);

	ALPM_DEBUG(printf("alpm: RECVB from 0x%x, byte=0x%x, error=0x%x\n", slave, *byte, error));

	return (error);
}

static int
alsmb_smb_writeb(device_t dev, u_char slave, char cmd, char byte)
{
	struct alsmb_softc *sc = (struct alsmb_softc *)device_get_softc(dev);
	int error;

	alsmb_clear(sc);
	if (!alsmb_idle(sc))
		return (SMB_EBUSY);

	ALPM_SMBOUTB(sc, SMBHADDR, slave & ~LSB);
	ALPM_SMBOUTB(sc, SMBCMD, SMBWRBYTE);
	ALPM_SMBOUTB(sc, SMBHDATA, byte);
	ALPM_SMBOUTB(sc, SMBHCMD, cmd);
	ALPM_SMBOUTB(sc, SMBSTART, 0xff);

	error = alsmb_wait(sc);

	ALPM_DEBUG(printf("alpm: WRITEB to 0x%x, cmd=0x%x, byte=0x%x, error=0x%x\n", slave, cmd, byte, error));

	return (error);
}

static int
alsmb_smb_readb(device_t dev, u_char slave, char cmd, char *byte)
{
	struct alsmb_softc *sc = (struct alsmb_softc *)device_get_softc(dev);
	int error;

	alsmb_clear(sc);
	if (!alsmb_idle(sc))
		return (SMB_EBUSY);

	ALPM_SMBOUTB(sc, SMBHADDR, slave | LSB);
	ALPM_SMBOUTB(sc, SMBCMD, SMBWRBYTE);
	ALPM_SMBOUTB(sc, SMBHCMD, cmd);
	ALPM_SMBOUTB(sc, SMBSTART, 0xff);

	if ((error = alsmb_wait(sc)) == SMB_ENOERR)
		*byte = ALPM_SMBINB(sc, SMBHDATA);

	ALPM_DEBUG(printf("alpm: READB from 0x%x, cmd=0x%x, byte=0x%x, error=0x%x\n", slave, cmd, *byte, error));

	return (error);
}

static int
alsmb_smb_writew(device_t dev, u_char slave, char cmd, short word)
{
	struct alsmb_softc *sc = (struct alsmb_softc *)device_get_softc(dev);
	int error;

	alsmb_clear(sc);
	if (!alsmb_idle(sc))
		return (SMB_EBUSY);

	ALPM_SMBOUTB(sc, SMBHADDR, slave & ~LSB);
	ALPM_SMBOUTB(sc, SMBCMD, SMBWRWORD);
	ALPM_SMBOUTB(sc, SMBHDATA, word & 0x00ff);
	ALPM_SMBOUTB(sc, SMBHDATB, (word & 0xff00) >> 8);
	ALPM_SMBOUTB(sc, SMBHCMD, cmd);
	ALPM_SMBOUTB(sc, SMBSTART, 0xff);

	error = alsmb_wait(sc);

	ALPM_DEBUG(printf("alpm: WRITEW to 0x%x, cmd=0x%x, word=0x%x, error=0x%x\n", slave, cmd, word, error));

	return (error);
}

static int
alsmb_smb_readw(device_t dev, u_char slave, char cmd, short *word)
{
	struct alsmb_softc *sc = (struct alsmb_softc *)device_get_softc(dev);
	int error;
	u_char high, low;

	alsmb_clear(sc);
	if (!alsmb_idle(sc))
		return (SMB_EBUSY);

	ALPM_SMBOUTB(sc, SMBHADDR, slave | LSB);
	ALPM_SMBOUTB(sc, SMBCMD, SMBWRWORD);
	ALPM_SMBOUTB(sc, SMBHCMD, cmd);
	ALPM_SMBOUTB(sc, SMBSTART, 0xff);

	if ((error = alsmb_wait(sc)) == SMB_ENOERR) {
		low = ALPM_SMBINB(sc, SMBHDATA);
		high = ALPM_SMBINB(sc, SMBHDATB);

		*word = ((high & 0xff) << 8) | (low & 0xff);
	}

	ALPM_DEBUG(printf("alpm: READW from 0x%x, cmd=0x%x, word=0x%x, error=0x%x\n", slave, cmd, *word, error));

	return (error);
}

static int
alsmb_smb_bwrite(device_t dev, u_char slave, char cmd, u_char count, char *buf)
{
	struct alsmb_softc *sc = (struct alsmb_softc *)device_get_softc(dev);
	u_char remain, len, i;
	int error = SMB_ENOERR;

	alsmb_clear(sc);
	if(!alsmb_idle(sc))
		return (SMB_EBUSY);

	remain = count;
	while (remain) {
		len = min(remain, 32);

		ALPM_SMBOUTB(sc, SMBHADDR, slave & ~LSB);
	
		/* set the cmd and reset the
		 * 32-byte long internal buffer */
		ALPM_SMBOUTB(sc, SMBCMD, SMBWRBLOCK | SMB_BLK_CLR);

		ALPM_SMBOUTB(sc, SMBHDATA, len);

		/* fill the 32-byte internal buffer */
		for (i=0; i<len; i++) {
			ALPM_SMBOUTB(sc, SMBHBLOCK, buf[count-remain+i]);
			DELAY(2);
		}
		ALPM_SMBOUTB(sc, SMBHCMD, cmd);
		ALPM_SMBOUTB(sc, SMBSTART, 0xff);

		if ((error = alsmb_wait(sc)) != SMB_ENOERR)
			goto error;

		remain -= len;
	}

error:
	ALPM_DEBUG(printf("alpm: WRITEBLK to 0x%x, count=0x%x, cmd=0x%x, error=0x%x", slave, count, cmd, error));

	return (error);
}

static int
alsmb_smb_bread(device_t dev, u_char slave, char cmd, u_char count, char *buf)
{
	struct alsmb_softc *sc = (struct alsmb_softc *)device_get_softc(dev);
	u_char remain, len, i;
	int error = SMB_ENOERR;

	alsmb_clear(sc);
	if (!alsmb_idle(sc))
		return (SMB_EBUSY);

	remain = count;
	while (remain) {
		ALPM_SMBOUTB(sc, SMBHADDR, slave | LSB);
	
		/* set the cmd and reset the
		 * 32-byte long internal buffer */
		ALPM_SMBOUTB(sc, SMBCMD, SMBWRBLOCK | SMB_BLK_CLR);

		ALPM_SMBOUTB(sc, SMBHCMD, cmd);
		ALPM_SMBOUTB(sc, SMBSTART, 0xff);

		if ((error = alsmb_wait(sc)) != SMB_ENOERR)
			goto error;

		len = ALPM_SMBINB(sc, SMBHDATA);

		/* read the 32-byte internal buffer */
		for (i=0; i<len; i++) {
			buf[count-remain+i] = ALPM_SMBINB(sc, SMBHBLOCK);
			DELAY(2);
		}

		remain -= len;
	}
error:
	ALPM_DEBUG(printf("alpm: READBLK to 0x%x, count=0x%x, cmd=0x%x, error=0x%x", slave, count, cmd, error));

	return (error);
}

DRIVER_MODULE(alsmb, root, alsmb_driver, alsmb_devclass, 0, 0);
