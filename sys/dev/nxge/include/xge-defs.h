/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
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
 *
 * $FreeBSD: src/sys/dev/nxge/include/xge-defs.h,v 1.1.2.1 2007/11/02 00:52:32 rwatson Exp $
 */

#ifndef XGE_DEFS_H
#define XGE_DEFS_H

#define XGE_PCI_VENDOR_ID           0x17D5
#define XGE_PCI_DEVICE_ID_XENA_1    0x5731
#define XGE_PCI_DEVICE_ID_XENA_2    0x5831
#define XGE_PCI_DEVICE_ID_HERC_1    0x5732
#define XGE_PCI_DEVICE_ID_HERC_2    0x5832
#define XGE_PCI_DEVICE_ID_TITAN_1   0x5733
#define XGE_PCI_DEVICE_ID_TITAN_2   0x5833

#define XGE_DRIVER_NAME             "Xge driver"
#define XGE_DRIVER_VENDOR           "Neterion, Inc"
#define XGE_CHIP_FAMILY             "Xframe"
#define XGE_SUPPORTED_MEDIA_0       "Fiber"

#include <dev/nxge/include/version.h>

#if defined(__cplusplus)
#define __EXTERN_BEGIN_DECLS    extern "C" {
#define __EXTERN_END_DECLS  }
#else
#define __EXTERN_BEGIN_DECLS
#define __EXTERN_END_DECLS
#endif

__EXTERN_BEGIN_DECLS

/*---------------------------- DMA attributes ------------------------------*/
/*           Used in xge_os_dma_malloc() and xge_os_dma_map() */
/*---------------------------- DMA attributes ------------------------------*/

/* XGE_OS_DMA_REQUIRES_SYNC  - should be defined or
	                         NOT defined in the Makefile */
#define XGE_OS_DMA_CACHELINE_ALIGNED      0x1
/* Either STREAMING or CONSISTENT should be used.
   The combination of both or none is invalid */
#define XGE_OS_DMA_STREAMING              0x2
#define XGE_OS_DMA_CONSISTENT             0x4
#define XGE_OS_SPRINTF_STRLEN             64

/*---------------------------- common stuffs -------------------------------*/

#define XGE_OS_LLXFMT       "%llx"
#define XGE_OS_NEWLINE      "\n"
#ifdef XGE_OS_MEMORY_CHECK
typedef struct {
	void *ptr;
	int size;
	char *file;
	int line;
} xge_os_malloc_t;

#define XGE_OS_MALLOC_CNT_MAX   64*1024
extern xge_os_malloc_t g_malloc_arr[XGE_OS_MALLOC_CNT_MAX];
extern int g_malloc_cnt;

#define XGE_OS_MEMORY_CHECK_MALLOC(_vaddr, _size, _file, _line) { \
	if (_vaddr) { \
	    int index_mem_chk; \
	    for (index_mem_chk=0; index_mem_chk < g_malloc_cnt; index_mem_chk++) { \
	        if (g_malloc_arr[index_mem_chk].ptr == NULL) { \
	            break; \
	        } \
	    } \
	    if (index_mem_chk == g_malloc_cnt) { \
	        g_malloc_cnt++; \
	        if (g_malloc_cnt >= XGE_OS_MALLOC_CNT_MAX) { \
	          xge_os_bug("g_malloc_cnt exceed %d", \
	                    XGE_OS_MALLOC_CNT_MAX); \
	        } \
	    } \
	    g_malloc_arr[index_mem_chk].ptr = _vaddr; \
	    g_malloc_arr[index_mem_chk].size = _size; \
	    g_malloc_arr[index_mem_chk].file = _file; \
	    g_malloc_arr[index_mem_chk].line = _line; \
	    for (index_mem_chk=0; index_mem_chk<_size; index_mem_chk++) { \
	        *((char *)_vaddr+index_mem_chk) = 0x5a; \
	    } \
	} \
}

#define XGE_OS_MEMORY_CHECK_FREE(_vaddr, _check_size) { \
	int index_mem_chk; \
	for (index_mem_chk=0; index_mem_chk < XGE_OS_MALLOC_CNT_MAX; index_mem_chk++) { \
	    if (g_malloc_arr[index_mem_chk].ptr == _vaddr) { \
	        g_malloc_arr[index_mem_chk].ptr = NULL; \
	        if(_check_size && g_malloc_arr[index_mem_chk].size!=_check_size) { \
	            xge_os_printf("OSPAL: freeing with wrong " \
	                  "size %d! allocated at %s:%d:"XGE_OS_LLXFMT":%d", \
	                 (int)_check_size, \
	                 g_malloc_arr[index_mem_chk].file, \
	                 g_malloc_arr[index_mem_chk].line, \
	                 (unsigned long long)(ulong_t) \
	                    g_malloc_arr[index_mem_chk].ptr, \
	                 g_malloc_arr[index_mem_chk].size); \
	        } \
	        break; \
	    } \
	} \
	if (index_mem_chk == XGE_OS_MALLOC_CNT_MAX) { \
	    xge_os_printf("OSPAL: ptr "XGE_OS_LLXFMT" not found!", \
	            (unsigned long long)(ulong_t)_vaddr); \
	} \
}
#else
#define XGE_OS_MEMORY_CHECK_MALLOC(ptr, size, file, line)
#define XGE_OS_MEMORY_CHECK_FREE(vaddr, check_size)
#endif

__EXTERN_END_DECLS

#endif /* XGE_DEFS_H */
