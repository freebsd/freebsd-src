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

#ifndef	__QCOM_ESS_EDMA_DEBUG_H__
#define	__QCOM_ESS_EDMA_DEBUG_H__

#define	QCOM_ESS_EDMA_DBG_INTERRUPT			0x00000001
#define	QCOM_ESS_EDMA_DBG_DESCRIPTOR_SETUP		0x00000002
#define	QCOM_ESS_EDMA_DBG_RX_RING_MGMT			0x00000004
#define	QCOM_ESS_EDMA_DBG_TX_RING_MGMT			0x00000008
#define	QCOM_ESS_EDMA_DBG_RX_FRAME			0x00000010
#define	QCOM_ESS_EDMA_DBG_RX_RING			0x00000020
#define	QCOM_ESS_EDMA_DBG_TX_FRAME			0x00000040
#define	QCOM_ESS_EDMA_DBG_TX_RING			0x00000080
#define	QCOM_ESS_EDMA_DBG_TX_RING_COMPLETE		0x00000100
#define	QCOM_ESS_EDMA_DBG_TX_TASK			0x00000200
#define	QCOM_ESS_EDMA_DBG_TX_FRAME_ERROR		0x00000400
#define	QCOM_ESS_EDMA_DBG_STATE				0x00000800

#define	QCOM_ESS_EDMA_DPRINTF(sc, flags, ...)				\
	do {								\
		if ((sc)->sc_debug & (flags))				\
			device_printf((sc)->sc_dev, __VA_ARGS__);	\
	} while (0)

#endif	/* __QCOM_ESS_EDMA_DEBUG_H__ */
