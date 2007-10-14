/*	$FreeBSD$	*/
/*	$OpenBSD: it.c,v 1.22 2007/03/22 16:55:31 deraadt Exp $	*/

/*-
 * Copyright (c) 2003 Julien Bordet <zejames@greyhats.org>
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <isa/isavar.h>
#include <sys/systm.h>

#include <sys/sensors.h>

#include <dev/it/itvar.h>

#if defined(ITDEBUG)
#define DPRINTF(x)		do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

/*
 * IT87-compatible chips can typically measure voltages up to 4.096 V.
 * To measure higher voltages the input is attenuated with (external)
 * resistors.  Negative voltages are measured using a reference
 * voltage.  So we have to convert the sensor values back to real
 * voltages by applying the appropriate resistor factor.
 */
#define RFACT_NONE	10000
#define RFACT(x, y)	(RFACT_NONE * ((x) + (y)) / (y))

int it_probe(struct device *);
int it_attach(struct device *);
int it_detach(struct device *);
u_int8_t it_readreg(struct it_softc *, int);
void it_writereg(struct it_softc *, int, int);
void it_setup_volt(struct it_softc *, int, int);
void it_setup_temp(struct it_softc *, int, int);
void it_setup_fan(struct it_softc *, int, int);

void it_generic_stemp(struct it_softc *, struct ksensor *);
void it_generic_svolt(struct it_softc *, struct ksensor *);
void it_generic_fanrpm(struct it_softc *, struct ksensor *);

void it_refresh_sensor_data(struct it_softc *);
void it_refresh(void *);

extern struct cfdriver it_cd;

static device_method_t it_methods[] = {
	/* Methods from the device interface */
	DEVMETHOD(device_probe,		it_probe),
	DEVMETHOD(device_attach,	it_attach),
	DEVMETHOD(device_detach,	it_detach),

	/* Terminate method list */
	{ 0, 0 }
};

static driver_t it_driver = {
	"it",
	it_methods,
	sizeof (struct it_softc)
};

static devclass_t it_devclass;

DRIVER_MODULE(it, isa, it_driver, it_devclass, NULL, NULL);


const int it_vrfact[] = {
	RFACT_NONE,
	RFACT_NONE,
	RFACT_NONE,
	RFACT(68, 100),
	RFACT(30, 10),
	RFACT(21, 10),
	RFACT(83, 20),
	RFACT(68, 100),
	RFACT_NONE
};

int
it_probe(struct device *dev)
{
	struct resource *iores;
	int iorid = 0;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t cr;

	iores = bus_alloc_resource(dev, SYS_RES_IOPORT, &iorid,
	    0ul, ~0ul, 8, RF_ACTIVE);
	if (iores == NULL) {
		DPRINTF(("%s: can't map i/o space\n", __func__));
		return 1;
	}
	iot = rman_get_bustag(iores);
	ioh = rman_get_bushandle(iores);

	/* Check Vendor ID */
	bus_space_write_1(iot, ioh, ITC_ADDR, ITD_CHIPID);
	cr = bus_space_read_1(iot, ioh, ITC_DATA);
	bus_release_resource(dev, SYS_RES_IOPORT, iorid, iores);
	DPRINTF(("it: vendor id 0x%x\n", cr));
	if (cr != IT_ID_IT87)
		return 1;

	return 0;
}

int
it_attach(struct device *dev)
{
	struct it_softc *sc = device_get_softc(dev);
	int i;
	u_int8_t cr;

	sc->sc_dev = dev;
	sc->sc_iores = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->sc_iorid,
	    0ul, ~0ul, 8, RF_ACTIVE);
	if (sc->sc_iores == NULL) {
		device_printf(dev, "can't map i/o space\n");
		return 1;
	}
	sc->sc_iot = rman_get_bustag(sc->sc_iores);
	sc->sc_ioh = rman_get_bushandle(sc->sc_iores);

	sc->numsensors = IT_NUM_SENSORS;

	it_setup_fan(sc, 0, 3);
	it_setup_volt(sc, 3, 9);
	it_setup_temp(sc, 12, 3);

	if (sensor_task_register(sc, it_refresh, 5)) {
		device_printf(sc->sc_dev, "unable to register update task\n");
		return 1;
	}

	/* Activate monitoring */
	cr = it_readreg(sc, ITD_CONFIG);
	cr |= 0x01 | 0x08;
	it_writereg(sc, ITD_CONFIG, cr);

	/* Initialize sensors */
	strlcpy(sc->sensordev.xname, device_get_nameunit(sc->sc_dev),
	    sizeof(sc->sensordev.xname));
	for (i = 0; i < sc->numsensors; ++i)
		sensor_attach(&sc->sensordev, &sc->sensors[i]);
	sensordev_install(&sc->sensordev);

	return 0;
}

int
it_detach(struct device *dev)
{
	struct it_softc *sc = device_get_softc(dev);
	int error;

	sensordev_deinstall(&sc->sensordev);
	sensor_task_unregister(sc);

	error = bus_release_resource(dev, SYS_RES_IOPORT, sc->sc_iorid,
	    sc->sc_iores);
	if (error)
		return error;

	return 0;
}

u_int8_t
it_readreg(struct it_softc *sc, int reg)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, ITC_ADDR, reg);
	return (bus_space_read_1(sc->sc_iot, sc->sc_ioh, ITC_DATA));
}

void
it_writereg(struct it_softc *sc, int reg, int val)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, ITC_ADDR, reg);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, ITC_DATA, val);
}

void
it_setup_volt(struct it_softc *sc, int start, int n)
{
	int i;

	for (i = 0; i < n; ++i) {
		sc->sensors[start + i].type = SENSOR_VOLTS_DC;
	}

	snprintf(sc->sensors[start + 0].desc, sizeof(sc->sensors[0].desc),
	    "VCORE_A");
	snprintf(sc->sensors[start + 1].desc, sizeof(sc->sensors[1].desc),
	    "VCORE_B");
	snprintf(sc->sensors[start + 2].desc, sizeof(sc->sensors[2].desc),
	    "+3.3V");
	snprintf(sc->sensors[start + 3].desc, sizeof(sc->sensors[3].desc),
	    "+5V");
	snprintf(sc->sensors[start + 4].desc, sizeof(sc->sensors[4].desc),
	    "+12V");
	snprintf(sc->sensors[start + 5].desc, sizeof(sc->sensors[5].desc),
	    "Unused");
	snprintf(sc->sensors[start + 6].desc, sizeof(sc->sensors[6].desc),
	    "-12V");
	snprintf(sc->sensors[start + 7].desc, sizeof(sc->sensors[7].desc),
	    "+5VSB");
	snprintf(sc->sensors[start + 8].desc, sizeof(sc->sensors[8].desc),
	    "VBAT");
}

void
it_setup_temp(struct it_softc *sc, int start, int n)
{
	int i;

	for (i = 0; i < n; ++i)
		sc->sensors[start + i].type = SENSOR_TEMP;
}

void
it_setup_fan(struct it_softc *sc, int start, int n)
{
	int i;

	for (i = 0; i < n; ++i)
		sc->sensors[start + i].type = SENSOR_FANRPM;
}

void
it_generic_stemp(struct it_softc *sc, struct ksensor *sensors)
{
	int i, sdata;

	for (i = 0; i < 3; i++) {
		sdata = it_readreg(sc, ITD_SENSORTEMPBASE + i);
		/* Convert temperature to Fahrenheit degres */
		sensors[i].value = sdata * 1000000 + 273150000;
	}
}

void
it_generic_svolt(struct it_softc *sc, struct ksensor *sensors)
{
	int i, sdata;

	for (i = 0; i < 9; i++) {
		sdata = it_readreg(sc, ITD_SENSORVOLTBASE + i);
		DPRINTF(("sdata[volt%d] 0x%x\n", i, sdata));
		/* voltage returned as (mV >> 4) */
		sensors[i].value = (sdata << 4);
		/* these two values are negative and formula is different */
		if (i == 5 || i == 6)
			sensors[i].value = ((sdata << 4) - IT_VREF);
		/* rfact is (factor * 10^4) */
		sensors[i].value *= it_vrfact[i];
		/* division by 10 gets us back to uVDC */
		sensors[i].value /= 10;
		if (i == 5 || i == 6)
			sensors[i].value += IT_VREF * 1000;
	}
}

void
it_generic_fanrpm(struct it_softc *sc, struct ksensor *sensors)
{
	int i, sdata, divisor, odivisor, ndivisor;

	odivisor = ndivisor = divisor = it_readreg(sc, ITD_FAN);
	for (i = 0; i < 3; i++, divisor >>= 3) {
		sensors[i].flags &= ~SENSOR_FINVALID;
		if ((sdata = it_readreg(sc, ITD_SENSORFANBASE + i)) == 0xff) {
			sensors[i].flags |= SENSOR_FINVALID;
			if (i == 2)
				ndivisor ^= 0x40;
			else {
				ndivisor &= ~(7 << (i * 3));
				ndivisor |= ((divisor + 1) & 7) << (i * 3);
			}
		} else if (sdata == 0) {
			sensors[i].value = 0;
		} else {
			if (i == 2)
				divisor = divisor & 1 ? 3 : 1;
			sensors[i].value = 1350000 / (sdata << (divisor & 7));
		}
	}
	if (ndivisor != odivisor)
		it_writereg(sc, ITD_FAN, ndivisor);
}

/*
 * pre:  last read occurred >= 1.5 seconds ago
 * post: sensors[] current data are the latest from the chip
 */
void
it_refresh_sensor_data(struct it_softc *sc)
{
	/* Refresh our stored data for every sensor */
	it_generic_stemp(sc, &sc->sensors[12]);
	it_generic_svolt(sc, &sc->sensors[3]);
	it_generic_fanrpm(sc, &sc->sensors[0]);
}

void
it_refresh(void *arg)
{
	struct it_softc *sc = (struct it_softc *)arg;

	it_refresh_sensor_data(sc);
}
