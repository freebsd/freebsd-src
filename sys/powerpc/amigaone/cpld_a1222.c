/*-
 * Copyright (c) 2020 Justin Hibbits
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/limits.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <sys/kdb.h>

#include "cpld.h"

/*
 * A driver for the AmigaOne A1222 "Tabor" Main CPLD.
 *
 * The main CPLD is the interface between the CPU and the GPIO CPLD.
 * Communication with the GPIO CPLD is over the main CPLD's mailbox interface,
 * along with the dual-port RAM on the CPLD.
 *
 * Only one process can open the CPLD character device at a time.  The driver
 * enforces this to simplify the communication protocol.
 */

/* Resource access addresses. */
#define	CPLD_MEM_ADDR_H		0x00
#define	CPLD_MEM_ADDR_L		0x01
#define	CPLD_MEM_DATA		0x80

#define	CPLD_MAX_DRAM_WORDS	0x800

/* CPLD Registers. */
#define	CPLD_REG_SIG1		0x00
#define	CPLD_REG_SIG2		0x01
#define	CPLD_REG_HWREV		0x02
#define	CPLD_REG_CPLDREV	0x03
#define	CPLD_REG_MBC2X		0x04
#define	CPLD_REG_MBX2C		0x05
#define	CPLD_REG_FAN1_TACHO_U	0x10
#define	CPLD_REG_FAN1_TACHO_L	0x11
#define	CPLD_REG_FAN2_TACHO_U	0x12
#define	CPLD_REG_FAN2_TACHO_L	0x13
#define	CPLD_REG_FAN3_TACHO_U	0x14
#define	CPLD_REG_FAN3_TACHO_L	0x15
#define	CPLD_REG_DATE_UU	0x20
#define	CPLD_REG_DATE_UL	0x21
#define	CPLD_REG_DATE_LU	0x22
#define	CPLD_REG_DATE_LL	0x23
#define	CPLD_REG_TIME_UU	0x24
#define	CPLD_REG_TIME_UL	0x25
#define	CPLD_REG_TIME_LU	0x26
#define	CPLD_REG_TIME_LL	0x27
#define	CPLD_REG_SCR1		0x5c
#define	CPLD_REG_SCR2		0x6a
#define	CPLD_REG_RAM		0x80

struct cpld_softc {
	device_t	 sc_dev;
	struct resource	*sc_mem;
	struct cdev	*sc_cdev;
	struct mtx	 sc_mutex;
	bool		 sc_isopen;
};

static d_open_t		cpld_open;
static d_close_t	cpld_close;
static d_ioctl_t	cpld_ioctl;

static struct cdevsw cpld_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	cpld_open,
	.d_close =	cpld_close,
	.d_ioctl =	cpld_ioctl,
	.d_name =	"nvram",
};

static device_probe_t	cpld_probe;
static device_attach_t	cpld_attach;
static int		cpld_fan_sysctl(SYSCTL_HANDLER_ARGS);

static device_method_t cpld_methods[] = {
	DEVMETHOD(device_probe,		cpld_probe),
	DEVMETHOD(device_attach,	cpld_attach),

	DEVMETHOD_END
};

static driver_t cpld_driver = {
	"cpld",
	cpld_methods,
	sizeof(struct cpld_softc)
};

DRIVER_MODULE(cpld, lbc, cpld_driver, 0, 0);

static void
cpld_write(struct cpld_softc *sc, int addr, int data)
{
	if (addr >= CPLD_REG_RAM) {
		bus_write_1(sc->sc_mem, CPLD_MEM_ADDR_H, addr);
		bus_write_1(sc->sc_mem, CPLD_MEM_ADDR_L, addr);
	} else
		bus_write_1(sc->sc_mem, CPLD_MEM_ADDR_H, addr);
	bus_write_1(sc->sc_mem, CPLD_MEM_DATA, data);
}

static int
cpld_read(struct cpld_softc *sc, int addr)
{
	if (addr >= CPLD_REG_RAM) {
		bus_write_1(sc->sc_mem, CPLD_MEM_ADDR_H, addr);
		bus_write_1(sc->sc_mem, CPLD_MEM_ADDR_L, addr);
	} else
		bus_write_1(sc->sc_mem, CPLD_MEM_ADDR_H, addr);

	return (bus_read_1(sc->sc_mem, CPLD_MEM_DATA));
}

/*
 * This is only to read a register that's split into two 8-bit registers.
 * Dual-port RAM is not accepted for this purpose.
 */
static int
cpld_read_pair(struct cpld_softc *sc, int addr)
{
	int tmp;

	KASSERT(addr <= 0xff, ("Invalid register-pair base address %x.", addr));
	bus_write_1(sc->sc_mem, CPLD_MEM_ADDR_H, addr);
	tmp = bus_read_1(sc->sc_mem, CPLD_MEM_DATA) << 8;

	bus_write_1(sc->sc_mem, CPLD_MEM_ADDR_H, addr + 1);
	tmp |= bus_read_1(sc->sc_mem, CPLD_MEM_DATA);

	return (tmp);
}

static int
cpld_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "aeon,tabor-cpld"))
		return (ENXIO);

	device_set_desc(dev, "AmigaOne Tabor CPLD");

	return (BUS_PROBE_GENERIC);
}

static int
cpld_attach(device_t dev)
{
	struct make_dev_args mda;
	struct cpld_softc *sc;
	int rid;
	int date, time, tmp;
	int err;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	rid = 0;
	sc->sc_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE|RF_SHAREABLE);
	if (sc->sc_mem == NULL) {
		device_printf(dev, "Unable to allocate memory resource.\n");
		return (ENXIO);
	}
	mtx_init(&sc->sc_mutex, "cpld", NULL, MTX_DEF);

	if (bootverbose) {
		date = (cpld_read_pair(sc, CPLD_REG_DATE_UU) << 16) |
		    cpld_read_pair(sc, CPLD_REG_DATE_LU);
		time = (cpld_read_pair(sc, CPLD_REG_TIME_UU) << 16) |
		    cpld_read_pair(sc, CPLD_REG_TIME_LU);

		device_printf(dev, "Build date: %04x-%02x-%02x\n",
		    (date >> 16) & 0xffff, (date >> 8) & 0xff, date & 0xff);
#if 0
		/* Build time is nonsense on tested system. */
		device_printf(dev, "Build time: %02x:%02x:%02x\n",
		    (time >> 16) & 0xff, (time >> 8) & 0xff, time & 0xff);
#endif
	}

	tmp = cpld_read(sc, CPLD_REG_HWREV);
	device_printf(dev, "Hardware revision: %d\n", tmp);

	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "cpu_fan", CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, sc,
	    CPLD_REG_FAN1_TACHO_U, cpld_fan_sysctl, "I",
	    "CPU Fan speed in RPM");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "case_1_fan", CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, sc,
	    CPLD_REG_FAN2_TACHO_U, cpld_fan_sysctl, "I",
	    "Case fan 1 speed in RPM");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "case_2_fan", CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, sc,
	    CPLD_REG_FAN3_TACHO_U, cpld_fan_sysctl, "I",
	    "Case fan 2 speed in RPM");

	make_dev_args_init(&mda);
	mda.mda_flags =  MAKEDEV_CHECKNAME;
	mda.mda_devsw = &cpld_cdevsw;
	mda.mda_uid = UID_ROOT;
	mda.mda_gid = GID_WHEEL;
	mda.mda_mode = 0660;
	err = make_dev_s(&mda, &sc->sc_cdev, "cpld");
	if (err != 0) {
		device_printf(dev, "Error creating character device: %d\n", err);
		device_printf(dev, "Only sysctl interfaces will be available.\n");
	}

	return (0);
}

static int
cpld_fan_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct cpld_softc *sc;
	int error, old, rpm;
	int fan_reg;

	sc = arg1;
	fan_reg = arg2;
	mtx_lock(&sc->sc_mutex);
	/* Read until we get some level of read stability. */
	rpm = cpld_read(sc, fan_reg);
	do {
		old = rpm;
		rpm = cpld_read_pair(sc, fan_reg);
	} while (abs(rpm - old) > 10);
	mtx_unlock(&sc->sc_mutex);

	/* Convert RPS->RPM. */
	rpm *= 60;
	error = sysctl_handle_int(oidp, &rpm, 0, req);

	return (error);
}

static int
cpld_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct cpld_softc *sc = dev->si_drv1;

	if (sc->sc_isopen)
		return (EBUSY);
	sc->sc_isopen = 1;
	return (0);
}

static int
cpld_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct cpld_softc *sc = dev->si_drv1;

	sc->sc_isopen = 0;
	return (0);
}

/*
 * Send a command over the CPLD to the other side.
 *
 * This will first copy the data into the dual-port RAM, then signal the other
 * side by writing to the mailbox.
 */
static int
cpld_send(device_t dev, struct cpld_cmd_data *d)
{
	struct cpld_softc *sc;
	uint16_t *word;
	int i;

	if (d->cmd > USHRT_MAX)
		return (EINVAL);

	sc = device_get_softc(dev);

	mtx_lock(&sc->sc_mutex);
	for (i = 0, word = d->words; i < d->len; i++, word++) {
		if (i == 0)
			cpld_write(sc, CPLD_REG_RAM + d->offset, *word);
		else
			bus_write_4(sc->sc_mem, CPLD_MEM_DATA, *word);
	}

	cpld_write(sc, CPLD_REG_MBC2X, d->cmd);
	mtx_unlock(&sc->sc_mutex);

	return (0);
}

static int
cpld_recv(device_t dev, struct cpld_cmd_data *d)
{
	struct cpld_softc *sc;
	uint16_t *word;
	int i;

	sc = device_get_softc(dev);

	mtx_lock(&sc->sc_mutex);
	d->cmd = cpld_read(sc, CPLD_REG_MBX2C);

	for (i = 0, word = d->words; i < d->len; i++, word++) {
		if (i == 0)
			*word = cpld_read(sc, CPLD_REG_RAM + d->offset);
		else
			*word = bus_read_4(sc->sc_mem, CPLD_MEM_DATA);
	}
	mtx_unlock(&sc->sc_mutex);

	return (0);
}

static int
cpld_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct cpld_softc *sc;
	struct cpld_cmd_data *d;
	void *xfer_data, *tmp;
	int err;

	sc = dev->si_drv1;

	err = 0;
	d = (struct cpld_cmd_data *)data;
	if (d->len + d->offset > CPLD_MAX_DRAM_WORDS) {
		return (EINVAL);
	}
	xfer_data = malloc(d->len * sizeof(uint16_t), M_TEMP, M_WAITOK);

	switch (cmd) {
	case IOCCPLDSEND:
		err = copyin(d->words, xfer_data, d->len * sizeof(uint16_t));
		d->words = xfer_data;
		if (err == 0)
			err = cpld_send(sc->sc_dev, d);
		break;
	case IOCCPLDRECV:
		tmp = d->words;
		d->words = xfer_data;
		err = cpld_recv(sc->sc_dev, d);
		d->words = tmp;
		if (err == 0)
			err = copyout(xfer_data, d->words,
			    d->len * sizeof(uint16_t));
		break;
	default:
		err = ENOTTY;
		break;
	}
	free(xfer_data, M_TEMP);

	return (err);
}
