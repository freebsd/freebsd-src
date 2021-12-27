/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>
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

#ifndef	__QCOM_SPI_REG_H__
#define	__QCOM_SPI_REG_H__

#define	SPI_CONFIG			0x0300
#define		SPI_CONFIG_HS_MODE	(1U << 10)
#define		SPI_CONFIG_INPUT_FIRST	(1U << 9)
#define		SPI_CONFIG_LOOPBACK	(1U << 8)

#define	SPI_IO_CONTROL			0x0304
#define		SPI_IO_C_FORCE_CS		(1U << 11)
#define		SPI_IO_C_CLK_IDLE_HIGH		(1U << 10)
#define		SPI_IO_C_MX_CS_MODE		(1U << 8)
#define		SPI_IO_C_CS_N_POLARITY_0	(1U << 4)
#define		SPI_IO_C_CS_SELECT(x)		(((x) & 3) << 2)
#define		SPI_IO_C_CS_SELECT_MASK		0x000c
#define		SPI_IO_C_TRISTATE_CS		(1U << 1)
#define		SPI_IO_C_NO_TRI_STATE		(1U << 0)

#define	SPI_ERROR_FLAGS			0x0308
#define	SPI_ERROR_FLAGS_EN		0x030c
#define		SPI_ERROR_CLK_OVER_RUN		(1U << 1)
#define		SPI_ERROR_CLK_UNDER_RUN		(1U << 0)

/*
 * Strictly this isn't true; some controllers have
 * less CS lines exposed via GPIO/pinmux.
 */
#define	SPI_NUM_CHIPSELECTS		4

/*
 * The maximum single SPI transaction done in any mode.
 * Ie, if you have a PIO/DMA transaction larger than
 * this then it must be split up into SPI_MAX_XFER
 * sub-transactions in the transfer loop.
 */
#define	SPI_MAX_XFER			(65536 - 64)

/*
 * Any frequency at or above 26MHz is considered "high"
 * and will have some different parameters configured.
 */
#define	SPI_HS_MIN_RATE			26000000

#define	SPI_MAX_RATE			50000000

#endif	/* __QCOM_SPI_REG_H__ */

