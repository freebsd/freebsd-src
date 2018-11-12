/*-
 * Copyright (c) 2015 Oleksandr Tymoshenko <gonzo@freebsd.org>
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

#ifndef	IMX_IOMUXREG_H
#define	IMX_IOMUXREG_H

#define	IOMUXC_GPR0	0x00
#define	IOMUXC_GPR1	0x04
#define	IOMUXC_GPR2	0x08
#define	IOMUXC_GPR3	0x0C
#define		IOMUXC_GPR3_HDMI_MASK		(3 << 2)
#define		IOMUXC_GPR3_HDMI_IPU1_DI0	(0 << 2)
#define		IOMUXC_GPR3_HDMI_IPU1_DI1	(1 << 2)
#define		IOMUXC_GPR3_HDMI_IPU2_DI0	(2 << 2)
#define		IOMUXC_GPR3_HDMI_IPU2_DI1	(3 << 2)

#endif
