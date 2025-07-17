/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

/*
 * Texas Instruments DP83867IR/CR Robust, High Immunity
 * 10/100/1000 Ethernet Physical Layer Transceiver.
 */

#ifndef _DEV_MII_TIPHY_H_
#define _DEV_MII_TIPHY_H_

#define	DP83867_PHYCR			0x10	/* PHY Control Register */
#define	 PHYCR_SGMII_EN			(1 << 11)
#define	DP83867_CFG2			0x14	/* Configuration Register 2 */
#define	 CFG2_SPEED_OPT_10M_EN		(1 << 6) /* Speed Optimization */
#define	 CFG2_SPEED_OPT_ENHANCED_EN	(1 << 8)
#define	 CFG2_SPEED_OPT_ATTEMPT_CNT_S	10
#define	 CFG2_SPEED_OPT_ATTEMPT_CNT_M	(0x3 << CFG2_SPEED_OPT_ATTEMPT_CNT_S)
#define	 CFG2_SPEED_OPT_ATTEMPT_CNT_1	(0 << CFG2_SPEED_OPT_ATTEMPT_CNT_S)
#define	 CFG2_SPEED_OPT_ATTEMPT_CNT_2	(1 << CFG2_SPEED_OPT_ATTEMPT_CNT_S)
#define	 CFG2_SPEED_OPT_ATTEMPT_CNT_4	(2 << CFG2_SPEED_OPT_ATTEMPT_CNT_S)
#define	 CFG2_SPEED_OPT_ATTEMPT_CNT_8	(3 << CFG2_SPEED_OPT_ATTEMPT_CNT_S)
#define	 CFG2_INTERRUPT_POLARITY	(1 << 13) /* Int pin is active low. */
#define	DP83867_CFG4			0x31 /* Configuration Register 4 */

#endif /* !_DEV_MII_TIPHY_H_ */
