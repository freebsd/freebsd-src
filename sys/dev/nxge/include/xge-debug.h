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
 * $FreeBSD$
 */

/*
 *  FileName :    xge-debug.h
 *
 *  Description:  debug facilities
 *
 *  Created:      6 May 2004
 */

#ifndef XGE_DEBUG_H
#define XGE_DEBUG_H

#include <dev/nxge/include/xge-os-pal.h>

__EXTERN_BEGIN_DECLS

/*
 * __FUNCTION__ is, together with __PRETTY_FUNCTION__ or something similar,
 * a gcc extension. we'll have to #ifdef around that, and provide some
 * meaningful replacement for those, so to make some gcc versions happier
 */
#ifndef __func__
#ifdef __FUNCTION__
#define __func__ __FUNCTION__
#endif
#endif


#ifdef XGE_DEBUG_FP
#define XGE_DEBUG_FP_DEVICE	0x1
#define XGE_DEBUG_FP_CHANNEL	0x2
#define XGE_DEBUG_FP_FIFO	0x4
#define XGE_DEBUG_FP_RING	0x8
#endif

/**
 * enum xge_debug_level_e
 * @XGE_NONE: debug disabled
 * @XGE_ERR: all errors going to be logged out
 * @XGE_TRACE: all errors plus all kind of verbose tracing print outs
 *                 going to be logged out. Very noisy.
 *
 * This enumeration going to be used to switch between different
 * debug levels during runtime if DEBUG macro defined during
 * compilation. If DEBUG macro not defined than code will be
 * compiled out.
 */
typedef enum xge_debug_level_e {
	XGE_NONE   = 0,
	XGE_TRACE  = 1,
	XGE_ERR    = 2,
} xge_debug_level_e;

#define XGE_DEBUG_MODULE_MASK_DEF	0x30000030
#define XGE_DEBUG_LEVEL_DEF		XGE_ERR

#if defined(XGE_DEBUG_TRACE_MASK) || defined(XGE_DEBUG_ERR_MASK)

extern unsigned long *g_module_mask;
extern int *g_level;

#ifndef XGE_DEBUG_TRACE_MASK
#define XGE_DEBUG_TRACE_MASK 0
#endif

#ifndef XGE_DEBUG_ERR_MASK
#define XGE_DEBUG_ERR_MASK 0
#endif

/*
 * @XGE_COMPONENT_HAL_CONFIG: do debug for xge core config module
 * @XGE_COMPONENT_HAL_FIFO: do debug for xge core fifo module
 * @XGE_COMPONENT_HAL_RING: do debug for xge core ring module
 * @XGE_COMPONENT_HAL_CHANNEL: do debug for xge core channel module
 * @XGE_COMPONENT_HAL_DEVICE: do debug for xge core device module
 * @XGE_COMPONENT_HAL_DMQ: do debug for xge core DMQ module
 * @XGE_COMPONENT_HAL_UMQ: do debug for xge core UMQ module
 * @XGE_COMPONENT_HAL_SQ: do debug for xge core SQ module
 * @XGE_COMPONENT_HAL_SRQ: do debug for xge core SRQ module
 * @XGE_COMPONENT_HAL_CQRQ: do debug for xge core CRQ module
 * @XGE_COMPONENT_HAL_POOL: do debug for xge core memory pool module
 * @XGE_COMPONENT_HAL_BITMAP: do debug for xge core BITMAP module
 * @XGE_COMPONENT_CORE: do debug for xge KMA core module
 * @XGE_COMPONENT_OSDEP: do debug for xge KMA os dependent parts
 * @XGE_COMPONENT_LL: do debug for xge link layer module
 * @XGE_COMPONENT_ALL: activate debug for all modules with no exceptions
 *
 * This enumeration going to be used to distinguish modules
 * or libraries during compilation and runtime.  Makefile must declare
 * XGE_DEBUG_MODULE_MASK macro and set it to proper value.
 */
#define XGE_COMPONENT_HAL_CONFIG		0x00000001
#define	XGE_COMPONENT_HAL_FIFO			0x00000002
#define	XGE_COMPONENT_HAL_RING			0x00000004
#define	XGE_COMPONENT_HAL_CHANNEL		0x00000008
#define	XGE_COMPONENT_HAL_DEVICE		0x00000010
#define	XGE_COMPONENT_HAL_MM			0x00000020
#define	XGE_COMPONENT_HAL_QUEUE	        0x00000040
#define	XGE_COMPONENT_HAL_INTERRUPT     0x00000080
#define	XGE_COMPONENT_HAL_STATS	        0x00000100
#ifdef XGEHAL_RNIC
#define	XGE_COMPONENT_HAL_DMQ	        0x00000200
#define	XGE_COMPONENT_HAL_UMQ	        0x00000400
#define	XGE_COMPONENT_HAL_SQ	        0x00000800
#define	XGE_COMPONENT_HAL_SRQ			0x00001000
#define	XGE_COMPONENT_HAL_CQRQ			0x00002000
#define	XGE_COMPONENT_HAL_POOL	        0x00004000
#define	XGE_COMPONENT_HAL_BITMAP		0x00008000
#endif

	/* space for CORE_XXX */
#define	XGE_COMPONENT_OSDEP			0x10000000
#define	XGE_COMPONENT_LL			0x20000000
#define	XGE_COMPONENT_ALL			0xffffffff

#ifndef XGE_DEBUG_MODULE_MASK
#error "XGE_DEBUG_MODULE_MASK macro must be defined for DEBUG mode..."
#endif

#ifndef __GNUC__
#ifdef XGE_TRACE_INTO_CIRCULAR_ARR
        #define xge_trace_aux(fmt) xge_os_vatrace(g_xge_os_tracebuf, fmt)
#else
        #define xge_trace_aux(fmt) xge_os_vaprintf(fmt)
#endif

/**
 * xge_debug
 * @level: level of debug verbosity.
 * @fmt: printf like format string
 *
 * Provides logging facilities. Can be customized on per-module
 * basis or/and with debug levels. Input parameters, except
 * module and level, are the same as posix printf. This function
 * may be compiled out if DEBUG macro was never defined.
 * See also: xge_debug_level_e{}.
 */
#define xge_debug(module, level, fmt) { \
if (((level >= XGE_TRACE && ((module & XGE_DEBUG_TRACE_MASK) == module)) || \
    (level >= XGE_ERR && ((module & XGE_DEBUG_ERR_MASK) == module))) && \
    level >= *g_level && module & *(unsigned int *)g_module_mask) { \
                xge_trace_aux(fmt); \
	} \
}
#else /* __GNUC__ */

#ifdef XGE_TRACE_INTO_CIRCULAR_ARR
        #define xge_trace_aux(fmt...) xge_os_trace(g_xge_os_tracebuf, fmt)
#else
        #define xge_trace_aux(fmt...) xge_os_printf(fmt)
#endif

#define xge_debug(module, level, fmt...) { \
if (((level >= XGE_TRACE && ((module & XGE_DEBUG_TRACE_MASK) == module)) || \
    (level >= XGE_ERR && ((module & XGE_DEBUG_ERR_MASK) == module))) && \
    level >= *g_level && module & *(unsigned int *)g_module_mask) { \
                xge_trace_aux(fmt); \
	} \
}
#endif /* __GNUC__ */

#if (XGE_COMPONENT_HAL_STATS & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_stats(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_HAL_STATS;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_stats(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_STATS, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_stats(xge_debug_level_e level, char *fmt, ...) {}
#else /* __GNUC__ */
#define xge_debug_stats(level, fmt...)
#endif /* __GNUC__ */
#endif

/* Interrupt Related */
#if (XGE_COMPONENT_HAL_INTERRUPT & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_interrupt(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_HAL_INTERRUPT;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_interrupt(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_INTERRUPT, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_interrupt(xge_debug_level_e level, char *fmt, ...) {}
#else /* __GNUC__ */
#define xge_debug_interrupt(level, fmt...)
#endif /* __GNUC__ */
#endif

#if (XGE_COMPONENT_HAL_QUEUE & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_queue(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_HAL_QUEUE;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_queue(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_QUEUE, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_queue(xge_debug_level_e level, char *fmt,
...) {}
#else /* __GNUC__ */
#define xge_debug_queue(level, fmt...)
#endif /* __GNUC__ */
#endif

#if (XGE_COMPONENT_HAL_MM & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_mm(xge_debug_level_e level, char *fmt, ...)
{
	u32 module = XGE_COMPONENT_HAL_MM;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_mm(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_MM, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_mm(xge_debug_level_e level, char *fmt, ...)
{}
#else /* __GNUC__ */
#define xge_debug_mm(level, fmt...)
#endif /* __GNUC__ */
#endif

#if (XGE_COMPONENT_HAL_CONFIG & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_config(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_HAL_CONFIG;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_config(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_CONFIG, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_config(xge_debug_level_e level, char *fmt,
...) {}
#else /* __GNUC__ */
#define xge_debug_config(level, fmt...)
#endif /* __GNUC__ */
#endif

#if (XGE_COMPONENT_HAL_FIFO & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_fifo(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_HAL_FIFO;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_fifo(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_FIFO, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_fifo(xge_debug_level_e level, char *fmt, ...) {}
#else /* __GNUC__ */
#define xge_debug_fifo(level, fmt...)
#endif /* __GNUC__ */
#endif

#if (XGE_COMPONENT_HAL_RING & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_ring(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_HAL_RING;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_ring(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_RING, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_ring(xge_debug_level_e level, char *fmt, ...) {}
#else /* __GNUC__ */
#define xge_debug_ring(level, fmt...)
#endif /* __GNUC__ */
#endif

#if (XGE_COMPONENT_HAL_CHANNEL & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_channel(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_HAL_CHANNEL;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_channel(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_CHANNEL, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_channel(xge_debug_level_e level, char *fmt, ...) {}
#else /* __GNUC__ */
#define xge_debug_channel(level, fmt...)
#endif /* __GNUC__ */
#endif

#if (XGE_COMPONENT_HAL_DEVICE & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_device(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_HAL_DEVICE;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_device(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_DEVICE, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_device(xge_debug_level_e level, char *fmt, ...) {}
#else /* __GNUC__ */
#define xge_debug_device(level, fmt...)
#endif /* __GNUC__ */
#endif

#ifdef XGEHAL_RNIC

#if (XGE_COMPONENT_HAL_DMQ & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_dmq(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_HAL_DMQ;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_dmq(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_DMQ, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_dmq(xge_debug_level_e level, char *fmt, ...) {}
#else /* __GNUC__ */
#define xge_debug_dmq(level, fmt...)
#endif /* __GNUC__ */
#endif

#if (XGE_COMPONENT_HAL_UMQ & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_umq(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_HAL_UMQ;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_umq(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_UMQ, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_umq(xge_debug_level_e level, char *fmt, ...) {}
#else /* __GNUC__ */
#define xge_debug_umq(level, fmt...)
#endif /* __GNUC__ */
#endif

#if (XGE_COMPONENT_HAL_SQ & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_sq(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_HAL_SQ;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_sq(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_SQ, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_sq(xge_debug_level_e level, char *fmt, ...) {}
#else /* __GNUC__ */
#define xge_debug_sq(level, fmt...)
#endif /* __GNUC__ */
#endif

#if (XGE_COMPONENT_HAL_SRQ & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_srq(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_HAL_SRQ;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_srq(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_SRQ, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_srq(xge_debug_level_e level, char *fmt, ...) {}
#else /* __GNUC__ */
#define xge_debug_srq(level, fmt...)
#endif /* __GNUC__ */
#endif

#if (XGE_COMPONENT_HAL_CQRQ & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_cqrq(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_HAL_CQRQ;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_cqrq(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_CQRQ, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_cqrq(xge_debug_level_e level, char *fmt, ...) {}
#else /* __GNUC__ */
#define xge_debug_cqrq(level, fmt...)
#endif /* __GNUC__ */
#endif

#if (XGE_COMPONENT_HAL_POOL & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_pool(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_HAL_POOL;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_pool(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_POOL, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_pool(xge_debug_level_e level, char *fmt, ...) {}
#else /* __GNUC__ */
#define xge_debug_pool(level, fmt...)
#endif /* __GNUC__ */
#endif

#if (XGE_COMPONENT_HAL_BITMAP & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_bitmap(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_HAL_BITMAP;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_bitmap(level, fmt...) \
	xge_debug(XGE_COMPONENT_HAL_BITMAP, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_bitmap(xge_debug_level_e level, char *fmt, ...) {}
#else /* __GNUC__ */
#define xge_debug_bitmap(level, fmt...)
#endif /* __GNUC__ */
#endif

#endif

#if (XGE_COMPONENT_OSDEP & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_osdep(xge_debug_level_e level, char *fmt, ...) {
	u32 module = XGE_COMPONENT_OSDEP;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_osdep(level, fmt...) \
	xge_debug(XGE_COMPONENT_OSDEP, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_osdep(xge_debug_level_e level, char *fmt, ...) {}
#else /* __GNUC__ */
#define xge_debug_osdep(level, fmt...)
#endif /* __GNUC__ */
#endif

#if (XGE_COMPONENT_LL & XGE_DEBUG_MODULE_MASK)
#ifndef __GNUC__
static inline void xge_debug_ll(xge_debug_level_e level, char *fmt, ...)
{
	u32 module = XGE_COMPONENT_LL;
	xge_debug(module, level, fmt);
}
#else /* __GNUC__ */
#define xge_debug_ll(level, fmt...) \
	xge_debug(XGE_COMPONENT_LL, level, fmt)
#endif /* __GNUC__ */
#else
#ifndef __GNUC__
static inline void xge_debug_ll(xge_debug_level_e level, char *fmt, ...) {}
#else /* __GNUC__ */
#define xge_debug_ll(level, fmt...)
#endif /* __GNUC__ */
#endif

#else

static inline void xge_debug_interrupt(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_stats(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_queue(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_mm(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_config(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_fifo(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_ring(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_channel(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_device(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_dmq(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_umq(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_sq(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_srq(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_cqrq(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_pool(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_bitmap(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_hal(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_osdep(xge_debug_level_e level, char *fmt, ...) {}
static inline void xge_debug_ll(xge_debug_level_e level, char *fmt, ...) {}

#endif /* end of XGE_DEBUG_*_MASK */

#ifdef XGE_DEBUG_ASSERT

/**
 * xge_assert
 * @test: C-condition to check
 * @fmt: printf like format string
 *
 * This function implements traditional assert. By default assertions
 * are enabled. It can be disabled by defining XGE_DEBUG_ASSERT macro in
 * compilation
 * time.
 */
#define xge_assert(test) { \
        if (!(test)) xge_os_bug("bad cond: "#test" at %s:%d\n", \
	__FILE__, __LINE__); }
#else
#define xge_assert(test)
#endif /* end of XGE_DEBUG_ASSERT */

__EXTERN_END_DECLS

#endif /* XGE_DEBUG_H */
