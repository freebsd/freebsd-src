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

#ifndef	__QCOM_QUP_REG_H__
#define	__QCOM_QUP_REG_H__

#define	QUP_CONFIG			0x0000
#define		QUP_CONFIG_N			0x001f
#define		QUP_CONFIG_SPI_MODE		(1U << 8)
#define		QUP_CONFIG_MINI_CORE_I2C_MASTER	(2U << 8)
#define		QUP_CONFIG_MINI_CORE_I2C_SLAVE	(3U << 8)

#define		QUP_CONFIG_NO_OUTPUT		(1U << 6)
#define		QUP_CONFIG_NO_INPUT		(1U << 7)
#define		QUP_CONFIG_APP_CLK_ON_EN	(1 << 12)
#define		QUP_CONFIG_CLOCK_AUTO_GATE	(1U << 13)

#define	QUP_STATE			0x0004
#define		QUP_STATE_VALID		(1U << 2)
#define		QUP_STATE_RESET		0
#define		QUP_STATE_RUN		1
#define		QUP_STATE_PAUSE		3
#define		QUP_STATE_MASK		3
#define		QUP_STATE_CLEAR		2

#define	QUP_IO_M_MODES			0x0008
#define		QUP_IO_M_OUTPUT_BLOCK_SIZE_MASK		0x3
#define		QUP_IO_M_OUTPUT_BLOCK_SIZE_SHIFT	0

#define		QUP_IO_M_OUTPUT_FIFO_SIZE_MASK		0x7
#define		QUP_IO_M_OUTPUT_FIFO_SIZE_SHIFT		2

#define		QUP_IO_M_INPUT_BLOCK_SIZE_MASK		0x3
#define		QUP_IO_M_INPUT_BLOCK_SIZE_SHIFT		5

#define		QUP_IO_M_INPUT_FIFO_SIZE_MASK		0x7
#define		QUP_IO_M_INPUT_FIFO_SIZE_SHIFT		7

#define		QUP_IO_M_PACK_EN		(1U << 15)
#define		QUP_IO_M_UNPACK_EN		(1U << 14)
#define		QUP_IO_M_INPUT_MODE_SHIFT	12
#define		QUP_IO_M_OUTPUT_MODE_SHIFT	10
#define		QUP_IO_M_INPUT_MODE_MASK	0x3
#define		QUP_IO_M_OUTPUT_MODE_MASK	0x3

#define		QUP_IO_M_MODE_FIFO		0
#define		QUP_IO_M_MODE_BLOCK		1
#define		QUP_IO_M_MODE_DMOV		2
#define		QUP_IO_M_MODE_BAM		3

#define	QUP_SW_RESET			0x000c

#define	QUP_OPERATIONAL			0x0018
#define		QUP_OP_IN_BLOCK_READ_REQ	(1U << 13)
#define		QUP_OP_OUT_BLOCK_WRITE_REQ	(1U << 12)
#define		QUP_OP_MAX_INPUT_DONE_FLAG	(1U << 11)
#define		QUP_OP_MAX_OUTPUT_DONE_FLAG	(1U << 10)
#define		QUP_OP_IN_SERVICE_FLAG		(1U << 9)
#define		QUP_OP_OUT_SERVICE_FLAG		(1U << 8)
#define		QUP_OP_IN_FIFO_FULL		(1U << 7)
#define		QUP_OP_OUT_FIFO_FULL		(1U << 6)
#define		QUP_OP_IN_FIFO_NOT_EMPTY	(1U << 5)
#define		QUP_OP_OUT_FIFO_NOT_EMPTY	(1U << 4)

#define	QUP_ERROR_FLAGS			0x001c
#define	QUP_ERROR_FLAGS_EN		0x0020
#define		QUP_ERROR_OUTPUT_OVER_RUN	(1U << 5)
#define		QUP_ERROR_INPUT_UNDER_RUN	(1U << 4)
#define		QUP_ERROR_OUTPUT_UNDER_RUN	(1U << 3)
#define		QUP_ERROR_INPUT_OVER_RUN	(1U << 2)

#define	QUP_OPERATIONAL_MASK		0x0028

#define	QUP_HW_VERSION			0x0030
#define		QUP_HW_VERSION_2_1_1		0x20010001

#define	QUP_MX_OUTPUT_CNT		0x0100
#define	QUP_MX_OUTPUT_CNT_CURRENT	0x0104
#define	QUP_OUTPUT_FIFO			0x0110
#define	QUP_MX_WRITE_CNT		0x0150
#define	QUP_MX_WRITE_CNT_CURRENT	0x0154
#define	QUP_MX_INPUT_CNT		0x0200
#define	QUP_MX_INPUT_CNT_CURRENT	0x0204
#define	QUP_MX_READ_CNT			0x0208
#define	QUP_MX_READ_CNT_CURRENT		0x020c
#define	QUP_INPUT_FIFO			0x0218

#endif	/* __QCOM_QUP_REG_H__ */

