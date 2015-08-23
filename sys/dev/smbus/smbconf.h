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
#ifndef __SMBONF_H
#define __SMBONF_H

#include <sys/queue.h>

#define SMBPRI (PZERO+8)		/* XXX sleep/wakeup queue priority */

#define n(flags) (~(flags) & (flags))

/*
 * How tsleep() is called in smb_request_bus().
 */
#define SMB_DONTWAIT	0
#define SMB_NOINTR	0
#define SMB_WAIT	0x1
#define SMB_INTR	0x2

/*
 * callback index
 */
#define SMB_REQUEST_BUS	0x1
#define SMB_RELEASE_BUS	0x2

/*
 * SMB bus errors
 */
#define SMB_ENOERR	0x0
#define SMB_EBUSERR	0x1
#define SMB_ENOTSUPP	0x2
#define SMB_ENOACK	0x4
#define SMB_ECOLLI	0x8
#define SMB_EABORT	0x10
#define SMB_ETIMEOUT	0x20
#define SMB_EBUSY	0x40
#define	SMB_EINVAL	0x100

/*
 * How Quick command is executed
 */
#define SMB_QWRITE	0x0
#define SMB_QREAD	0x1

/*
 * smbus transction op with pass-thru capabilities
 *
 * This smbus function is capable of doing a smbus command transaction
 * (read or write), and can be flagged to not issue the 'cmd' and/or
 * issue or expect a count field as well as flagged for chaining (no STOP),
 * which gives it an i2c pass-through capability.
 *
 * NOSTOP- Caller chaining transactions, do not issue STOP
 * NOCMD-  Do not transmit the command field
 * NOCNT-  Do not transmit (wr) or expect (rd) the count field
 */
#define SMB_TRANS_NOSTOP  0x0001  /* do not send STOP at end */
#define SMB_TRANS_NOCMD   0x0002  /* ignore cmd field (do not tx) */ 
#define SMB_TRANS_NOCNT   0x0004  /* do not tx or rx count field */  
#define SMB_TRANS_7BIT    0x0008  /* change address mode to 7-bit */ 
#define SMB_TRANS_10BIT   0x0010  /* change address mode to 10-bit */
#define SMB_TRANS_NOREPORT  0x0020  /* do not report errors */

/*
 * ivars codes
 */
enum smbus_ivars {
    SMBUS_IVAR_ADDR,	/* slave address of the device */
};

int	smbus_request_bus(device_t, device_t, int);
int	smbus_release_bus(device_t, device_t);
device_t smbus_alloc_bus(device_t);
int	smbus_error(int error);

void	smbus_intr(device_t, u_char, char low, char high, int error);

#define SMBUS_ACCESSOR(var, ivar, type)					\
	__BUS_ACCESSOR(smbus, var, SMBUS, ivar, type)

SMBUS_ACCESSOR(addr,		ADDR,		int)

#undef SMBUS_ACCESSOR

extern driver_t smbus_driver;
extern devclass_t smbus_devclass;

#define smbus_quick(bus,slave,how) \
	(SMBUS_QUICK(device_get_parent(bus), slave, how))
#define smbus_sendb(bus,slave,byte) \
	(SMBUS_SENDB(device_get_parent(bus), slave, byte))
#define smbus_recvb(bus,slave,byte) \
	(SMBUS_RECVB(device_get_parent(bus), slave, byte))
#define smbus_writeb(bus,slave,cmd,byte) \
	(SMBUS_WRITEB(device_get_parent(bus), slave, cmd, byte))
#define smbus_writew(bus,slave,cmd,word) \
	(SMBUS_WRITEW(device_get_parent(bus), slave, cmd, word))
#define smbus_readb(bus,slave,cmd,byte) \
	(SMBUS_READB(device_get_parent(bus), slave, cmd, byte))
#define smbus_readw(bus,slave,cmd,word) \
	(SMBUS_READW(device_get_parent(bus), slave, cmd, word))
#define smbus_pcall(bus,slave,cmd,sdata,rdata) \
	(SMBUS_PCALL(device_get_parent(bus), slave, cmd, sdata, rdata))
#define smbus_bwrite(bus,slave,cmd,count,buf) \
	(SMBUS_BWRITE(device_get_parent(bus), slave, cmd, count, buf))
#define smbus_bread(bus,slave,cmd,count,buf) \
	(SMBUS_BREAD(device_get_parent(bus), slave, cmd, count, buf))
#define smbus_trans(bus,slave,cmd,op,wbuf,wcount,rbuf,rcount,actualp) \
	(SMBUS_TRANS(device_get_parent(bus), slave, cmd, op, \
	wbuf, wcount, rbuf, rcount, actualp))

#define SMBUS_MODVER	1
#define SMBUS_MINVER	1
#define SMBUS_MAXVER	1
#define SMBUS_PREFVER	SMBUS_MODVER

#endif
