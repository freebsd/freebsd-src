/*-
 * Copyright (c) 1998, 1999 Takanori Watanabe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.    IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <machine/bus.h>

#include <sys/uio.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <dev/smbus/smbconf.h>

#include "smbus_if.h"

/*This should be removed if force_pci_map_int supported*/
#include <sys/interrupt.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <pci/intpmreg.h>

#include "opt_intpm.h"

static struct _pcsid
{
	u_int32_t type;
	char	*desc;
} pci_ids[] = {
	{ 0x71138086, "Intel 82371AB Power management controller" },
	{ 0x719b8086, "Intel 82443MX Power management controller" },
#if 0
	/* Not a good idea yet, this stops isab0 functioning */
	{ 0x02001166, "ServerWorks OSB4 PCI to ISA Bridge" },
#endif

	{ 0x00000000, NULL }
};

static int intsmb_probe(device_t);
static int intsmb_attach(device_t);
static int intsmb_intr(device_t dev);
static int intsmb_slvintr(device_t dev);
static void  intsmb_alrintr(device_t dev);
static int intsmb_callback(device_t dev, int index, void *data);
static int intsmb_quick(device_t dev, u_char slave, int how);
static int intsmb_sendb(device_t dev, u_char slave, char byte);
static int intsmb_recvb(device_t dev, u_char slave, char *byte);
static int intsmb_writeb(device_t dev, u_char slave, char cmd, char byte);
static int intsmb_writew(device_t dev, u_char slave, char cmd, short word);
static int intsmb_readb(device_t dev, u_char slave, char cmd, char *byte);
static int intsmb_readw(device_t dev, u_char slave, char cmd, short *word);
static int intsmb_pcall(device_t dev, u_char slave, char cmd, short sdata, short *rdata);
static int intsmb_bwrite(device_t dev, u_char slave, char cmd, u_char count, char *buf);
static int intsmb_bread(device_t dev, u_char slave, char cmd, u_char *count, char *buf);
static void intsmb_start(device_t dev, u_char cmd, int nointr);
static int intsmb_stop(device_t dev);
static int intsmb_stop_poll(device_t dev);
static int intsmb_free(device_t dev);
static int intpm_probe (device_t dev);
static int intpm_attach (device_t dev);
static void intpm_intr(void *arg);

static devclass_t intsmb_devclass;

static device_method_t intpm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		intsmb_probe),
	DEVMETHOD(device_attach,	intsmb_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	/* SMBus interface */
	DEVMETHOD(smbus_callback,	intsmb_callback),
	DEVMETHOD(smbus_quick,		intsmb_quick),
	DEVMETHOD(smbus_sendb,		intsmb_sendb),
	DEVMETHOD(smbus_recvb,		intsmb_recvb),
	DEVMETHOD(smbus_writeb,		intsmb_writeb),
	DEVMETHOD(smbus_writew,		intsmb_writew),
	DEVMETHOD(smbus_readb,		intsmb_readb),
	DEVMETHOD(smbus_readw,		intsmb_readw),
	DEVMETHOD(smbus_pcall,		intsmb_pcall),
	DEVMETHOD(smbus_bwrite,		intsmb_bwrite),
	DEVMETHOD(smbus_bread,		intsmb_bread),

	{ 0, 0 }
};

struct intpm_pci_softc {
	bus_space_tag_t		smbst;
	bus_space_handle_t	smbsh;
	bus_space_tag_t		pmst;
	bus_space_handle_t	pmsh;
	device_t		smbus;
};


struct intsmb_softc {
	struct intpm_pci_softc	*pci_sc;
	bus_space_tag_t		st;
	bus_space_handle_t	sh;
	device_t		smbus;
	int			isbusy;
};

static driver_t intpm_driver = {
	"intsmb",
	intpm_methods,
	sizeof(struct intsmb_softc),
};

static devclass_t intpm_devclass;

static device_method_t intpm_pci_methods[] = {
	DEVMETHOD(device_probe,		intpm_probe),
	DEVMETHOD(device_attach,	intpm_attach),

	{ 0, 0 }
};

static driver_t intpm_pci_driver = {
	"intpm",
	intpm_pci_methods,
	sizeof(struct intpm_pci_softc)
};

static int
intsmb_probe(device_t dev)
{
	struct intsmb_softc *sc = device_get_softc(dev);

	sc->smbus = device_add_child(dev, "smbus", -1);
	if (!sc->smbus)
		return (EINVAL);    /* XXX don't know what to return else */
	device_set_desc(dev, "Intel PIIX4 SMBUS Interface");

	return (BUS_PROBE_DEFAULT); /* XXX don't know what to return else */
}
static int
intsmb_attach(device_t dev)
{
	struct intsmb_softc *sc = device_get_softc(dev);

	sc->pci_sc = device_get_softc(device_get_parent(dev));
	sc->isbusy = 0;
	sc->sh = sc->pci_sc->smbsh;
	sc->st = sc->pci_sc->smbst;
	sc->pci_sc->smbus = dev;
	device_probe_and_attach(sc->smbus);
#ifdef ENABLE_ALART
	/*Enable Arart*/
	bus_space_write_1(sc->st, sc->sh, PIIX4_SMBSLVCNT,
	    PIIX4_SMBSLVCNT_ALTEN);
#endif
	return (0);
}

static int
intsmb_callback(device_t dev, int index, void *data)
{
	int error = 0;
	intrmask_t s;

	s = splnet();
	switch (index) {
	case SMB_REQUEST_BUS:
		break;
	case SMB_RELEASE_BUS:
		break;
	default:
		error = EINVAL;
	}
	splx(s);

	return (error);
}

/* Counterpart of smbtx_smb_free(). */
static int
intsmb_free(device_t dev)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	intrmask_t s;

	if ((bus_space_read_1(sc->st, sc->sh, PIIX4_SMBHSTSTS) &
	    PIIX4_SMBHSTSTAT_BUSY) ||
#ifdef ENABLE_ALART
	    (bus_space_read_1(sc->st, sc->sh, PIIX4_SMBSLVSTS) & 
	    PIIX4_SMBSLVSTS_BUSY) ||
#endif
	    sc->isbusy)
		return (EBUSY);
	s = splhigh();
	sc->isbusy = 1;
	/* Disable Interrupt in slave part. */
#ifndef ENABLE_ALART
	bus_space_write_1(sc->st, sc->sh, PIIX4_SMBSLVCNT, 0);
#endif
	/* Reset INTR Flag to prepare INTR. */
	bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTSTS,
	    (PIIX4_SMBHSTSTAT_INTR | PIIX4_SMBHSTSTAT_ERR |
	    PIIX4_SMBHSTSTAT_BUSC | PIIX4_SMBHSTSTAT_FAIL));
	splx(s);
	return (0);
}

static int
intsmb_intr(device_t dev)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	int status;

	status = bus_space_read_1(sc->st, sc->sh, PIIX4_SMBHSTSTS);
	if (status & PIIX4_SMBHSTSTAT_BUSY)
		return (1);

	if (status & (PIIX4_SMBHSTSTAT_INTR | PIIX4_SMBHSTSTAT_ERR |
	    PIIX4_SMBHSTSTAT_BUSC | PIIX4_SMBHSTSTAT_FAIL)) {
		int tmp;

		tmp = bus_space_read_1(sc->st, sc->sh, PIIX4_SMBHSTCNT);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTCNT,
		    tmp & ~PIIX4_SMBHSTCNT_INTREN);
		if (sc->isbusy) {
			sc->isbusy = 0;
			wakeup(sc);
		}
		return (0);
	}
	return (1); /* Not Completed */
}

static int
intsmb_slvintr(device_t dev)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	int status, retval;

	retval = 1;
	status = bus_space_read_1(sc->st, sc->sh, PIIX4_SMBSLVSTS);
	if (status & PIIX4_SMBSLVSTS_BUSY)
		return (retval);
	if (status & PIIX4_SMBSLVSTS_ALART) {
		intsmb_alrintr(dev);
		retval = 0;
	} else if (status & ~(PIIX4_SMBSLVSTS_ALART | PIIX4_SMBSLVSTS_SDW2
		| PIIX4_SMBSLVSTS_SDW1)) {
		retval = 0;
	}

	/* Reset Status Register */
	bus_space_write_1(sc->st, sc->sh, PIIX4_SMBSLVSTS,
	    PIIX4_SMBSLVSTS_ALART | PIIX4_SMBSLVSTS_SDW2 |
	    PIIX4_SMBSLVSTS_SDW1 | PIIX4_SMBSLVSTS_SLV);
	return (retval);
}

static void
intsmb_alrintr(device_t dev)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	int slvcnt;
#ifdef ENABLE_ALART
	int error;
#endif

	/* Stop generating INTR from ALART. */
	slvcnt = bus_space_read_1(sc->st, sc->sh, PIIX4_SMBSLVCNT);
#ifdef ENABLE_ALART
	bus_space_write_1(sc->st, sc->sh, PIIX4_SMBSLVCNT,
	    slvcnt & ~PIIX4_SMBSLVCNT_ALTEN);
#endif
	DELAY(5);

	/* Ask bus who asserted it and then ask it what's the matter. */
#ifdef ENABLE_ALART
	error = intsmb_free(dev);
	if (!error) {
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTADD,
		    SMBALTRESP | LSB);
		intsmb_start(dev, PIIX4_SMBHSTCNT_PROT_BYTE, 1);
		if (!(error = intsmb_stop_poll(dev))) {
			u_int8_t addr;

			addr = bus_space_read_1(sc->st, sc->sh,
			    PIIX4_SMBHSTDAT0);
			printf("ALART_RESPONSE: 0x%x\n", addr);
		}
	} else
		printf("ERROR\n");

	/* Re-enable INTR from ALART. */
	bus_space_write_1(sc->st, sc->sh, PIIX4_SMBSLVCNT,
	    slvcnt | PIIX4_SMBSLVCNT_ALTEN);
	DELAY(5);
#endif
}

static void
intsmb_start(device_t dev, unsigned char cmd, int nointr)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	unsigned char tmp;

	tmp = bus_space_read_1(sc->st, sc->sh, PIIX4_SMBHSTCNT);
	tmp &= 0xe0;
	tmp |= cmd;
	tmp |= PIIX4_SMBHSTCNT_START;

	/* While not in autoconfiguration enable interrupts. */
	if (!cold || !nointr)
		tmp |= PIIX4_SMBHSTCNT_INTREN;
	bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTCNT, tmp);
}

/*
 * Polling Code.
 *
 * Polling is not encouraged because it requires waiting for the
 * device if it is busy.
 * (29063505.pdf from Intel) But during boot, interrupt cannot be used, so use
 * polling code then.
 */
static int
intsmb_stop_poll(device_t dev)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	int error, i;
	int tmp;

	/*
	 *  In smbtx driver, Simply waiting.
	 *  This loops 100-200 times.
	 */
	for (i = 0; i < 0x7fff; i++)
		if (bus_space_read_1(sc->st, sc->sh, PIIX4_SMBHSTSTS) &
		    PIIX4_SMBHSTSTAT_BUSY)
			break;

	for (i = 0; i < 0x7fff; i++) {
		int status;

		status = bus_space_read_1(sc->st, sc->sh, PIIX4_SMBHSTSTS);
		if (!(status & PIIX4_SMBHSTSTAT_BUSY)) {
			sc->isbusy = 0;
			error = (status & PIIX4_SMBHSTSTAT_ERR) ? EIO :
			    (status & PIIX4_SMBHSTSTAT_BUSC) ? EBUSY : 
			    (status & PIIX4_SMBHSTSTAT_FAIL) ? EIO : 0;
			if (error == 0 && !(status & PIIX4_SMBHSTSTAT_INTR))
				printf("unknown cause why?");
			return (error);
		}
	}

	sc->isbusy = 0;
	tmp = bus_space_read_1(sc->st, sc->sh, PIIX4_SMBHSTCNT);
	bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTCNT, 
	    tmp & ~PIIX4_SMBHSTCNT_INTREN);
	return (EIO);
}

/*
 * Wait for completion and return result.
 */
static int
intsmb_stop(device_t dev)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	int error;
	intrmask_t s;

	if (cold) {
		/* So that it can use device during device probe on SMBus. */
		error = intsmb_stop_poll(dev);
		return (error);
	}

	if (!tsleep(sc, (PWAIT) | PCATCH, "SMBWAI", hz/8)) {
		int status;

		status = bus_space_read_1(sc->st, sc->sh, PIIX4_SMBHSTSTS);
		if (!(status & PIIX4_SMBHSTSTAT_BUSY)) {
			error = (status & PIIX4_SMBHSTSTAT_ERR) ? EIO :
			    (status & PIIX4_SMBHSTSTAT_BUSC) ? EBUSY : 
			    (status & PIIX4_SMBHSTSTAT_FAIL) ? EIO : 0;
			if (error == 0 && !(status & PIIX4_SMBHSTSTAT_INTR))
				printf("intsmb%d: unknown cause why?\n",
				    device_get_unit(dev));
#ifdef ENABLE_ALART
			bus_space_write_1(sc->st, sc->sh, PIIX4_SMBSLVCNT,
			    PIIX4_SMBSLVCNT_ALTEN);
#endif
			return (error);
		}
	}

	/* Timeout Procedure. */
	s = splhigh();
	sc->isbusy = 0;

	/* Re-enable supressed interrupt from slave part. */
	bus_space_write_1(sc->st, sc->sh, PIIX4_SMBSLVCNT,
	    PIIX4_SMBSLVCNT_ALTEN);
	splx(s);
	return (EIO);
}

static int
intsmb_quick(device_t dev, u_char slave, int how)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	int error = 0;
	u_char data;

	data = slave;

	/* Quick command is part of Address, I think. */
	switch(how) {
	case SMB_QWRITE:
		data &= ~LSB;
		break;
	case SMB_QREAD:
		data |= LSB;
		break;
	default:
		error = EINVAL;
	}
	if (!error) {
		error = intsmb_free(dev);
		if (!error) {
			bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTADD,
			    data);
			intsmb_start(dev, PIIX4_SMBHSTCNT_PROT_QUICK, 0);
			error = intsmb_stop(dev);
		}
	}

	return (error);
}

static int
intsmb_sendb(device_t dev, u_char slave, char byte)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	int error;

	error = intsmb_free(dev);
	if (!error) {
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTADD,
		    slave & ~LSB);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTCMD, byte);
		intsmb_start(dev, PIIX4_SMBHSTCNT_PROT_BYTE, 0);
		error = intsmb_stop(dev);
	}
	return (error);
}

static int
intsmb_recvb(device_t dev, u_char slave, char *byte)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	int error;

	error = intsmb_free(dev);
	if (!error) {
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTADD, slave | LSB);
		intsmb_start(dev, PIIX4_SMBHSTCNT_PROT_BYTE, 0);
		if (!(error = intsmb_stop(dev))) {
#ifdef RECV_IS_IN_CMD
			/*
			 * Linux SMBus stuff also troubles
			 * Because Intel's datasheet does not make clear.
			 */
			*byte = bus_space_read_1(sc->st, sc->sh,
			    PIIX4_SMBHSTCMD);
#else
			*byte = bus_space_read_1(sc->st, sc->sh, 
			    PIIX4_SMBHSTDAT0);
#endif
		}
	}
	return (error);
}

static int
intsmb_writeb(device_t dev, u_char slave, char cmd, char byte)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	int error;

	error = intsmb_free(dev);
	if (!error) {
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTADD,
		    slave & ~LSB);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTCMD, cmd);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTDAT0, byte);
		intsmb_start(dev, PIIX4_SMBHSTCNT_PROT_BDATA, 0);
		error = intsmb_stop(dev);
	}
	return (error);
}

static int
intsmb_writew(device_t dev, u_char slave, char cmd, short word)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	int error;

	error = intsmb_free(dev);
	if (!error) {
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTADD,
		    slave & ~LSB);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTCMD, cmd);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTDAT0, 
		    word & 0xff);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTDAT1, 
		    (word >> 8) & 0xff);
		intsmb_start(dev, PIIX4_SMBHSTCNT_PROT_WDATA, 0);
		error = intsmb_stop(dev);
	}
	return (error);
}

static int
intsmb_readb(device_t dev, u_char slave, char cmd, char *byte)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	int error;

	error = intsmb_free(dev);
	if (!error) {
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTADD, slave | LSB);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTCMD, cmd);
		intsmb_start(dev, PIIX4_SMBHSTCNT_PROT_BDATA, 0);
		if (!(error = intsmb_stop(dev)))
			*byte = bus_space_read_1(sc->st, sc->sh,
			    PIIX4_SMBHSTDAT0);
	}
	return (error);
}
static int
intsmb_readw(device_t dev, u_char slave, char cmd, short *word)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	int error;

	error = intsmb_free(dev);
	if (!error) {
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTADD, slave | LSB);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTCMD, cmd);
		intsmb_start(dev, PIIX4_SMBHSTCNT_PROT_WDATA, 0);
		if (!(error = intsmb_stop(dev))) {
			*word = bus_space_read_1(sc->st, sc->sh,
			    PIIX4_SMBHSTDAT0);
			*word |= bus_space_read_1(sc->st, sc->sh,
			    PIIX4_SMBHSTDAT1) << 8;
		}
	}
	return (error);
}

/*
 * Data sheet claims that it implements all function, but also claims
 * that it implements 7 function and not mention PCALL. So I don't know
 * whether it will work.
 */
static int
intsmb_pcall(device_t dev, u_char slave, char cmd, short sdata, short *rdata)
{
#ifdef PROCCALL_TEST
	struct intsmb_softc *sc = device_get_softc(dev);
	int error;

	error = intsmb_free(dev);
	if (!error) {
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTADD,
 		    slave & ~LSB);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTCMD, cmd);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTDAT0,
		    sdata & 0xff);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTDAT1,
		    (sdata & 0xff) >> 8);
		intsmb_start(dev, PIIX4_SMBHSTCNT_PROT_WDATA, 0);
	}
	if (!(error = intsmb_stop(dev))) {
		*rdata = bus_space_read_1(sc->st, sc->sh, PIIX4_SMBHSTDAT0);
		*rdata |= bus_space_read_1(sc->st, sc->sh, PIIX4_SMBHSTDAT1) <<
		    8;
	}
	return (error);
#else
	return (0);
#endif
}

static int
intsmb_bwrite(device_t dev, u_char slave, char cmd, u_char count, char *buf)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	int error, i;

	error = intsmb_free(dev);
	if (count > SMBBLOCKTRANS_MAX || count == 0)
		error = SMB_EINVAL;
	if (!error) {
		/* Reset internal array index. */
		bus_space_read_1(sc->st, sc->sh, PIIX4_SMBHSTCNT);

		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTADD,
		    slave & ~LSB);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTCMD, cmd);
		for (i = 0; i < count; i++)
			bus_space_write_1(sc->st, sc->sh, PIIX4_SMBBLKDAT,
			    buf[i]);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTDAT0, count);
		intsmb_start(dev, PIIX4_SMBHSTCNT_PROT_BLOCK, 0);
		error = intsmb_stop(dev);
	}
	return (error);
}

static int
intsmb_bread(device_t dev, u_char slave, char cmd, u_char *count, char *buf)
{
	struct intsmb_softc *sc = device_get_softc(dev);
	int error, i;
	u_char data, nread;

	error = intsmb_free(dev);
	if (*count > SMBBLOCKTRANS_MAX || *count == 0)
		error = SMB_EINVAL;
	if (!error) {
		/* Reset internal array index. */
		bus_space_read_1(sc->st, sc->sh, PIIX4_SMBHSTCNT);

		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTADD, slave | LSB);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTCMD, cmd);
		bus_space_write_1(sc->st, sc->sh, PIIX4_SMBHSTDAT0, *count);
		intsmb_start(dev, PIIX4_SMBHSTCNT_PROT_BLOCK, 0);
		error = intsmb_stop(dev);
		if (!error) {
			nread= bus_space_read_1(sc->st, sc->sh,
			    PIIX4_SMBHSTDAT0);
			if (nread != 0 && nread <= SMBBLOCKTRANS_MAX) {
				for (i = 0; i < nread; i++) {
					data = bus_space_read_1(sc->st, sc->sh, 
					    PIIX4_SMBBLKDAT);
					if (i < *count)
						buf[i] = data;
				}
				*count = nread;
			} else {
				error = EIO;
			}
		}
	}
	return (error);
}

DRIVER_MODULE(intsmb, intpm, intpm_driver, intsmb_devclass, 0, 0);

static int
intpm_attach(device_t dev)
{
	struct intpm_pci_softc *sc;
	struct resource *res;
	device_t smbinterface;
	void *ih;
	char *str;
	int error, rid, value;
	int unit = device_get_unit(dev);

	sc = device_get_softc(dev);
	if (sc == NULL)
		return (ENOMEM);

	rid = PCI_BASE_ADDR_SMB;
	res = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid, RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev, "Could not allocate Bus space\n");
		return (ENXIO);
	}
	sc->smbst = rman_get_bustag(res);
	sc->smbsh = rman_get_bushandle(res);

#ifdef __i386__
	device_printf(dev, "%s %lx\n", (sc->smbst == I386_BUS_SPACE_IO) ?
	    "I/O mapped" : "Memory", rman_get_start(res));
#endif

#ifndef NO_CHANGE_PCICONF
	pci_write_config(dev, PCIR_INTLINE, 0x9, 1);
	pci_write_config(dev, PCI_HST_CFG_SMB,
	    PCI_INTR_SMB_IRQ9 | PCI_INTR_SMB_ENABLE, 1);
#endif
	value = pci_read_config(dev, PCI_HST_CFG_SMB, 1);
	switch (value & 0xe) {
	case PCI_INTR_SMB_SMI:
		str = "SMI";
		break;
	case PCI_INTR_SMB_IRQ9:
		str = "IRQ 9";
		break;
	default:
		str = "BOGUS";
	}
	device_printf(dev, "intr %s %s ", str,
	    (value & 1) ? "enabled" : "disabled");
	value = pci_read_config(dev, PCI_REVID_SMB, 1);
	printf("revision %d\n", value);

	/* Install interrupt handler. */
	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 9, 9, 1,
	    RF_SHAREABLE | RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev, "could not allocate irq");
		return (ENOMEM);
	}
	error = bus_setup_intr(dev, res, INTR_TYPE_MISC, intpm_intr, sc, &ih);
	if (error) {
		device_printf(dev, "Failed to map intr\n");
		return (error);
	}
	smbinterface = device_add_child(dev, "intsmb", unit);
	if (!smbinterface)
		printf("intsmb%d: could not add SMBus device\n", unit);
	device_probe_and_attach(smbinterface);

	value = pci_read_config(dev, PCI_BASE_ADDR_PM, 4);
	printf("intpm%d: PM %s %x \n", unit,
	    (value & 1) ? "I/O mapped" : "Memory", value & 0xfffe);
	return (0);
}

static int
intpm_probe(device_t dev)
{
	struct _pcsid *ep = pci_ids;
	uint32_t device_id = pci_get_devid(dev);

	while (ep->type && ep->type != device_id)
		++ep;
	if (ep->desc != NULL) {
		device_set_desc(dev, ep->desc);
		bus_set_resource(dev, SYS_RES_IRQ, 0, 9, 1); /* XXX setup intr resource */
		return (BUS_PROBE_DEFAULT);
	} else {
		return (ENXIO);
	}
}

static void
intpm_intr(void *arg)
{
	struct intpm_pci_softc *sc = arg;

	intsmb_intr(sc->smbus);
	intsmb_slvintr(sc->smbus);
}

DRIVER_MODULE(intpm, pci , intpm_pci_driver, intpm_devclass, 0, 0);
DRIVER_MODULE(smbus, intsmb, smbus_driver, smbus_devclass, 0, 0);
MODULE_DEPEND(intpm, smbus, SMBUS_MINVER, SMBUS_PREFVER, SMBUS_MAXVER);
MODULE_VERSION(intpm, 1);
