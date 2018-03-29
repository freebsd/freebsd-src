/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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

#ifdef PC98
#define COM_IF_INTERNAL		0x00
#define COM_IF_PC9861K_1	0x01
#define COM_IF_PC9861K_2	0x02
#define COM_IF_IND_SS_1		0x03
#define COM_IF_IND_SS_2		0x04
#define COM_IF_PIO9032B_1	0x05
#define COM_IF_PIO9032B_2	0x06
#define COM_IF_B98_01_1		0x07
#define COM_IF_B98_01_2		0x08
#define COM_IF_END1		COM_IF_B98_01_2
#define COM_IF_RSA98		0x10	/* same as COM_IF_NS16550 */
#define COM_IF_NS16550		0x11
#define COM_IF_SECOND_CCU	0x12	/* same as COM_IF_NS16550 */
#define COM_IF_MC16550II	0x13
#define COM_IF_MCRS98		0x14	/* same as COM_IF_MC16550II */
#define COM_IF_RSB3000		0x15
#define COM_IF_RSB384		0x16
#define COM_IF_MODEM_CARD	0x17
#define COM_IF_RSA98III		0x18
#define COM_IF_ESP98		0x19
#define COM_IF_END2		COM_IF_ESP98

#define GET_IFTYPE(type)	(((type) >> 24) & 0x1f)
#define SET_IFTYPE(type)	((type) << 24)

#define SET_FLAG(dev, bit) device_set_flags(dev, device_get_flags(dev) | (bit))
#define CLR_FLAG(dev, bit) device_set_flags(dev, device_get_flags(dev) & ~(bit))
#endif /* PC98 */

int	sioattach(device_t dev, int xrid, u_long rclk);
int	siodetach(device_t dev);
int	sioprobe(device_t dev, int xrid, u_long rclk, int noprobe);

extern	devclass_t	sio_devclass;
extern	char		sio_driver_name[];
