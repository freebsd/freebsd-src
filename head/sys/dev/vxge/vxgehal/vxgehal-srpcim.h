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

#ifndef	VXGE_HAL_SRPCIM_H
#define	VXGE_HAL_SRPCIM_H

__EXTERN_BEGIN_DECLS

/*
 * __hal_srpcim_vpath_t
 *
 * HAL srpcim vpath messaging state.
 */
typedef struct __hal_srpcim_vpath_t {
	u32	registered;
	u32	srpcim_id;
} __hal_srpcim_vpath_t;

/*
 * __hal_srpcim_t
 *
 * HAL srpcim object. Represents privileged mode srpcim device.
 */
typedef struct __hal_srpcim_t {
	__hal_srpcim_vpath_t vpath_state[VXGE_HAL_MAX_VIRTUAL_PATHS];
} __hal_srpcim_t;


vxge_hal_status_e
__hal_srpcim_alarm_process(
    __hal_device_t *hldev,
    u32 srpcim_id,
    u32 skip_alarms);

vxge_hal_status_e
__hal_srpcim_intr_enable(
    __hal_device_t *hldev,
    u32 srpcim_id);

vxge_hal_status_e
__hal_srpcim_intr_disable(
    __hal_device_t *hldev,
    u32 srpcim_id);

vxge_hal_status_e
__hal_srpcim_initialize(
    __hal_device_t *hldev);

vxge_hal_status_e
__hal_srpcim_terminate(
    __hal_device_t *hldev);

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_SRPCIM_H */
