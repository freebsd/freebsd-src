/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Adrian Chadd <adrian@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 */

#ifndef	__W25NREG_H__
#define	__W25NREG_H__

/*
 * Commands
 */
#define	CMD_READ_STATUS		0x05
#define	CMD_FAST_READ		0x0B
#define	CMD_PAGE_DATA_READ	0x13
#define	CMD_READ_IDENT		0x9F
#define	CMD_LAST_ECC_FAILURE	0xA9
#define	CMD_BLOCK_ERAS		0xD8

/*
 * Three status registers - 0xAx, 0xBx, 0xCx.
 *
 * status register 1 (0xA0) is for protection config/status
 * status register 2 (0xB0) is for configuration config/status
 * status register 3 (0xC0) is for general status
 */

#define	STATUS_REG_1		0xA0
#define		STATUS_REG_1_SRP1	0x10
#define		STATUS_REG_1_WP_EN	0x20
#define		STATUS_REG_1_TOP_BOTTOM_PROT	0x40
#define		STATUS_REG_1_BP0	0x80
#define		STATUS_REG_1_BP1	0x10
#define		STATUS_REG_1_BP2	0x20
#define		STATUS_REG_1_BP3	0x40
#define		STATUS_REG_1_SRP0	0x80

#define	STATUS_REG_2			0xB0
#define		STATUS_REG_2_BUF_EN	0x08
#define		STATUS_REG_2_ECC_EN	0x10
#define		STATUS_REG_2_SR1_LOCK	0x20
#define		STATUS_REG_2_OTP_EN	0x40
#define		STATUS_REG_2_OTP_L	0x80

#define	STATUS_REG_3			0xC0
#define		STATUS_REG_3_BUSY		0x01
#define		STATUS_REG_3_WRITE_EN_LATCH	0x02
#define		STATUS_REG_3_ERASE_FAIL		0x04
#define		STATUS_REG_3_PROGRAM_FAIL	0x08
#define		STATUS_REG_3_ECC_STATUS_0	0x10
#define		STATUS_REG_3_ECC_STATUS_1	0x20
#define		STATUS_REG_3_ECC_STATUS_SHIFT	4
#define		STATUS_REG_3_ECC_STATUS_MASK	0x03
#define		STATUS_REG_3_BBM_LUT_FULL	0x40

/* ECC status */
#define		STATUS_ECC_OK			0
#define		STATUS_ECC_1BIT_OK		1
#define		STATUS_ECC_2BIT_ERR		2
#define		STATUS_ECC_2BIT_ERR_MULTIPAGE	3

#endif	/* __W25NREG_H__ */
