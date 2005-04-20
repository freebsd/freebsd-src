/*-
 * Copyright (c) 1998 Nicolas Souchu, Marc Bouget
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <isa/isareg.h>
#include <isa/isavar.h>

#include <dev/iicbus/iiconf.h>
#include "iicbus_if.h"

#define IO_PCFSIZE	2

#define TIMEOUT	9999					/* XXX */

/* Status bits of S1 register (read only) */
#define nBB	0x01		/* busy when low set/reset by STOP/START*/
#define LAB	0x02		/* lost arbitration bit in multi-master mode */
#define AAS	0x04		/* addressed as slave */
#define LRB	0x08		/* last received byte when not AAS */
#define AD0	0x08		/* general call received when AAS */
#define BER	0x10		/* bus error, misplaced START or STOP */
#define STS	0x20		/* STOP detected in slave receiver mode */
#define PIN	0x80		/* pending interrupt not (r/w) */

/* Control bits of S1 register (write only) */
#define ACK	0x01
#define STO	0x02
#define STA	0x04
#define ENI	0x08
#define ES2	0x10
#define ES1	0x20
#define ES0	0x40

#define BUFSIZE 2048

#define SLAVE_TRANSMITTER	0x1
#define SLAVE_RECEIVER		0x2

#define PCF_DEFAULT_ADDR	0xaa

struct pcf_softc {

	int pcf_base;			/* isa port */
	int pcf_flags;
	u_char pcf_addr;		/* interface I2C address */

	int pcf_slave_mode;		/* receiver or transmitter */
	int pcf_started;		/* 1 if start condition sent */

	device_t iicbus;		/* the corresponding iicbus */

  	int rid_irq, rid_ioport;
	struct resource *res_irq, *res_ioport;
	void *intr_cookie;
};

static int pcf_probe(device_t);
static int pcf_attach(device_t);
static void pcfintr(void *arg);

static int pcf_print_child(device_t, device_t);

static int pcf_repeated_start(device_t, u_char, int);
static int pcf_start(device_t, u_char, int);
static int pcf_stop(device_t);
static int pcf_write(device_t, char *, int, int *, int);
static int pcf_read(device_t, char *, int, int *, int, int);
static int pcf_rst_card(device_t, u_char, u_char, u_char *);

static device_method_t pcf_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		pcf_probe),
	DEVMETHOD(device_attach,	pcf_attach),

	/* bus interface */
	DEVMETHOD(bus_print_child,	pcf_print_child),

	/* iicbus interface */
	DEVMETHOD(iicbus_callback,	iicbus_null_callback),
	DEVMETHOD(iicbus_repeated_start, pcf_repeated_start),
	DEVMETHOD(iicbus_start,		pcf_start),
	DEVMETHOD(iicbus_stop,		pcf_stop),
	DEVMETHOD(iicbus_write,		pcf_write),
	DEVMETHOD(iicbus_read,		pcf_read),
	DEVMETHOD(iicbus_reset,		pcf_rst_card),

	{ 0, 0 }
};

static driver_t pcf_driver = {
	"pcf",
	pcf_methods,
	sizeof(struct pcf_softc),
};

static devclass_t pcf_devclass;

#define DEVTOSOFTC(dev) ((struct pcf_softc *)device_get_softc(dev))

static int
pcf_probe(device_t pcfdev)
{
	struct pcf_softc *pcf = DEVTOSOFTC(pcfdev);
	device_t parent = device_get_parent(pcfdev);
	uintptr_t base;

	device_set_desc(pcfdev, "PCF8584 I2C bus controller");

	pcf = DEVTOSOFTC(pcfdev);
	bzero(pcf, sizeof(struct pcf_softc));

	pcf->rid_irq = pcf->rid_ioport = 0;
	pcf->res_irq = pcf->res_ioport = 0;

	/* IO port is mandatory */
	pcf->res_ioport = bus_alloc_resource(pcfdev, SYS_RES_IOPORT,
					     &pcf->rid_ioport, 0ul, ~0ul, 
					     IO_PCFSIZE, RF_ACTIVE);
	if (pcf->res_ioport == 0) {
		device_printf(pcfdev, "cannot reserve I/O port range\n");
		goto error;
	}
	BUS_READ_IVAR(parent, pcfdev, ISA_IVAR_PORT, &base);
	pcf->pcf_base = base;

	pcf->pcf_flags = device_get_flags(pcfdev);

	if (!(pcf->pcf_flags & IIC_POLLED)) {
		pcf->res_irq = bus_alloc_resource(pcfdev, SYS_RES_IRQ, &pcf->rid_irq,
						  0ul, ~0ul, 1, RF_ACTIVE);
		if (pcf->res_irq == 0) {
			device_printf(pcfdev, "can't reserve irq, polled mode.\n");
			pcf->pcf_flags |= IIC_POLLED;
		}
	}

	/* reset the chip */
	pcf_rst_card(pcfdev, IIC_FASTEST, PCF_DEFAULT_ADDR, NULL);

	return (0);
error:
	if (pcf->res_ioport != 0) {
		bus_deactivate_resource(pcfdev, SYS_RES_IOPORT, pcf->rid_ioport,
					pcf->res_ioport);
		bus_release_resource(pcfdev, SYS_RES_IOPORT, pcf->rid_ioport,
				     pcf->res_ioport);
	}
	return (ENXIO);
}

static int
pcf_attach(device_t pcfdev)
{
	struct pcf_softc *pcf = DEVTOSOFTC(pcfdev);
	device_t parent = device_get_parent(pcfdev);
	int error = 0;

	if (pcf->res_irq) {
		/* default to the tty mask for registration */	/* XXX */
		error = BUS_SETUP_INTR(parent, pcfdev, pcf->res_irq, INTR_TYPE_NET,
					    pcfintr, pcfdev, &pcf->intr_cookie);
		if (error)
			return (error);
	}

	pcf->iicbus = device_add_child(pcfdev, "iicbus", -1);

	/* probe and attach the iicbus */
	bus_generic_attach(pcfdev);

	return (0);
}

static int
pcf_print_child(device_t bus, device_t dev)
{
	struct pcf_softc *pcf = (struct pcf_softc *)device_get_softc(bus);
	int retval = 0;

	retval += bus_print_child_header(bus, dev);
	retval += printf(" on %s addr 0x%x\n", device_get_nameunit(bus),
			 (int)pcf->pcf_addr);

	return (retval);
}

/*
 * PCF8584 datasheet : when operate at 8 MHz or more, a minimun time of
 * 6 clocks cycles must be left between two consecutives access
 */
#define pcf_nops()	DELAY(10)

#define dummy_read(pcf)		PCF_GET_S0(pcf)
#define dummy_write(pcf)	PCF_SET_S0(pcf, 0)

/*
 * Specific register access to PCF8584
 */
static void PCF_SET_S0(struct pcf_softc *pcf, int data)
{
	outb(pcf->pcf_base, data);
	pcf_nops();
}

static void PCF_SET_S1(struct pcf_softc *pcf, int data)
{
	outb(pcf->pcf_base+1, data);
	pcf_nops();
}

static char PCF_GET_S0(struct pcf_softc *pcf)
{
	char data;

	data = inb(pcf->pcf_base);
	pcf_nops();

	return (data);
}

static char PCF_GET_S1(struct pcf_softc *pcf)
{
	char data;

	data = inb(pcf->pcf_base+1);
	pcf_nops();

	return (data);
}

/*
 * Polling mode for master operations wait for a new
 * byte incomming or outgoing
 */
static int pcf_wait_byte(struct pcf_softc *pcf)
{
	int counter = TIMEOUT;

	while (counter--) {

		if ((PCF_GET_S1(pcf) & PIN) == 0)
			return (0);
	}

	return (IIC_ETIMEOUT);
}

static int pcf_stop(device_t pcfdev)
{
	struct pcf_softc *pcf = DEVTOSOFTC(pcfdev);

	/*
	 * Send STOP condition iff the START condition was previously sent.
	 * STOP is sent only once even if an iicbus_stop() is called after
	 * an iicbus_read()... see pcf_read(): the pcf needs to send the stop
	 * before the last char is read.
	 */
	if (pcf->pcf_started) {
		/* set stop condition and enable IT */
		PCF_SET_S1(pcf, PIN|ES0|ENI|STO|ACK);

		pcf->pcf_started = 0;
	}

	return (0);
}


static int pcf_noack(struct pcf_softc *pcf, int timeout)
{
	int noack;
	int k = timeout/10;

	do {
		noack = PCF_GET_S1(pcf) & LRB;
		if (!noack)
			break;
		DELAY(10);				/* XXX wait 10 us */
	} while (k--);

	return (noack);
}

static int pcf_repeated_start(device_t pcfdev, u_char slave, int timeout)
{
	struct pcf_softc *pcf = DEVTOSOFTC(pcfdev);
	int error = 0;

	/* repeated start */
	PCF_SET_S1(pcf, ES0|STA|STO|ACK);

	/* set slave address to PCF. Last bit (LSB) must be set correctly
	 * according to transfer direction */
	PCF_SET_S0(pcf, slave);

	/* wait for address sent, polling */
	if ((error = pcf_wait_byte(pcf)))
		goto error;

	/* check for ack */
	if (pcf_noack(pcf, timeout)) {
		error = IIC_ENOACK;
		goto error;
	}

	return (0);

error:
	pcf_stop(pcfdev);
	return (error);
}

static int pcf_start(device_t pcfdev, u_char slave, int timeout)
{
	struct pcf_softc *pcf = DEVTOSOFTC(pcfdev);
	int error = 0;

	if ((PCF_GET_S1(pcf) & nBB) == 0)
		return (IIC_EBUSBSY);

	/* set slave address to PCF. Last bit (LSB) must be set correctly
	 * according to transfer direction */
	PCF_SET_S0(pcf, slave);

	/* START only */
	PCF_SET_S1(pcf, PIN|ES0|STA|ACK);

	pcf->pcf_started = 1;

	/* wait for address sent, polling */
	if ((error = pcf_wait_byte(pcf)))
		goto error;

	/* check for ACK */
	if (pcf_noack(pcf, timeout)) {
		error = IIC_ENOACK;
		goto error;
	}

	return (0);

error:
	pcf_stop(pcfdev);
	return (error);
}

static void
pcfintr(void *arg)
{
	device_t pcfdev = (device_t)arg;
	struct pcf_softc *pcf = DEVTOSOFTC(pcfdev);

	char data, status, addr;
	char error = 0;

	status = PCF_GET_S1(pcf);

	if (status & PIN) {
		device_printf(pcfdev, "spurious interrupt, status=0x%x\n", status & 0xff);

		goto error;
	}	

	if (status & LAB)
		device_printf(pcfdev, "bus arbitration lost!\n");

	if (status & BER) {
		error = IIC_EBUSERR;
		iicbus_intr(pcf->iicbus, INTR_ERROR, &error);

		goto error;
	}

	do {
		status = PCF_GET_S1(pcf);

		switch(pcf->pcf_slave_mode) {

		case SLAVE_TRANSMITTER:
			if (status & LRB) {
				/* ack interrupt line */
				dummy_write(pcf);

				/* no ack, don't send anymore */
				pcf->pcf_slave_mode = SLAVE_RECEIVER;

				iicbus_intr(pcf->iicbus, INTR_NOACK, NULL);
				break;
			}

			/* get data from upper code */
			iicbus_intr(pcf->iicbus, INTR_TRANSMIT, &data);

			PCF_SET_S0(pcf, data);	
			break;	
		
		case SLAVE_RECEIVER:
			if (status & AAS) {
				addr = PCF_GET_S0(pcf);

				if (status & AD0)
					iicbus_intr(pcf->iicbus, INTR_GENERAL, &addr);
				else
					iicbus_intr(pcf->iicbus, INTR_START, &addr);

				if (addr & LSB) {
					pcf->pcf_slave_mode = SLAVE_TRANSMITTER;

					/* get the first char from upper code */
					iicbus_intr(pcf->iicbus, INTR_TRANSMIT, &data);

					/* send first data byte */
					PCF_SET_S0(pcf, data);
				}

				break;
			}

			/* stop condition received? */
			if (status & STS) {
				/* ack interrupt line */
				dummy_read(pcf);

				/* emulate intr stop condition */
				iicbus_intr(pcf->iicbus, INTR_STOP, NULL);

			} else {
				/* get data, ack interrupt line */
				data = PCF_GET_S0(pcf);

				/* deliver the character */
				iicbus_intr(pcf->iicbus, INTR_RECEIVE, &data);
			}
			break;

		    default:
			panic("%s: unknown slave mode (%d)!", __func__,
				pcf->pcf_slave_mode);
		    }

	} while ((PCF_GET_S1(pcf) & PIN) == 0);

	return;

error:
	/* unknown event on bus...reset PCF */
	PCF_SET_S1(pcf, PIN|ES0|ENI|ACK);

	pcf->pcf_slave_mode = SLAVE_RECEIVER;

	return;
}

static int pcf_rst_card(device_t pcfdev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct pcf_softc *pcf = DEVTOSOFTC(pcfdev);

	if (oldaddr)
		*oldaddr = pcf->pcf_addr;

	/* retrieve own address from bus level */
	if (!addr)
		pcf->pcf_addr = PCF_DEFAULT_ADDR;
	else
		pcf->pcf_addr = addr;
	
	PCF_SET_S1(pcf, PIN);				/* initialize S1 */

	/* own address S'O<>0 */
	PCF_SET_S0(pcf, pcf->pcf_addr >> 1);

	/* select clock register */
	PCF_SET_S1(pcf, PIN|ES1);

	/* select bus speed : 18=90kb, 19=45kb, 1A=11kb, 1B=1.5kb */
	switch (speed) {
	case IIC_SLOW:
		PCF_SET_S0(pcf,  0x1b);
		break;

	case IIC_FAST:
		PCF_SET_S0(pcf,  0x19);
		break;

	case IIC_UNKNOWN:
	case IIC_FASTEST:
	default:
		PCF_SET_S0(pcf,  0x18);
		break;
	}

	/* set bus on, ack=yes, INT=yes */
	PCF_SET_S1(pcf, PIN|ES0|ENI|ACK);

	pcf->pcf_slave_mode = SLAVE_RECEIVER;

	return (0);
}

static int
pcf_write(device_t pcfdev, char *buf, int len, int *sent, int timeout /* us */)
{
	struct pcf_softc *pcf = DEVTOSOFTC(pcfdev);
	int bytes, error = 0;

#ifdef PCFDEBUG
	printf("pcf%d: >> writing %d bytes\n", device_get_unit(pcfdev), len);
#endif

	bytes = 0;
	while (len) {

		PCF_SET_S0(pcf, *buf++);

		/* wait for the byte to be send */
		if ((error = pcf_wait_byte(pcf)))
			goto error;

		/* check if ack received */
		if (pcf_noack(pcf, timeout)) {
			error = IIC_ENOACK;
			goto error;
		}

		len --;
		bytes ++;
	}

error:
	*sent = bytes;

#ifdef PCFDEBUG
	printf("pcf%d: >> %d bytes written (%d)\n",
		device_get_unit(pcfdev), bytes, error);
#endif

	return (error);
}

static int
pcf_read(device_t pcfdev, char *buf, int len, int *read, int last,
							int delay /* us */)
{
	struct pcf_softc *pcf = DEVTOSOFTC(pcfdev);
	int bytes, error = 0;

#ifdef PCFDEBUG
	printf("pcf%d: << reading %d bytes\n", device_get_unit(pcfdev), len);
#endif

	/* trig the bus to get the first data byte in S0 */
	if (len) {
		if (len == 1 && last)
			/* just one byte to read */
			PCF_SET_S1(pcf, ES0);		/* no ack */

		dummy_read(pcf);
	}

	bytes = 0;
	while (len) {

		/* XXX delay needed here */

		/* wait for trigged byte */
		if ((error = pcf_wait_byte(pcf))) {
			pcf_stop(pcfdev);
			goto error;
		}

		if (len == 1 && last)
			/* ok, last data byte already in S0, no I2C activity
			 * on next PCF_GET_S0() */
			pcf_stop(pcfdev);

		else if (len == 2 && last)
			/* next trigged byte with no ack */
			PCF_SET_S1(pcf, ES0);

		/* receive byte, trig next byte */
		*buf++ = PCF_GET_S0(pcf);

		len --;
		bytes ++;
	};

error:
	*read = bytes;

#ifdef PCFDEBUG
	printf("pcf%d: << %d bytes read (%d)\n",
		device_get_unit(pcfdev), bytes, error);
#endif

	return (error);
}

DRIVER_MODULE(pcf, isa, pcf_driver, pcf_devclass, 0, 0);
