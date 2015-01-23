/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef	VXGE_OS_DEBUG_H
#define	VXGE_OS_DEBUG_H

__EXTERN_BEGIN_DECLS

#ifndef	VXGE_DEBUG_INLINE_FUNCTIONS

#if defined(VXGE_TRACE_INTO_CIRCULAR_ARR)
#define	vxge_trace_aux(hldev, vpid, fmt, ...)				\
		vxge_os_vasprintf(hldev, vpid, fmt, __VA_ARGS__)
#else
#define	vxge_trace_aux(hldev, vpid, fmt, ...)				\
		vxge_os_vaprintf(hldev, vpid, fmt, __VA_ARGS__)
#endif

#define	vxge_debug(module, level, hldev, vpid, fmt, ...)		\
{									\
	if (((u32)level <=						\
		((vxge_hal_device_t *)hldev)->debug_level) &&		\
	    ((u32)module &						\
		((vxge_hal_device_t *)hldev)->debug_module_mask))	\
			vxge_trace_aux((vxge_hal_device_h)hldev,	\
					vpid, fmt, __VA_ARGS__);	\
}

/*
 * vxge_debug_driver
 * @level: level of debug verbosity.
 * @hldev: HAL Device
 * @vpid: Vpath id
 * @fmt: printf like format string
 *
 * Provides logging facilities for OS Dependent functions. Can be customized
 * with debug levels. Input parameters, except level, are the same
 * as posix printf. This function may be compiled out if DEBUG macro
 * was never defined.
 * See also: vxge_debug_level_e{}.
 */
#if (VXGE_COMPONENT_HAL_DRIVER & VXGE_DEBUG_MODULE_MASK)
#define	vxge_debug_driver(level, hldev, vpid, fmt, ...)		    \
	if ((u32)level <= g_debug_level)			    \
		vxge_os_vaprintf((vxge_hal_device_h)hldev,	    \
				vpid, fmt, __VA_ARGS__);
#else
#define	vxge_debug_driver(level, hldev, vpid, fmt, ...)
#endif

/*
 * vxge_debug_osdep
 * @level: level of debug verbosity.
 * @hldev: HAL Device
 * @vpid: Vpath id
 * @fmt: printf like format string
 *
 * Provides logging facilities for OS Dependent functions. Can be customized
 * with debug levels. Input parameters, except level, are the same
 * as posix printf. This function may be compiled out if DEBUG macro
 * was never defined.
 * See also: vxge_debug_level_e{}.
 */
#if (VXGE_COMPONENT_OSDEP & VXGE_DEBUG_MODULE_MASK)
#define	vxge_debug_osdep(level, hldev, vpid, fmt, ...) \
	vxge_debug(VXGE_COMPONENT_OSDEP, level, hldev, vpid, fmt, __VA_ARGS__)
#else
#define	vxge_debug_osdep(level, hldev, vpid, fmt, ...)
#endif

/*
 * vxge_debug_ll
 * @level: level of debug verbosity.
 * @hldev: HAL Device
 * @vpid: Vpath id
 * @fmt: printf like format string
 *
 * Provides logging facilities for LL driver. Can be customized
 * with debug levels. Input parameters, except level, are the same
 * as posix printf. This function may be compiled out if DEBUG macro
 * was never defined.
 * See also: vxge_debug_level_e{}.
 */
#if (VXGE_COMPONENT_LL & VXGE_DEBUG_MODULE_MASK)
#define	vxge_debug_ll(level, hldev, vpid, fmt, ...) \
	vxge_debug(VXGE_COMPONENT_LL, level, hldev, vpid, fmt, __VA_ARGS__)
#else
#define	vxge_debug_ll(level, hldev, vpid, fmt, ...)
#endif

/*
 * vxge_debug_uld
 * @component: The Component mask
 * @level: level of debug verbosity.
 * @hldev: HAL Device
 * @vpid: Vpath id
 * @fmt: printf like format string
 *
 * Provides logging facilities for LL driver. Can be customized
 * with debug levels. Input parameters, except level, are the same
 * as posix printf. This function may be compiled out if DEBUG macro
 * was never defined.
 * See also: vxge_debug_level_e{}.
 */
#if (VXGE_COMPONENT_ULD & VXGE_DEBUG_MODULE_MASK)
#define	vxge_debug_uld(component, level, hldev, vpid, fmt, ...) \
	vxge_debug(component, level, hldev, vpid, fmt, __VA_ARGS__)
#else
#define	vxge_debug_uld(level, hldev, vpid, fmt, ...)
#endif

#else	/* VXGE_DEBUG_INLINE_FUNCTIONS */

#if defined(VXGE_TRACE_INTO_CIRCULAR_ARR)
#define	vxge_trace_aux(hldev, vpid, fmt)				\
		vxge_os_vasprintf(hldev, vpid, fmt)
#else
#define	vxge_trace_aux(hldev, vpid, fmt)				\
		vxge_os_vaprintf(hldev, vpid, fmt)
#endif

#define	vxge_debug(module, level, hldev, vpid, fmt)			    \
{									    \
	if (((u32)level <= ((vxge_hal_device_t *)hldev)->debug_level) &&    \
	    ((u32)module & ((vxge_hal_device_t *)hldev)->debug_module_mask))\
		vxge_trace_aux((vxge_hal_device_h)hldev, vpid, fmt);	    \
}

/*
 * vxge_debug_driver
 * @level: level of debug verbosity.
 * @hldev: HAL Device
 * @vpid: Vpath id
 * @fmt: printf like format string
 *
 * Provides logging facilities for OS Dependent functions. Can be customized
 * with debug levels. Input parameters, except level, are the same
 * as posix printf. This function may be compiled out if DEBUG macro
 * was never defined.
 * See also: vxge_debug_level_e{}.
 */
static inline void vxge_debug_driver(
    vxge_debug_level_e level,
    vxge_hal_device_h hldev,
    u32 vpid,
    char *fmt, ...)
{
#if (VXGE_COMPONENT_HAL_DRIVER & VXGE_DEBUG_MODULE_MASK)
	if ((u32) level <= g_debug_level)
		vxge_os_vaprintf((vxge_hal_device_h) hldev, vpid, fmt);
#endif
}

/*
 * vxge_debug_osdep
 * @level: level of debug verbosity.
 * @hldev: HAL Device
 * @vpid: Vpath id
 * @fmt: printf like format string
 *
 * Provides logging facilities for OS Dependent functions. Can be customized
 * with debug levels. Input parameters, except level, are the same
 * as posix printf. This function may be compiled out if DEBUG macro
 * was never defined.
 * See also: vxge_debug_level_e{}.
 */
static inline void vxge_debug_osdep(
    vxge_debug_level_e level,
    vxge_hal_device_h hldev,
    u32 vpid,
    char *fmt, ...)
{
#if (VXGE_COMPONENT_OSDEP & VXGE_DEBUG_MODULE_MASK)
	vxge_debug(VXGE_COMPONENT_OSDEP, level, hldev, vpid, fmt)
#endif
}

/*
 * vxge_debug_ll
 * @level: level of debug verbosity.
 * @hldev: HAL Device
 * @vpid: Vpath id
 * @fmt: printf like format string
 *
 * Provides logging facilities for LL driver. Can be customized
 * with debug levels. Input parameters, except level, are the same
 * as posix printf. This function may be compiled out if DEBUG macro
 * was never defined.
 * See also: vxge_debug_level_e{}.
 */
static inline void vxge_debug_ll(
    vxge_debug_level_e level,
    vxge_hal_device_h hldev,
    u32 vpid,
    char *fmt, ...)
{
#if (VXGE_COMPONENT_LL & VXGE_DEBUG_MODULE_MASK)
	vxge_debug(VXGE_COMPONENT_LL, level, hldev, vpid, fmt)
#endif
}

/*
 * vxge_debug_uld
 * @component: The Component mask
 * @level: level of debug verbosity.
 * @hldev: HAL Device
 * @vpid: Vpath id
 * @fmt: printf like format string
 *
 * Provides logging facilities for LL driver. Can be customized
 * with debug levels. Input parameters, except level, are the same
 * as posix printf. This function may be compiled out if DEBUG macro
 * was never defined.
 * See also: vxge_debug_level_e{}.
 */
static inline void vxge_debug_uld(
    u32 component,
    vxge_debug_level_e level,
    vxge_hal_device_h hldev,
    u32 vpid,
    char *fmt, ...)
{
#if (VXGE_COMPONENT_ULD & VXGE_DEBUG_MODULE_MASK)
	vxge_debug(component, level, hldev, vpid, fmt)
#endif
}

#endif	/* end of VXGE_DEBUG_INLINE_FUNCTIONS */

__EXTERN_END_DECLS

#endif	/* VXGE_OS_DEBUG_H */
