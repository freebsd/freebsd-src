/******************************************************************************
 * memmap.h
 *
 * Copyright (c) 2008 Tristan Gingold <tgingold AT free fr>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __XEN_PUBLIC_HVM_MEMMAP_IA64_H__
#define __XEN_PUBLIC_HVM_MEMMAP_IA64_H__

#define MEM_G  (1UL << 30)
#define MEM_M  (1UL << 20)
#define MEM_K  (1UL << 10)

/* Guest physical address of IO ports space.  */
#define MMIO_START  (3 * MEM_G)
#define MMIO_SIZE   (512 * MEM_M)

#define VGA_IO_START  0xA0000UL
#define VGA_IO_SIZE   0x20000

#define LEGACY_IO_START  (MMIO_START + MMIO_SIZE)
#define LEGACY_IO_SIZE   (64 * MEM_M)

#define IO_PAGE_START  (LEGACY_IO_START + LEGACY_IO_SIZE)
#define IO_PAGE_SIZE   XEN_PAGE_SIZE

#define STORE_PAGE_START  (IO_PAGE_START + IO_PAGE_SIZE)
#define STORE_PAGE_SIZE   XEN_PAGE_SIZE

#define BUFFER_IO_PAGE_START  (STORE_PAGE_START + STORE_PAGE_SIZE)
#define BUFFER_IO_PAGE_SIZE   XEN_PAGE_SIZE

#define BUFFER_PIO_PAGE_START  (BUFFER_IO_PAGE_START + BUFFER_IO_PAGE_SIZE)
#define BUFFER_PIO_PAGE_SIZE   XEN_PAGE_SIZE

#define IO_SAPIC_START  0xfec00000UL
#define IO_SAPIC_SIZE   0x100000

#define PIB_START  0xfee00000UL
#define PIB_SIZE   0x200000

#define GFW_START  (4 * MEM_G - 16 * MEM_M)
#define GFW_SIZE   (16 * MEM_M)

/* domVTI */
#define GPFN_FRAME_BUFFER  0x1 /* VGA framebuffer */
#define GPFN_LOW_MMIO      0x2 /* Low MMIO range */
#define GPFN_PIB           0x3 /* PIB base */
#define GPFN_IOSAPIC       0x4 /* IOSAPIC base */
#define GPFN_LEGACY_IO     0x5 /* Legacy I/O base */
#define GPFN_HIGH_MMIO     0x6 /* High MMIO range */

/* Nvram belongs to GFW memory space  */
#define NVRAM_SIZE   (MEM_K * 64)
#define NVRAM_START  (GFW_START + 10 * MEM_M)

#define NVRAM_VALID_SIG  0x4650494e45584948 /* "HIXENIPF" */
struct nvram_save_addr {
    unsigned long addr;
    unsigned long signature;
};

#endif /* __XEN_PUBLIC_HVM_MEMMAP_IA64_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
