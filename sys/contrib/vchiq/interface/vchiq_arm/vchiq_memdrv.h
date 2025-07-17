/**
 * Copyright (c) 2010-2012 Broadcom. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the above-listed copyright holders may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2, as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VCHIQ_MEMDRV_H
#define VCHIQ_MEMDRV_H

/* ---- Include Files ----------------------------------------------------- */

#include <linux/kernel.h>
#include "vchiq_if.h"

/* ---- Constants and Types ---------------------------------------------- */

typedef struct {
	 void                   *armSharedMemVirt;
	 dma_addr_t              armSharedMemPhys;
	 size_t                  armSharedMemSize;

	 void                   *vcSharedMemVirt;
	 dma_addr_t              vcSharedMemPhys;
	 size_t                  vcSharedMemSize;
} VCHIQ_SHARED_MEM_INFO_T;

/* ---- Variable Externs ------------------------------------------------- */

/* ---- Function Prototypes ---------------------------------------------- */

void vchiq_get_shared_mem_info(VCHIQ_SHARED_MEM_INFO_T *info);

VCHIQ_STATUS_T vchiq_memdrv_initialise(void);

VCHIQ_STATUS_T vchiq_userdrv_create_instance(
	const VCHIQ_PLATFORM_DATA_T * platform_data);

VCHIQ_STATUS_T vchiq_userdrv_suspend(
	const VCHIQ_PLATFORM_DATA_T * platform_data);

VCHIQ_STATUS_T vchiq_userdrv_resume(
	const VCHIQ_PLATFORM_DATA_T * platform_data);

#endif
