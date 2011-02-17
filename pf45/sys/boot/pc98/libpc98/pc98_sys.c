/*-
 * Copyright (c) 2009 TAKAHASHI Yoshihiro <nyan@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <btxv86.h>
#include <machine/cpufunc.h>
#define _KERNEL
#include <pc98/pc98/pc98_machdep.h>

/*
 * Set machine type to PC98_SYSTEM_PARAMETER.
 */
void
set_machine_type(void)
{
	int i;
	u_long ret, data;

	/* PC98_SYSTEM_PARAMETER (0x501) */
	ret = ((*(u_char *)PTOV(0xA1501)) & 0x08) >> 3;

	/* Wait V-SYNC */
	while (inb(0x60) & 0x20) {}
	while (!(inb(0x60) & 0x20)) {}

	/* ANK 'A' font */
	outb(0xa1, 0x00);
	outb(0xa3, 0x41);

	/* M_NORMAL, use CG window (all NEC OK)  */
	for (i = data = 0; i < 4; i++)
		data += *((u_long *)PTOV(0xA4000) + i);	/* 0xa4000 */
	if (data == 0x6efc58fc)		/* DA data */
		ret |= M_NEC_PC98;
	else
		ret |= M_EPSON_PC98;
	ret |= (inb(0x42) & 0x20) ? M_8M : 0;

	/* PC98_SYSTEM_PARAMETER(0x400) */
	if ((*(u_char *)PTOV(0xA1400)) & 0x80)
		ret |= M_NOTE;
	if (ret & M_NEC_PC98) {
		/* PC98_SYSTEM_PARAMETER(0x458) */
		if ((*(u_char *)PTOV(0xA1458)) & 0x80)
			ret |= M_H98;
		else
			ret |= M_NOT_H98;
	} else
		ret |= M_NOT_H98;

	(*(u_long *)PTOV(0xA1620)) = ret;
}
