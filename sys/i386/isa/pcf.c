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
 *
 *	$Id: pcf.c,v 1.9 1999/05/08 21:59:27 dfr Exp $
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>

#include <machine/clock.h>

#include <i386/isa/isa_device.h>

#include <dev/iicbus/iiconf.h>
#include "iicbus_if.h"

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
	u_char pcf_addr;		/* interface I2C address */

	int pcf_slave_mode;		/* receiver or transmitter */
	int pcf_started;		/* 1 if start condition sent */

	device_t iicbus;		/* the corresponding iicbus */
};

struct pcf_isa_softc {

	int pcf_unit;			/* unit of the isa device */
	int pcf_base;			/* isa port */
	int pcf_irq;			/* isa irq or null if polled */

	unsigned int pcf_flags;		/* boot flags */
};

#define MAXPCF 2

static struct pcf_isa_softc *pcfdata[MAXPCF];
static int npcf = 0;

static int	pcfprobe_isa(struct isa_device *);
static int	pcfattach_isa(struct isa_device *);

struct isa_driver pcfdriver = {
	pcfprobe_isa, pcfattach_isa, "pcf"
};

static int pcf_probe(device_t);
static int pcf_attach(device_t);
static int pcf_print_child(device_t, device_t);

static int pcf_repeated_start(device_t, u_char, int);
static int pcf_start(device_t, u_char, int);
static int pcf_stop(device_t);
static int pcf_write(device_t, char *, int, int *, int);
static int pcf_read(device_t, char *, int, int *, int, int);
static ointhand2_t pcfintr;
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
pcfprobe_isa(struct isa_device *dvp)
{
	device_t pcfdev;
	struct pcf_isa_softc *pcf;

	if (npcf >= MAXPCF)
		return (0);

	if ((pcf = (struct pcf_isa_softc *)malloc(sizeof(struct pcf_isa_softc),
			M_DEVBUF, M_NOWAIT)) == NULL)
		return (0);

	pcf->pcf_base = dvp->id_iobase;		/* XXX should be ivars */
	pcf->pcf_unit = dvp->id_unit;

	if (!(dvp->id_flags & IIC_POLLED))
		pcf->pcf_irq = (dvp->id_irq);

	pcfdata[npcf++] = pcf;

	/* XXX add the pcf device to the root_bus until isa bus exists */
	pcfdev = device_add_child(root_bus, "pcf", pcf->pcf_unit, NULL);

	if (!pcfdev)
		goto error;

	return (1);

error:
	free(pcf, M_DEVBUF);
	return (0);
}

static int
pcfattach_isa(struct isa_device *isdp)
{
	isdp->id_ointr = pcfintr;
	return (1);				/* ok */
}

static int
pcf_probe(device_t pcfdev)
{
	struct pcf_softc *pcf = (struct pcf_softc *)device_get_softc(pcfdev);
	int unit = device_get_unit(pcfdev);

	/* retrieve base address from isa initialization
	 *
	 * XXX should use ivars with isabus
	 */
	pcf->pcf_base = pcfdata[unit]->pcf_base;

	/* reset the chip */
	pcf_rst_card(pcfdev, IIC_FASTEST, PCF_DEFAULT_ADDR, NULL);

	/* XXX try do detect chipset */

	device_set_desc(pcfdev, "PCF8584 I2C bus controller");

	return (0);
}

static int
pcf_attach(device_t pcfdev)
{
	struct pcf_softc *pcf = (struct pcf_softc *)device_get_softc(pcfdev);

	pcf->iicbus = iicbus_alloc_bus(pcfdev);

	/* probe and attach the iicbus */
	device_probe_and_attach(pcf->iicbus);

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
	 * STOP is sent only once even if a iicbus_stop() is called after
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
pcfintr(unit)
{
	struct pcf_softc *pcf =
		(struct pcf_softc *)devclass_get_softc(pcf_devclass, unit);

	char data, status, addr;
	char error = 0;

	status = PCF_GET_S1(pcf);

	if (status & PIN) {
		printf("pcf%d: spurious interrupt, status=0x%x\n", unit,
			status & 0xff);

		goto error;
	}	

	if (status & LAB)
		printf("pcf%d: bus arbitration lost!\n", unit);

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
			panic("%s: unknown slave mode (%d)!", __FUNCTION__,
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

DRIVER_MODULE(pcf, root, pcf_driver, pcf_devclass, 0, 0);
