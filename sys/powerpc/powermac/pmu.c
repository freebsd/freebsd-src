/*-
 * Copyright (c) 2006 Michael Lorenz
 * Copyright 2008 by Nathan Whitehorn
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/pio.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <sys/rman.h>

#include <dev/adb/adb.h>

#include "pmuvar.h"
#include "viareg.h"

/*
 * MacIO interface
 */
static int	pmu_probe(device_t);
static int	pmu_attach(device_t);
static int	pmu_detach(device_t);

static u_int pmu_adb_send(device_t dev, u_char command_byte, int len, 
    u_char *data, u_char poll);
static u_int pmu_adb_autopoll(device_t dev, uint16_t mask);
static void pmu_poll(device_t dev);

static device_method_t  pmu_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pmu_probe),
	DEVMETHOD(device_attach,	pmu_attach),
        DEVMETHOD(device_detach,        pmu_detach),
        DEVMETHOD(device_shutdown,      bus_generic_shutdown),
        DEVMETHOD(device_suspend,       bus_generic_suspend),
        DEVMETHOD(device_resume,        bus_generic_resume),

	/* bus interface, for ADB root */
        DEVMETHOD(bus_print_child,      bus_generic_print_child),
        DEVMETHOD(bus_driver_added,     bus_generic_driver_added),

	/* ADB bus interface */
	DEVMETHOD(adb_hb_send_raw_packet,   pmu_adb_send),
	DEVMETHOD(adb_hb_controller_poll,   pmu_poll),
	DEVMETHOD(adb_hb_set_autopoll_mask, pmu_adb_autopoll),

	{ 0, 0 },
};

static driver_t pmu_driver = {
	"pmu",
	pmu_methods,
	sizeof(struct pmu_softc),
};

static devclass_t pmu_devclass;

DRIVER_MODULE(pmu, macio, pmu_driver, pmu_devclass, 0, 0);
DRIVER_MODULE(adb, pmu, adb_driver, adb_devclass, 0, 0);

static int	pmuextint_probe(device_t);
static int	pmuextint_attach(device_t);

static device_method_t  pmuextint_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pmuextint_probe),
	DEVMETHOD(device_attach,	pmuextint_attach),
	
	{0,0}
};

static driver_t pmuextint_driver = {
	"pmuextint",
	pmuextint_methods,
	0
};

static devclass_t pmuextint_devclass;

DRIVER_MODULE(pmuextint, macgpio, pmuextint_driver, pmuextint_devclass, 0, 0);

/* Make sure uhid is loaded, as it turns off some of the ADB emulation */
MODULE_DEPEND(pmu, usb, 1, 1, 1);

static void pmu_intr(void *arg);
static void pmu_in(struct pmu_softc *sc);
static void pmu_out(struct pmu_softc *sc);
static void pmu_ack_on(struct pmu_softc *sc);
static void pmu_ack_off(struct pmu_softc *sc);
static int pmu_send(void *cookie, int cmd, int length, uint8_t *in_msg,
	int rlen, uint8_t *out_msg);
static uint8_t pmu_read_reg(struct pmu_softc *sc, u_int offset);
static void pmu_write_reg(struct pmu_softc *sc, u_int offset, uint8_t value);
static int pmu_intr_state(struct pmu_softc *);

/* these values shows that number of data returned after 'send' cmd is sent */
static signed char pm_send_cmd_type[] = {
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	0x01, 0x01,   -1,   -1,   -1,   -1,   -1,   -1,
	0x00, 0x00,   -1,   -1,   -1,   -1,   -1, 0x00,
	  -1, 0x00, 0x02, 0x01, 0x01,   -1,   -1,   -1,
	0x00,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	0x04, 0x14,   -1, 0x03,   -1,   -1,   -1,   -1,
	0x00, 0x00, 0x02, 0x02,   -1,   -1,   -1,   -1,
	0x01, 0x01,   -1,   -1,   -1,   -1,   -1,   -1,
	0x00, 0x00,   -1,   -1, 0x01,   -1,   -1,   -1,
	0x01, 0x00, 0x02, 0x02,   -1, 0x01, 0x03, 0x01,
	0x00, 0x01, 0x00, 0x00, 0x00,   -1,   -1,   -1,
	0x02,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   -1,   -1,
	0x01, 0x01, 0x01,   -1,   -1,   -1,   -1,   -1,
	0x00, 0x00,   -1,   -1,   -1,   -1, 0x04, 0x04,
	0x04,   -1, 0x00,   -1,   -1,   -1,   -1,   -1,
	0x00,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	0x01, 0x02,   -1,   -1,   -1,   -1,   -1,   -1,
	0x00, 0x00,   -1,   -1,   -1,   -1,   -1,   -1,
	0x02, 0x02, 0x02, 0x04,   -1, 0x00,   -1,   -1,
	0x01, 0x01, 0x03, 0x02,   -1,   -1,   -1,   -1,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	0x00,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	0x01, 0x01,   -1,   -1, 0x00, 0x00,   -1,   -1,
	  -1, 0x04, 0x00,   -1,   -1,   -1,   -1,   -1,
	0x03,   -1, 0x00,   -1, 0x00,   -1,   -1, 0x00,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1
};

/* these values shows that number of data returned after 'receive' cmd is sent */
static signed char pm_receive_cmd_type[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x02,   -1,   -1,   -1,   -1,   -1, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x05, 0x15,   -1, 0x02,   -1,   -1,   -1,   -1,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x02,   -1,   -1,   -1,   -1,   -1,   -1,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x00, 0x03, 0x03,   -1,   -1,   -1,   -1,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x04, 0x04, 0x03, 0x09,   -1,   -1,   -1,   -1,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  -1,   -1,   -1,   -1,   -1,   -1, 0x01, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x06,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x02,   -1,   -1,   -1,   -1,   -1,   -1,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x00, 0x00, 0x00,   -1,   -1,   -1,   -1,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x02,   -1,   -1, 0x02,   -1,   -1,   -1,
	0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	  -1,   -1, 0x02,   -1,   -1,   -1,   -1, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
};

/* We only have one of each device, so globals are safe */
static device_t pmu = NULL;
static device_t pmu_extint = NULL;

static int
pmuextint_probe(device_t dev)
{
	const char *type = ofw_bus_get_type(dev);

	if (strcmp(type, "extint-gpio1") != 0)
                return (ENXIO);

	device_set_desc(dev, "Apple PMU99 External Interrupt");
	return (0);
}

static int
pmu_probe(device_t dev)
{
	const char *type = ofw_bus_get_type(dev);

	if (strcmp(type, "via-pmu") != 0)
                return (ENXIO);

	device_set_desc(dev, "Apple PMU99 Controller");
	return (0);
}


static int
setup_pmu_intr(device_t dev, device_t extint)
{
	struct pmu_softc *sc;
	sc = device_get_softc(dev);

	sc->sc_irqrid = 0;
	sc->sc_irq = bus_alloc_resource_any(extint, SYS_RES_IRQ, &sc->sc_irqrid,
           	RF_ACTIVE);
        if (sc->sc_irq == NULL) {
                device_printf(dev, "could not allocate interrupt\n");
                return (ENXIO);
        }

	if (bus_setup_intr(dev, sc->sc_irq, INTR_TYPE_MISC | INTR_MPSAFE 
	    | INTR_ENTROPY, NULL, pmu_intr, dev, &sc->sc_ih) != 0) {
                device_printf(dev, "could not setup interrupt\n");
                bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irqrid,
                    sc->sc_irq);
                return (ENXIO);
        }

	return (0);
}

static int
pmuextint_attach(device_t dev)
{
	pmu_extint = dev;
	if (pmu)
		return (setup_pmu_intr(pmu,dev));

	return (0);
}

static int
pmu_attach(device_t dev)
{
	struct pmu_softc *sc;

	uint8_t reg;
	uint8_t cmd[2] = {2, 0};
	uint8_t resp[16];
	phandle_t node,child;
	
	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	
	sc->sc_memrid = 0;
	sc->sc_memr = bus_alloc_resource_any(dev, SYS_RES_MEMORY, 
		          &sc->sc_memrid, RF_ACTIVE);

	mtx_init(&sc->sc_mutex,"pmu",NULL,MTX_DEF | MTX_RECURSE);

	if (sc->sc_memr == NULL) {
		device_printf(dev, "Could not alloc mem resource!\n");
		return (ENXIO);
	}

	/*
	 * Our interrupt is attached to a GPIO pin. Depending on probe order,
	 * we may not have found it yet. If we haven't, it will find us, and
	 * attach our interrupt then.
	 */
	pmu = dev;
	if (pmu_extint != NULL) {
		if (setup_pmu_intr(dev,pmu_extint) != 0)
			return (ENXIO);
	}

	sc->sc_error = 0;
	sc->sc_polling = 0;
	sc->sc_autopoll = 0;

	/* Init PMU */

	reg = PMU_INT_TICK | PMU_INT_ADB | PMU_INT_PCEJECT | PMU_INT_SNDBRT;
	reg |= PMU_INT_BATTERY;
	reg |= PMU_INT_ENVIRONMENT;
	pmu_send(sc, PMU_SET_IMASK, 1, &reg, 16, resp);

	pmu_write_reg(sc, vIER, 0x90); /* make sure VIA interrupts are on */

	pmu_send(sc, PMU_SYSTEM_READY, 1, cmd, 16, resp);
	pmu_send(sc, PMU_GET_VERSION, 1, cmd, 16, resp);

	/* Initialize child buses (ADB) */
	node = ofw_bus_get_node(dev);

	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		char name[32];

		memset(name, 0, sizeof(name));
		OF_getprop(child, "name", name, sizeof(name));

		if (bootverbose)
			device_printf(dev, "PMU child <%s>\n",name);

		if (strncmp(name, "adb", 4) == 0) {
			sc->adb_bus = device_add_child(dev,"adb",-1);
		}
	}

	return (bus_generic_attach(dev));
}

static int 
pmu_detach(device_t dev) 
{
	struct pmu_softc *sc;

	sc = device_get_softc(dev);

	bus_teardown_intr(dev, sc->sc_irq, sc->sc_ih);
	bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irqrid, sc->sc_irq);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_memrid, sc->sc_memr);
	mtx_destroy(&sc->sc_mutex);

	return (bus_generic_detach(dev));
}

static uint8_t
pmu_read_reg(struct pmu_softc *sc, u_int offset) 
{
	return (bus_read_1(sc->sc_memr, offset));
}

static void
pmu_write_reg(struct pmu_softc *sc, u_int offset, uint8_t value) 
{
	bus_write_1(sc->sc_memr, offset, value);
}

static int
pmu_send_byte(struct pmu_softc *sc, uint8_t data)
{

	pmu_out(sc);
	pmu_write_reg(sc, vSR, data);
	pmu_ack_off(sc);
	/* wait for intr to come up */
	/* XXX should add a timeout and bail if it expires */
	do {} while (pmu_intr_state(sc) == 0);
	pmu_ack_on(sc);
	do {} while (pmu_intr_state(sc));
	pmu_ack_on(sc);
	return 0;
}

static inline int
pmu_read_byte(struct pmu_softc *sc, uint8_t *data)
{
	volatile uint8_t scratch;
	pmu_in(sc);
	scratch = pmu_read_reg(sc, vSR);
	pmu_ack_off(sc);
	/* wait for intr to come up */
	do {} while (pmu_intr_state(sc) == 0);
	pmu_ack_on(sc);
	do {} while (pmu_intr_state(sc));
	*data = pmu_read_reg(sc, vSR);
	return 0;
}

static int
pmu_intr_state(struct pmu_softc *sc)
{
	return ((pmu_read_reg(sc, vBufB) & vPB3) == 0);
}

static int
pmu_send(void *cookie, int cmd, int length, uint8_t *in_msg, int rlen,
    uint8_t *out_msg)
{
	struct pmu_softc *sc = cookie;
	int i, rcv_len = -1;
	uint8_t out_len, intreg;

	intreg = pmu_read_reg(sc, vIER);
	intreg &= 0x10;
	pmu_write_reg(sc, vIER, intreg);

	/* wait idle */
	do {} while (pmu_intr_state(sc));
	sc->sc_error = 0;

	/* send command */
	pmu_send_byte(sc, cmd);

	/* send length if necessary */
	if (pm_send_cmd_type[cmd] < 0) {
		pmu_send_byte(sc, length);
	}

	for (i = 0; i < length; i++) {
		pmu_send_byte(sc, in_msg[i]);
	}

	/* see if there's data to read */
	rcv_len = pm_receive_cmd_type[cmd];
	if (rcv_len == 0) 
		goto done;

	/* read command */
	if (rcv_len == 1) {
		pmu_read_byte(sc, out_msg);
		goto done;
	} else
		out_msg[0] = cmd;
	if (rcv_len < 0) {
		pmu_read_byte(sc, &out_len);
		rcv_len = out_len + 1;
	}
	for (i = 1; i < min(rcv_len, rlen); i++)
		pmu_read_byte(sc, &out_msg[i]);

done:
	pmu_write_reg(sc, vIER, (intreg == 0) ? 0 : 0x90);

	return rcv_len;
}


static void
pmu_poll(device_t dev)
{
	pmu_intr(dev);
}

static void
pmu_in(struct pmu_softc *sc)
{
	uint8_t reg;

	reg = pmu_read_reg(sc, vACR);
	reg &= ~vSR_OUT;
	reg |= 0x0c;
	pmu_write_reg(sc, vACR, reg);
}

static void
pmu_out(struct pmu_softc *sc)
{
	uint8_t reg;

	reg = pmu_read_reg(sc, vACR);
	reg |= vSR_OUT;
	reg |= 0x0c;
	pmu_write_reg(sc, vACR, reg);
}

static void
pmu_ack_off(struct pmu_softc *sc)
{
	uint8_t reg;

	reg = pmu_read_reg(sc, vBufB);
	reg &= ~vPB4;
	pmu_write_reg(sc, vBufB, reg);
}

static void
pmu_ack_on(struct pmu_softc *sc)
{
	uint8_t reg;

	reg = pmu_read_reg(sc, vBufB);
	reg |= vPB4;
	pmu_write_reg(sc, vBufB, reg);
}

static void
pmu_intr(void *arg)
{
	device_t        dev;
	struct pmu_softc *sc;

	unsigned int len;
	uint8_t resp[16];
	uint8_t junk[16];

        dev = (device_t)arg;
	sc = device_get_softc(dev);

	mtx_lock(&sc->sc_mutex);

	pmu_write_reg(sc, vIFR, 0x90);	/* Clear 'em */
	len = pmu_send(sc, PMU_INT_ACK, 0, NULL, 16, resp);

	mtx_unlock(&sc->sc_mutex);

	if ((len < 1) || (resp[1] == 0)) {
		return;
	}

	if (resp[1] & PMU_INT_ADB) {
		/*
		 * the PMU will turn off autopolling after each command that
		 * it did not issue, so we assume any but TALK R0 is ours and
		 * re-enable autopoll here whenever we receive an ACK for a
		 * non TR0 command.
		 */
		mtx_lock(&sc->sc_mutex);

		if ((resp[2] & 0x0f) != (ADB_COMMAND_TALK << 2)) {
			if (sc->sc_autopoll) {
				uint8_t cmd[] = {0, PMU_SET_POLL_MASK, 
				    (sc->sc_autopoll >> 8) & 0xff, 
				    sc->sc_autopoll & 0xff};

				pmu_send(sc, PMU_ADB_CMD, 4, cmd, 16, junk);
			}
		}	

		mtx_unlock(&sc->sc_mutex);

		adb_receive_raw_packet(sc->adb_bus,resp[1],resp[2],
			len - 3,&resp[3]);
	}
}

static u_int
pmu_adb_send(device_t dev, u_char command_byte, int len, u_char *data, 
    u_char poll)
{
	struct pmu_softc *sc = device_get_softc(dev);
	int i,replen;
	uint8_t packet[16], resp[16];

	/* construct an ADB command packet and send it */

	packet[0] = command_byte;

	packet[1] = 0;
	packet[2] = len;
	for (i = 0; i < len; i++)
		packet[i + 3] = data[i];

	mtx_lock(&sc->sc_mutex);
	replen = pmu_send(sc, PMU_ADB_CMD, len + 3, packet, 16, resp);
	mtx_unlock(&sc->sc_mutex);

	if (poll)
		pmu_poll(dev);

	return 0;
}

static u_int 
pmu_adb_autopoll(device_t dev, uint16_t mask) 
{
	struct pmu_softc *sc = device_get_softc(dev);

	/* magical incantation to re-enable autopolling */
	uint8_t cmd[] = {0, PMU_SET_POLL_MASK, (mask >> 8) & 0xff, mask & 0xff};
	uint8_t resp[16];

	mtx_lock(&sc->sc_mutex);

	if (sc->sc_autopoll == mask) {
		mtx_unlock(&sc->sc_mutex);
		return 0;
	}

	sc->sc_autopoll = mask & 0xffff;

	if (mask)
		pmu_send(sc, PMU_ADB_CMD, 4, cmd, 16, resp);
	else
		pmu_send(sc, PMU_ADB_POLL_OFF, 0, NULL, 16, resp);

	mtx_unlock(&sc->sc_mutex);
	
	return 0;
}
