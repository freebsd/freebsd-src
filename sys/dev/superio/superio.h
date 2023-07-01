/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Andriy Gapon
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

#ifndef SUPERIO_H
#define SUPERIO_H

typedef enum superio_vendor {
	SUPERIO_VENDOR_NONE,
	SUPERIO_VENDOR_ITE,
	SUPERIO_VENDOR_NUVOTON,
	SUPERIO_VENDOR_FINTEK,
	SUPERIO_VENDOR_MAX
} superio_vendor_t;

typedef enum superio_dev_type {
	SUPERIO_DEV_NONE,
	SUPERIO_DEV_HWM,
	SUPERIO_DEV_WDT,
	SUPERIO_DEV_GPIO,
	SUPERIO_DEV_MAX
} superio_dev_type_t;

superio_vendor_t superio_vendor(device_t dev);
uint16_t superio_devid(device_t dev);
uint8_t superio_revid(device_t dev);
int superio_extid(device_t dev);
uint8_t superio_read(device_t dev, uint8_t reg);
uint8_t superio_ldn_read(device_t dev, uint8_t ldn, uint8_t reg);
void superio_write(device_t dev, uint8_t reg, uint8_t val);
void superio_ldn_write(device_t dev, uint8_t ldn, uint8_t reg, uint8_t val);
bool superio_dev_enabled(device_t dev, uint8_t mask);
void superio_dev_enable(device_t dev, uint8_t mask);
void superio_dev_disable(device_t dev, uint8_t mask);

device_t superio_find_dev(device_t superio, superio_dev_type_t type,
    int ldn);

enum superio_ivars {
	SUPERIO_IVAR_LDN =	10600,
	SUPERIO_IVAR_TYPE,
	SUPERIO_IVAR_IOBASE,
	SUPERIO_IVAR_IOBASE2,
	SUPERIO_IVAR_IRQ,
	SUPERIO_IVAR_DMA
};

#define	SUPERIO_ACCESSOR(var, ivar, type)				\
	__BUS_ACCESSOR(superio, var, SUPERIO, ivar, type)

SUPERIO_ACCESSOR(ldn,		LDN,		uint8_t)
SUPERIO_ACCESSOR(type,		TYPE,		superio_dev_type_t)
SUPERIO_ACCESSOR(iobase,	IOBASE,		uint16_t)
SUPERIO_ACCESSOR(iobase2,	IOBASE2,	uint16_t)
SUPERIO_ACCESSOR(irq,		IRQ,		uint8_t)
SUPERIO_ACCESSOR(dma,		DMA,		uint8_t)

#undef SUPERIO_ACCESSOR

#endif /*SUPERIO_H*/
