/*
 * Copyright (C) 2000
 * Dr. Duncan McLennan Barclay, dmlb@ragnet.demon.co.uk.
 *
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DUNCAN BARCLAY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DUNCAN BARCLAY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 *
 */

/*	$NetBSD: if_rayreg.h,v 1.1 2000/01/23 23:59:22 chopps Exp $	*/
/* 
 * Copyright (c) 2000 Christian E. Hopps
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
 * 3. Neither the name of the author nor the names of any co-contributors
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
 */

/*
 * CCR registers, appearing in the attribute memory space
 */
#define RAY_CCR		0xf00	/* CCR register offset */
#define RAY_COR		(RAY_CCR + (0x00 << 1))	/* config option register */
#define	RAY_CCSR	(RAY_CCR + (0x01 << 1))	/* config/status register */
#define	RAY_PIN		(RAY_CCR + (0x02 << 1))	/* not in hw */
#define	RAY_SOCKETCOPY	(RAY_CCR + (0x03 << 1))	/* not used by hw */
#define	RAY_HCSIR	(RAY_CCR + (0x05 << 1))	/* HCS intr register */
#define	RAY_ECFIR	(RAY_CCR + (0x06 << 1))	/* ECF intr register */
/*
 * We don't seem to be able to access these in a simple manner
 */
#define	RAY_AR0		(RAY_CCR + 0x08)	/* authorization register 0 (unused) */
#define	RAY_AR1		(RAY_CCR + 0x09)	/* authorization register 1 (unused) */
#define	RAY_PMR		(RAY_CCR + 0x0a)	/* program mode register (unused) */
#define	RAY_TMR		(RAY_CCR + 0x0b)	/* pc test mode register (unused) */
#define	RAY_FCWR	(RAY_CCR + 0x10)	/* frequency control word register */
#define RAY_TMC1	(RAY_CCR + 0x14)	/* test mode control 1 (unused) */
#define RAY_TMC2	(RAY_CCR + 0x15)	/* test mode control 1 (unused) */
#define RAY_TMC3	(RAY_CCR + 0x16)	/* test mode control 1 (unused) */
#define RAY_TMC4	(RAY_CCR + 0x17)	/* test mode control 1 (unused) */

/*
 * COR register bits
 */
#define	RAY_COR_CFG_NUM		0x01	/* currently ignored and set */
#define RAY_COR_CFG_MASK	0x3f	/* mask for function */
#define	RAY_COR_LEVEL_IRQ	0x40	/* currently ignored and set */
#define	RAY_COR_RESET		0x80	/* soft-reset the card */

/*
 * CCS register bits
 */
/* XXX the linux driver indicates bit 0 is the irq bit */
#define	RAY_CCS_IRQ		0x02	/* interrupt pending */
#define	RAY_CCS_POWER_DOWN	0x04

/*
 * HCSI register bits
 *
 * the host can only clear this bit.
 */
#define	RAY_HCSIR_IRQ		0x01	/* indicates an interrupt */

/*
 * ECFI register values
 */
#define	RAY_ECSIR_IRQ		0x01	/* interrupt the card */

/*
 * authorization register 0 values
 *    -- used for testing/programming the card (unused)
 */
#define	RAY_AR0_ON		0x57

/*
 * authorization register 1 values
 *	-- used for testing/programming the card (unused)
 */
#define	RAY_AR1_ON		0x82

/*
 * PMR bits -- these are used to program the card (unused)
 */
#define	RAY_PMR_PC2PM		0x02	/* grant access to firmware flash */
#define	RAY_PMR_PC2CAL		0x10	/* read access to the A/D modem inp */
#define	RAY_PMR_MLSE		0x20	/* read access to the MSLE prom */

/*
 * TMR bits -- get access to test modes (unused)
 */
#define	RAY_TMR_TEST		0x08	/* test mode */

/*
 * FCWR -- frequency control word, values from [0x02,0xA6] map to
 * RF frequency values.
 */

/*
 * 48k of memory
 */
#define	RAY_SRAM_MEM_BASE	0
#define	RAY_SRAM_MEM_SIZE	0xc000

/*
 * offsets into shared ram
 */
#define	RAY_SCB_BASE		0x0	/* cfg/status/ctl area */
#define	RAY_STATUS_BASE		0x0100
#define	RAY_HOST_TO_ECF_BASE	0x0200
#define	RAY_ECF_TO_HOST_BASE	0x0300
#define	RAY_CCS_BASE		0x0400
#define	RAY_RCS_BASE		0x0800
#define	RAY_APOINT_TIM_BASE	0x0c00
#define	RAY_SSID_LIST_BASE	0x0d00
#define	RAY_TX_BASE		0x1000
#define	RAY_TX_SIZE		0x7000
#define	RAY_TX_END		0x8000
#define	RAY_RX_BASE		0x8000
#define	RAY_RX_END		0xc000
#define	RAY_RX_MASK		0x3fff

struct ray_ecf_startup_v4 {
	u_int8_t	e_status;
	u_int8_t	e_station_addr[ETHER_ADDR_LEN];
	u_int8_t	e_prg_cksum;
	u_int8_t	e_cis_cksum;
	u_int8_t	e_resv0[7];
	u_int8_t	e_japan_callsign[12];
};
struct ray_ecf_startup_v5 {
	u_int8_t	e_status;
	u_int8_t	e_station_addr[ETHER_ADDR_LEN];
	u_int8_t	e_resv0;
	u_int8_t	e_rates[8];
	u_int8_t	e_japan_callsign[12];
	u_int8_t	e_prg_cksum;
	u_int8_t	e_cis_cksum;
	u_int8_t	e_fw_build_string;
	u_int8_t	e_fw_build;
	u_int8_t	e_fw_resv;
	u_int8_t	e_asic_version;
	u_int8_t	e_tib_size;
	u_int8_t	e_resv1[29];
};

/*
 * Status word result codes
 */
#define	RAY_ECFS_RESERVED0		0x01
#define	RAY_ECFS_PROC_SELF_TEST		0x02
#define	RAY_ECFS_PROG_MEM_CHECKSUM	0x04
#define	RAY_ECFS_DATA_MEM_TEST		0x08
#define	RAY_ECFS_RX_CALIBRATION		0x10
#define	RAY_ECFS_FW_VERSION_COMPAT	0x20
#define	RAY_ECFS_RERSERVED1		0x40
#define	RAY_ECFS_TEST_COMPLETE		0x80
#define	RAY_ECFS_CARD_OK		RAY_ECFS_TEST_COMPLETE

/*
 * Firmware build codes
 */
#define	RAY_ECFS_BUILD_4		0x55
#define	RAY_ECFS_BUILD_5		0x5
