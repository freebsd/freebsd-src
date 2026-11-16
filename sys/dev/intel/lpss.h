/*
 * Copyright (c) 2026 Abdelkader Boudih <seuros@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _DEV_INTEL_LPSS_H_
#define	_DEV_INTEL_LPSS_H_

/*
 * Intel LPSS private registers, shared by the I2C, SPI and UART functions.
 * Offsets are relative to the private block, which sits at one of these
 * bases in the function's first memory BAR.
 */
#define	LPSS_PRIV_LPT		0x800	/* Lynx Point, Atom SoCs */
#define	LPSS_PRIV_BYT		0x400	/* Bay Trail, Braswell */
#define	LPSS_PRIV_SPT		0x200	/* Sunrise Point and later */

/* CLK - functional clock gate and M/N divider; rate is reference * M / N. */
#define	LPSS_PRIV_CLK		0x00
#define	 LPSS_PRIV_CLK_EN		(1U << 0)
#define	 LPSS_PRIV_CLK_M_SHIFT		1
#define	 LPSS_PRIV_CLK_N_SHIFT		16
#define	 LPSS_PRIV_CLK_MN_MASK		0x7fff
#define	 LPSS_PRIV_CLK_UPDATE		(1U << 31)	/* latch M and N */
#define	 LPSS_PRIV_CLK_MN(m, n)						\
	((((n) & LPSS_PRIV_CLK_MN_MASK) << LPSS_PRIV_CLK_N_SHIFT) |	\
	 (((m) & LPSS_PRIV_CLK_MN_MASK) << LPSS_PRIV_CLK_M_SHIFT))

/* RESETS - function and DMA resets; polarity is inverted from SPT onwards. */
#define	LPSS_PRIV_RESETS	0x04
#define	 LPSS_PRIV_RESETS_FUNC		0x3
#define	 LPSS_PRIV_RESETS_IDMA		(1U << 2)

#endif /* _DEV_INTEL_LPSS_H_ */
