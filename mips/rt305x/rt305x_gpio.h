/*-
 * Copyright (c) 2010 Aleksandr Rybalko.
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
#ifndef _RT305X_GPIO_H_
#define _RT305X_GPIO_H_

#define NGPIO			52

#define RGMII_GPIO_MODE_MASK	(0x0fffULL<<40)
#define SDRAM_GPIO_MODE_MASK  	(0xffffULL<<24)
#define MDIO_GPIO_MODE_MASK   	(0x0003ULL<<22)
#define JTAG_GPIO_MODE_MASK   	(0x001fULL<<17)
#define UARTL_GPIO_MODE_MASK  	(0x0003ULL<<15)
#define UARTF_GPIO_MODE_MASK  	(0x00ffULL<<7)
#define SPI_GPIO_MODE_MASK    	(0x000fULL<<3)
#define I2C_GPIO_MODE_MASK    	(0x0003ULL<<1)

#define GPIO23_00_INT		0x00 /* Programmed I/O Int Status */
#define GPIO23_00_EDGE		0x04 /* Programmed I/O Edge Status */
#define GPIO23_00_RENA		0x08 /* Programmed I/O Int on Rising */
#define GPIO23_00_FENA		0x0C /* Programmed I/O Int on Falling */
#define GPIO23_00_DATA		0x20 /* Programmed I/O Data */
#define GPIO23_00_DIR		0x24 /* Programmed I/O Direction */
#define GPIO23_00_POL		0x28 /* Programmed I/O Pin Polarity */
#define GPIO23_00_SET		0x2C /* Set PIO Data Bit */
#define GPIO23_00_RESET		0x30 /* Clear PIO Data bit */
#define GPIO23_00_TOG		0x34 /* Toggle PIO Data bit */

#define GPIO39_24_INT		0x38
#define GPIO39_24_EDGE		0x3c
#define GPIO39_24_RENA		0x40
#define GPIO39_24_FENA		0x44
#define GPIO39_24_DATA		0x48
#define GPIO39_24_DIR		0x4c
#define GPIO39_24_POL		0x50
#define GPIO39_24_SET		0x54
#define GPIO39_24_RESET		0x58
#define GPIO39_24_TOG		0x5c

#define GPIO51_40_INT		0x60
#define GPIO51_40_EDGE		0x64
#define GPIO51_40_RENA		0x68
#define GPIO51_40_FENA		0x6C
#define GPIO51_40_DATA		0x70
#define GPIO51_40_DIR		0x74
#define GPIO51_40_POL		0x78
#define GPIO51_40_SET		0x7C
#define GPIO51_40_RESET		0x80
#define GPIO51_40_TOG		0x84

#define GPIO_REG(g, n)							\
	((g<24)?(GPIO23_00_##n):(g<40)?(GPIO39_24_##n):(GPIO51_40_##n))
#define GPIO_MASK(g)							\
	((g<24)?(1<<g):(g<40)?(1<<(g-24)):(1<<(g-40)))
#define GPIO_BIT_SHIFT(g)	((g<24)?(g):(g<40)?(g-24):(g-40))

#define GPIO_READ(r, g, n) 						\
	bus_read_4(r->gpio_mem_res, GPIO_REG(g, n))
#define GPIO_WRITE(r, g, n, v) 						\
	bus_write_4(r->gpio_mem_res, GPIO_REG(g, n), v)
#define GPIO_READ_ALL(r, n) 						\
	(((uint64_t)bus_read_4(r->gpio_mem_res, GPIO23_00_##n)) |	\
	(((uint64_t)bus_read_4(r->gpio_mem_res, GPIO39_24_##n)) << 24) |\
	(((uint64_t)bus_read_4(r->gpio_mem_res, GPIO51_40_##n)) << 40))
#define GPIO_WRITE_ALL(r, n, v) 					\
	{bus_write_4(r->gpio_mem_res,GPIO23_00_##n, v      &0x00ffffff);\
	bus_write_4(r->gpio_mem_res, GPIO39_24_##n, (v>>24)&0x0000ffff);\
	bus_write_4(r->gpio_mem_res, GPIO51_40_##n, (v>>40)&0x00000fff);}


#define GPIO_BIT_CLR(r, g, n) 						\
	bus_write_4(r->gpio_mem_res, GPIO_REG(g, n), 			\
	    bus_read_4(r->gpio_mem_res, GPIO_REG(g, n)) & ~GPIO_MASK(g))
#define GPIO_BIT_SET(r, g, n) 						\
	bus_write_4(r->gpio_mem_res, GPIO_REG(g, n), 			\
	    bus_read_4(r->gpio_mem_res, GPIO_REG(g, n)) | GPIO_MASK(g))

#define GPIO_BIT_GET(r, g, n)						\
	((bus_read_4(r->gpio_mem_res, GPIO_REG(g, n)) >> 		\
	    GPIO_BIT_SHIFT(g)) & 1)

#define GPIO_LOCK(_sc)		mtx_lock(&(_sc)->gpio_mtx)
#define GPIO_UNLOCK(_sc)	mtx_unlock(&(_sc)->gpio_mtx)
#define GPIO_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->gpio_mtx, MA_OWNED)

#endif /* _RT305X_GPIO_H_ */

