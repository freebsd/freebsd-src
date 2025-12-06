/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Adrian Chadd <adrian@FreeBSD.org>
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
 */
#ifndef	__IF_RGE_DEBUG_H__
#define	__IF_RGE_DEBUG_H__

#define	RGE_DEBUG_XMIT		0x00000001
#define	RGE_DEBUG_RECV		0x00000002
#define	RGE_DEBUG_INTR		0x00000004
#define	RGE_DEBUG_SETUP		0x00000008
#define	RGE_DEBUG_INIT		0x00000010
#define	RGE_DEBUG_XMIT_DESC	0x00000020
#define	RGE_DEBUG_RECV_DESC	0x00000040

#define	RGE_DPRINTF(sc, dbg, ...)					\
	do {								\
		if (((sc)->sc_debug & (dbg)) != 0)			\
			device_printf((sc)->sc_dev, __VA_ARGS__);	\
	} while (0)

#define	RGE_DLOG(sc, dbg, ...)						\
	do {								\
		if (((sc)->sc_debug & (dbg)) != 0)			\
			device_log((sc)->sc_dev, LOG_DEBUG,		\
			    __VA_ARGS__);				\
	} while (0)

#define	RGE_PRINT_ERROR(sc, ...)					\
	do {								\
		device_printf((sc)->sc_dev, "[ERR] " __VA_ARGS__);	\
	} while (0)

#define	RGE_PRINT_INFO(sc, ...)						\
	do {								\
		device_printf((sc)->sc_dev, "[INFO] " __VA_ARGS__);	\
	} while (0)

#define	RGE_PRINT_TODO(sc, ...)						\
	do {								\
		device_printf((sc)->sc_dev, "[TODO] " __VA_ARGS__);	\
	} while (0)


#define	RGE_PRINT_WARN(sc, ...)						\
	do {								\
		device_printf((sc)->sc_dev, "[WARN] " __VA_ARGS__);	\
	} while (0)

#endif	/* __IF_RGE_DEBUG_H__ */
