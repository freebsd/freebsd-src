/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 */

#ifndef	__QCOM_TCSR_REG_H__
#define	__QCOM_TCSR_REG_H__

#define	QCOM_TCSR_USB_PORT_SEL_BASE		0x1A4000B0
#define	QCOM_TCSR_USB_PORT_SEL			0xB0
#define	QCOM_TCSR_USB_HSPHY_CONFIG		0x0C

#define	QCOM_TCSR_ESS_INTERFACE_SEL_OFFSET	0x0
#define	QCOM_TCSR_ESS_INTERFACE_SEL_MASK	0xF

#define	QCOM_TCSR_WIFI0_GLB_CFG_OFFSET		0x0
#define	QCOM_TCSR_WIFI1_GLB_CFG_OFFSET		0x4
#define	QCOM_TCSR_PNOC_SNOC_MEMTYPE_M0_M2	0x4

#endif	/* __QCOM_TCSR_REG_H__ */
