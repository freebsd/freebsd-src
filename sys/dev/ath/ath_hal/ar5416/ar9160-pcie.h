/*
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2008 Atheros Communications, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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
 * $FreeBSD$
 */

/* hand-crafted from code that does explicit register writes */
static const uint32_t ar9160PciePhy[][2] = {
    { AR_PCIE_SERDES,  0x9248fc00 },
    { AR_PCIE_SERDES,  0x24924924 },
    { AR_PCIE_SERDES,  0x28000039 },
    { AR_PCIE_SERDES,  0x53160824 },
    { AR_PCIE_SERDES,  0xe5980579 },
    { AR_PCIE_SERDES,  0x001defff },
    { AR_PCIE_SERDES,  0x1aaabe40 },
    { AR_PCIE_SERDES,  0xbe105554 },
    { AR_PCIE_SERDES,  0x000e3007 },
    { AR_PCIE_SERDES2, 0x00000000 },
};
