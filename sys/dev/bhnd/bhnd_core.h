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

#endif /* _BHND_BHND_CORE_H_ */
