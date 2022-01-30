/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Adrian Chadd <adrian@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
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

/*
 * DT constants for the Qualcomm QCA807x PHY
 */

#ifndef	_DT_BINDINGS_NET_QCOM_QCA807X_H__
#define	_DT_BINDINGS_NET_QCOM_QCA807X_H__

/*
 * PSGMII driver configuration.  This controls the TX voltage
 * used between the SoC and the external PHY over the SERDES
 * interface.
 *
 * The default value is 12 (600mV)
 */
#define	PSGMII_QSGMII_TX_DRIVER_140MV	0
#define	PSGMII_QSGMII_TX_DRIVER_160MV	1
#define	PSGMII_QSGMII_TX_DRIVER_180MV	2
#define	PSGMII_QSGMII_TX_DRIVER_200MV	3
#define	PSGMII_QSGMII_TX_DRIVER_220MV	4
#define	PSGMII_QSGMII_TX_DRIVER_240MV	5
#define	PSGMII_QSGMII_TX_DRIVER_260MV	6
#define	PSGMII_QSGMII_TX_DRIVER_280MV	7
#define	PSGMII_QSGMII_TX_DRIVER_300MV	8
#define	PSGMII_QSGMII_TX_DRIVER_320MV	9
#define	PSGMII_QSGMII_TX_DRIVER_400MV	10
#define	PSGMII_QSGMII_TX_DRIVER_500MV	11
#define	PSGMII_QSGMII_TX_DRIVER_600MV	12

/*
 * These fields control the PHY power saving based on the
 * cable length.
 *
 * 0 - full amplitude, full bias current
 * 1 - amplitude follows cable length, half bias current
 * 2 - full amplitude, bias current follows cable length
 * 3 - both amplitude and bias current follow cable length
 * 4 - full amplitude, half bias current
 * 5 - amplitude follows cable length, 1/4 bias current
 *     when cable length < 10m else half bias current
 * 6 - full amplitude, bias current follows cable length,
 *     bias reduced further by half when cable length < 10m
 * 7 - amplitude follows cable length, same bias current
 *     setting as '6'
 */
#define	QCA807X_CONTROL_DAC_FULL_VOLT_BIAS		0
#define	QCA807X_CONTROL_DAC_DSP_VOLT_HALF_BIAS		1
#define	QCA807X_CONTROL_DAC_FULL_VOLT_DSP_BIAS		2
#define	QCA807X_CONTROL_DAC_DSP_VOLT_BIAS		3
#define	QCA807X_CONTROL_DAC_FULL_VOLT_HALF_BIAS		4
#define	QCA807X_CONTROL_DAC_DSP_VOLT_QUARTER_BIAS	5
#define	QCA807X_CONTROL_DAC_FULL_VOLT_HALF_BIAS_SHORT	6
#define	QCA807X_CONTROL_DAC_DSP_VOLT_HALF_BIAS_SHORT	7

#endif	/* __DT_BINDINGS_NET_QCOM_QCA807X_H__ */
