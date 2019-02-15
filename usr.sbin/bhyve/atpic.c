/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "inout.h"

#define	IO_ICU1		0x20
#define	IO_ICU2		0xA0
#define	ICU_IMR_OFFSET	1

static int
atpic_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
	      uint32_t *eax, void *arg)
{
	if (bytes != 1)
		return (-1);

	if (in) {
		if (port & ICU_IMR_OFFSET) {
			/* all interrupts masked */
			*eax = 0xff;
		} else {
			*eax = 0x00;
		}
	}

	/* Pretend all writes to the 8259 are alright */
	return (0);
}

INOUT_PORT(atpic, IO_ICU1, IOPORT_F_INOUT, atpic_handler);
INOUT_PORT(atpic, IO_ICU1 + ICU_IMR_OFFSET, IOPORT_F_INOUT, atpic_handler);
INOUT_PORT(atpic, IO_ICU2, IOPORT_F_INOUT, atpic_handler);
INOUT_PORT(atpic, IO_ICU2 + ICU_IMR_OFFSET, IOPORT_F_INOUT, atpic_handler);
