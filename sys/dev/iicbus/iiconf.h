/*-
 * Copyright (c) 1998 Nicolas Souchu
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
#ifndef __IICONF_H
#define __IICONF_H

#include <sys/queue.h>

#define IICPRI (PZERO+8)		/* XXX sleep/wakeup queue priority */

#define n(flags) (~(flags) & (flags))

#define LSB 0x1

/*
 * How tsleep() is called in iic_request_bus().
 */
#define IIC_DONTWAIT	0
#define IIC_NOINTR	0
#define IIC_WAIT	0x1
#define IIC_INTR	0x2

/*
 * i2c modes
 */
#define IIC_MASTER	0x1
#define IIC_SLAVE	0x2
#define IIC_POLLED	0x4

/*
 * i2c speed
 */
#define IIC_UNKNOWN	0x0
#define IIC_SLOW	0x1
#define IIC_FAST	0x2
#define IIC_FASTEST	0x3

#define IIC_LAST_READ	0x1

/*
 * callback index
 */
#define IIC_REQUEST_BUS	0x1
#define IIC_RELEASE_BUS	0x2

/*
 * interrupt events
 */
#define INTR_GENERAL	0x1	/* general call received */
#define INTR_START	0x2	/* the I2C interface is addressed */
#define INTR_STOP	0x3	/* stop condition received */
#define INTR_RECEIVE	0x4	/* character received */
#define INTR_TRANSMIT	0x5	/* character to transmit */
#define INTR_ERROR	0x6	/* error */
#define INTR_NOACK	0x7	/* no ack from master receiver */

/*
 * adapter layer errors
 */
#define IIC_NOERR	0x0	/* no error occured */
#define IIC_EBUSERR	0x1	/* bus error */
#define IIC_ENOACK	0x2	/* ack not received until timeout */
#define IIC_ETIMEOUT	0x3	/* timeout */
#define IIC_EBUSBSY	0x4	/* bus busy */
#define IIC_ESTATUS	0x5	/* status error */
#define IIC_EUNDERFLOW	0x6	/* slave ready for more data */
#define IIC_EOVERFLOW	0x7	/* too much data */
#define IIC_ENOTSUPP	0x8	/* request not supported */
#define IIC_ENOADDR	0x9	/* no address assigned to the interface */

/*
 * ivars codes
 */
#define IICBUS_IVAR_ADDR	0x1	/* I2C address of the device */

extern int iicbus_request_bus(device_t, device_t, int);
extern int iicbus_release_bus(device_t, device_t);
extern device_t iicbus_alloc_bus(device_t);

extern void iicbus_intr(device_t, int, char *);

extern int iicbus_null_repeated_start(device_t, u_char);
extern int iicbus_null_callback(device_t, int, caddr_t);

#define iicbus_reset(bus,speed,addr,oldaddr) \
	(IICBUS_RESET(device_get_parent(bus), speed, addr, oldaddr))

/* basic I2C operations */
extern int iicbus_started(device_t);
extern int iicbus_start(device_t, u_char, int);
extern int iicbus_stop(device_t);
extern int iicbus_repeated_start(device_t, u_char, int);
extern int iicbus_write(device_t, char *, int, int *, int);
extern int iicbus_read(device_t, char *, int, int *, int, int);

/* single byte read/write functions, start/stop not managed */
extern int iicbus_write_byte(device_t, char, int);
extern int iicbus_read_byte(device_t, char *, int);

/* Read/write operations with start/stop conditions managed */
extern int iicbus_block_write(device_t, u_char, char *, int, int *);
extern int iicbus_block_read(device_t, u_char, char *, int, int *);

extern u_char iicbus_get_addr(device_t);

#endif
