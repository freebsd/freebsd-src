/*-
 * Copyright (c) 2015 Landon Fuller <landon@landonf.org>
 * Copyright (c) 2010 Broadcom Corporation
 *
 * This file is derived from the hndsoc.h header distributed with
 * Broadcom's initial brcm80211 Linux driver release, as
 * contributed to the Linux staging repository.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 * $FreeBSD$
 */

#ifndef _BHND_BHND_CORE_H_
#define _BHND_BHND_CORE_H_

/* Common core control flags */
#define	BHND_CF 		0x0408
#define	BHND_CF_BIST_EN		0x8000		/**< built-in self test */
#define	BHND_CF_PME_EN		0x4000		/**< ??? */
#define	BHND_CF_CORE_BITS	0x3ffc		/**< core specific flag mask */
#define	BHND_CF_FGC		0x0002		/**< force clock gating */
#define	BHND_CF_CLOCK_EN	0x0001		/**< enable clock */

/* Common core status flags */
#define	BHND_SF			0x0500
#define	BHND_SF_BIST_DONE	0x8000		/**< ??? */
#define	BHND_SF_BIST_ERROR	0x4000		/**< ??? */
#define	BHND_SF_GATED_CLK	0x2000		/**< clock gated */
#define	BHND_SF_DMA64		0x1000		/**< supports 64-bit DMA */
#define	BHND_SF_CORE_BITS	0x0fff		/**< core-specific status mask */

/*Reset core control flags */
#define	BHND_RESET_CF		0x0800
#define	BHND_RESET_CF_ENABLE	0x0001

#define	BHND_RESET_SF		0x0804

/* 
 * A register that is common to all cores to
 * communicate w/PMU regarding clock control.
 * 
 * TODO: Determine when this register first appeared.
 */
#define BHND_CLK_CTL_ST			0x1e0		/**< clock control and status */

/*
 * BHND_CLK_CTL_ST register 
 * 
 * Clock Mode		Name	Description
 * High Throughput	(HT)	Full bandwidth, low latency. Generally supplied
 * 				from PLL.
 * Active Low Power	(ALP)	Register access, low speed DMA.
 * Idle Low Power	(ILP)	No interconnect activity, or if long latency
 * 				is permitted.
 */
#define	BHND_CCS_FORCEALP		0x00000001	/**< force ALP request */
#define	BHND_CCS_FORCEHT		0x00000002	/**< force HT request */
#define	BHND_CCS_FORCEILP		0x00000004	/**< force ILP request */
#define	BHND_CCS_ALPAREQ		0x00000008	/**< ALP Avail Request */
#define	BHND_CCS_HTAREQ			0x00000010	/**< HT Avail Request */
#define	BHND_CCS_FORCEHWREQOFF		0x00000020	/**< Force HW Clock Request Off */
#define	BHND_CCS_ERSRC_REQ_MASK		0x00000700	/**< external resource requests */
#define	BHND_CCS_ERSRC_REQ_SHIFT	8
#define	BHND_CCS_ALPAVAIL		0x00010000	/**< ALP is available */
#define	BHND_CCS_HTAVAIL		0x00020000	/**< HT is available */
#define	BHND_CCS_BP_ON_APL		0x00040000	/**< RO: Backplane is running on ALP clock */
#define	BHND_CCS_BP_ON_HT		0x00080000	/**< RO: Backplane is running on HT clock */
#define	BHND_CCS_ERSRC_STS_MASK		0x07000000	/**< external resource status */
#define	BHND_CCS_ERSRC_STS_SHIFT	24

#define	BHND_CCS0_HTAVAIL		0x00010000	/**< HT avail in chipc and pcmcia on 4328a0 */
#define	BHND_CCS0_ALPAVAIL		0x00020000	/**< ALP avail in chipc and pcmcia on 4328a0 */

#endif /* _BHND_BHND_CORE_H_ */
