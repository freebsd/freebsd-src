/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2015-2016 Andriy Voskoboinyk <avos@FreeBSD.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $OpenBSD: if_urtwnreg.h,v 1.3 2010/11/16 18:02:59 damien Exp $
 * $FreeBSD$
 */

#ifndef R88E_REG_H
#define R88E_REG_H

#include <dev/rtwn/rtl8192c/r92c_reg.h>

/*
 * MAC registers.
 */
/* System Configuration. */
#define R88E_BB_PAD_CTRL		0x064
#define R88E_HIMR			0x0b0
#define R88E_HISR			0x0b4
#define R88E_HIMRE			0x0b8
#define R88E_HISRE			0x0bc
/* MAC General Configuration. */
#define R88E_32K_CTRL			0x194
#define R88E_HMEBOX_EXT(idx)		(0x1f0 + (idx) * 4)
/* Protocol Configuration. */
#define R88E_TXPKTBUF_BCNQ1_BDNY	0x457
#define R88E_MACID_NO_LINK		0x484
#define R88E_TX_RPT_CTRL		0x4ec
#define R88E_TX_RPT_MACID_MAX		0x4ed
#define R88E_TX_RPT_TIME		0x4f0
#define R88E_SCH_TXCMD			0x5f8


/* Bits for R88E_HIMR. */
#define R88E_HIMR_CPWM			0x00000100
#define R88E_HIMR_CPWM2			0x00000200
#define R88E_HIMR_TBDER			0x04000000
#define R88E_HIMR_PSTIMEOUT		0x20000000

/* Bits for R88E_HIMRE.*/
#define R88E_HIMRE_RXFOVW		0x00000100
#define R88E_HIMRE_TXFOVW		0x00000200
#define R88E_HIMRE_RXERR		0x00000400
#define R88E_HIMRE_TXERR		0x00000800

/* Bits for R88E_TX_RPT_CTRL. */
#define R88E_TX_RPT1_ENA		0x01
#define R88E_TX_RPT2_ENA		0x02

/* Bits for R92C_MBID_NUM. */
#define R88E_MBID_TXBCN_RPT(id)		(0x08 << (id))

/* Bits for R92C_SECCFG. */
#define R88E_SECCFG_CHK_KEYID	0x0100


/*
 * Baseband registers.
 */
/* Bits for R92C_LSSI_PARAM(i). */
#define R88E_LSSI_PARAM_ADDR_M	0x0ff00000
#define R88E_LSSI_PARAM_ADDR_S	20


/*
 * RF (6052) registers.
 */
#define R88E_RF_T_METER		0x42

/* Bits for R92C_RF_CHNLBW. */
#define R88E_RF_CHNLBW_BW20	0x00c00

/* Bits for R88E_RF_T_METER. */
#define R88E_RF_T_METER_VAL_M	0x0fc00
#define R88E_RF_T_METER_VAL_S	10
#define R88E_RF_T_METER_START	0x30000

#endif	/* R88E_REG_H */
