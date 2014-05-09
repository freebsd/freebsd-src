/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * RMI_BSD */
/* MDIO Low level Access routines */
/* All Phy's accessed from GMAC0 base */

#ifndef _XGMAC_MDIO_H_
#define _XGMAC_MDIO_H_

static inline int
xmdio_read(volatile unsigned int *_mmio,
    uint32_t phy_addr, uint32_t address);
static inline void
xmdio_write(volatile unsigned int *_mmio,
    uint32_t phy_addr, uint32_t address, uint32_t data);
static inline void
xmdio_address(volatile unsigned int *_mmio,
    uint32_t phy_addr, uint32_t dev_ad, uint32_t address);

static inline void
xmdio_address(volatile unsigned int *_mmio,
    uint32_t phy_addr, uint32_t dev_ad, uint32_t address)
{
	uint32_t st_field = 0x0;
	uint32_t op_type = 0x0;	/* address operation */
	uint32_t ta_field = 0x2;/* ta field */

	_mmio[0x11] = ((st_field & 0x3) << 30) |
	    ((op_type & 0x3) << 28) |
	    ((phy_addr & 0x1F) << 23) |
	    ((dev_ad & 0x1F) << 18) |
	    ((ta_field & 0x3) << 16) |
	    ((address & 0xffff) << 0);

	_mmio[0x10] = (0x0 << 3) | 0x5;
	_mmio[0x10] = (0x1 << 3) | 0x5;
	_mmio[0x10] = (0x0 << 3) | 0x5;

	/* wait for dev_ad cycle to complete */
	while (_mmio[0x14] & 0x1) {
	};

}

/* function prototypes */
static inline int
xmdio_read(volatile unsigned int *_mmio,
    uint32_t phy_addr, uint32_t address)
{
	uint32_t st_field = 0x0;
	uint32_t op_type = 0x3;	/* read operation */
	uint32_t ta_field = 0x2;/* ta field */
	uint32_t data = 0;

	xmdio_address(_mmio, phy_addr, 5, address);
	_mmio[0x11] = ((st_field & 0x3) << 30) |
	    ((op_type & 0x3) << 28) |
	    ((phy_addr & 0x1F) << 23) |
	    ((5 & 0x1F) << 18) |
	    ((ta_field & 0x3) << 16) |
	    ((data & 0xffff) << 0);

	_mmio[0x10] = (0x0 << 3) | 0x5;
	_mmio[0x10] = (0x1 << 3) | 0x5;
	_mmio[0x10] = (0x0 << 3) | 0x5;

	/* wait for write cycle to complete */
	while (_mmio[0x14] & 0x1) {
	};

	data = _mmio[0x11] & 0xffff;
	return (data);
}

static inline void
xmdio_write(volatile unsigned int *_mmio,
    uint32_t phy_addr, uint32_t address, uint32_t data)
{
	uint32_t st_field = 0x0;
	uint32_t op_type = 0x1;	/* write operation */
	uint32_t ta_field = 0x2;/* ta field */

	xmdio_address(_mmio, phy_addr, 5, address);
	_mmio[0x11] = ((st_field & 0x3) << 30) |
	    ((op_type & 0x3) << 28) |
	    ((phy_addr & 0x1F) << 23) |
	    ((5 & 0x1F) << 18) |
	    ((ta_field & 0x3) << 16) |
	    ((data & 0xffff) << 0);

	_mmio[0x10] = (0x0 << 3) | 0x5;
	_mmio[0x10] = (0x1 << 3) | 0x5;
	_mmio[0x10] = (0x0 << 3) | 0x5;

	/* wait for write cycle to complete */
	while (_mmio[0x14] & 0x1) {
	};

}

#endif
