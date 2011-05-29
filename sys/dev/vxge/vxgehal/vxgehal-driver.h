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

#ifndef	VXGE_HAL_DRIVER_H
#define	VXGE_HAL_DRIVER_H

__EXTERN_BEGIN_DECLS

/* maximum number of events consumed in a syncle poll() cycle */
#define	VXGE_HAL_DRIVER_QUEUE_CONSUME_MAX	5

/*
 * struct __hal_driver_t - Represents HAL object for driver.
 * @config: HAL configuration.
 * @devices: List of all PCI-enumerated X3100 devices in the system.
 * A single vxge_hal_driver_t instance contains zero or more
 * X3100 devices.
 * @devices_lock: Lock to protect %devices when inserting/removing.
 * @is_initialized: True if HAL is initialized; false otherwise.
 * @uld_callbacks: Upper-layer driver callbacks. See vxge_hal_uld_cbs_t {}.
 * @debug_module_mask: 32bit mask that defines which components of the
 * driver are to be traced. The trace-able components are listed in
 * xgehal_debug.h:
 * @debug_level: See vxge_debug_level_e {}.
 *
 * HAL (driver) object. There is a single instance of this structure per HAL.
 */
typedef struct __hal_driver_t {
	vxge_hal_driver_config_t	config;
	int				is_initialized;
	vxge_hal_uld_cbs_t		uld_callbacks;
	u32				debug_level;
} __hal_driver_t;

extern __hal_driver_t *g_vxge_hal_driver;

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_DRIVER_H */
