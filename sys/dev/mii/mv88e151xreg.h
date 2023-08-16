/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Rubicon Communications, LLC (Netgate).
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_MII_MV88E151XREG_H_
#define	_DEV_MII_MV88E151XREG_H_

#define	MV88E151X_FIBER_STATUS		17
#define	 MV88E151X_STATUS_SPEED_MASK	0xc000
#define	 MV88E151X_STATUS_SPEED_SHIFT	14
#define	 MV88E151X_STATUS_SPEED_1000	2
#define	 MV88E151X_STATUS_SPEED_100	1
#define	 MV88E151X_STATUS_SPEED_10	0
#define	 MV88E151X_STATUS_FDX		(1 << 13)
#define	 MV88E151X_STATUS_RESOLVED	(1 << 11)
#define	 MV88E151X_STATUS_LINK		(1 << 10)
#define	 MV88E151X_STATUS_SYNC		(1 << 5)
#define	 MV88E151X_STATUS_ENERGY	(1 << 4)
#define	MV88E151X_PAGE			22
#define	 MV88E151X_PAGE_COPPER		0
#define	 MV88E151X_PAGE_FIBER		1

#endif /* _DEV_MII_MV88E151XREG_H_ */
