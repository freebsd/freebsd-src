/*-
 * Copyright (c) 2011 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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
 *
 */

#ifndef __T4_IOCTL_H__
#define __T4_IOCTL_H__

/*
 * Ioctl commands specific to this driver.
 */
enum {
	T4_GET32 = 0x40,		/* read 32 bit register */
	T4_SET32,			/* write 32 bit register */
	T4_REGDUMP,			/* dump of all registers */
};

struct t4_reg32 {
	uint32_t addr;
	uint32_t val;
};

#define T4_REGDUMP_SIZE  (160 * 1024)
struct t4_regdump {
	uint32_t  version;
	uint32_t  len; /* bytes */
	uint8_t   *data;
};

#define CHELSIO_T4_GETREG32	_IOWR('f', T4_GET32, struct t4_reg32)
#define CHELSIO_T4_SETREG32	_IOW('f', T4_SET32, struct t4_reg32)
#define CHELSIO_T4_REGDUMP	_IOWR('f', T4_REGDUMP, struct t4_regdump)
#endif
