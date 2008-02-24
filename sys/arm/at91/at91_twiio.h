/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
 *
 * $FreeBSD: src/sys/arm/at91/at91_twiio.h,v 1.1 2006/02/04 23:32:13 imp Exp $
 */

#ifndef _ARM_AT91_AT91_TWIIO_H
#define _ARM_AT91_AT91_TWIIO_H

#include <sys/ioccom.h>

struct at91_twi_io
{
	int	dadr;			/* Device address */
	int	type;			/* read/write */
#define TWI_IO_READ_MASTER	1
#define TWI_IO_WRITE_MASTER	2
	int	iadrsz;			/* Internal addr size */
	uint32_t iadr;			/* Interbak addr */
	size_t  xfer_len;		/* Size to transfer */
	caddr_t xfer_buf;		/* buffer for xfer */
};

struct at91_twi_clock
{
	int	ckdiv;			/* Clock divider */
	int	high_rate;		/* rate of clock high period */
	int	low_rate;		/* rate of clock low period */
};

/** TWIIOCXFER: Do a two-wire transfer
 */
#define TWIIOCXFER	_IOW('x', 1, struct at91_twi_io)

/** TWIIOCSETCLOCK: Sets the clocking parameters for this operation.
 */
#define TWIIOCSETCLOCK _IOW('x', 2, struct at91_twi_clock)

#endif /* !_ARM_AT91_AT91_TWIIO_H */


