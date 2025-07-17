/*-
 * SPDX-License-Identifier: BSD-2-Clause OR GPL-2.0
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _SCI_CONTROLLER_H_
#define _SCI_CONTROLLER_H_

/**
 * @file
 *
 * @brief This file contains all of the interface methods that can be called
 *        by an SCI user on all SCI controller objects.
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <dev/isci/scil/sci_types.h>


#define SCI_CONTROLLER_INVALID_IO_TAG 0xFFFF

typedef enum _SCI_CONTROLLER_ERROR {
    SCI_CONTROLLER_FATAL_ERROR = 1,
    SCI_CONTROLLER_REMOTE_DEVICE_ERROR,
    SCI_CONTROLLER_FATAL_MEMORY_ERROR
} SCI_CONTROLLER_ERROR;

/**
 * @brief This method simply returns a handle for the memory descriptor
 *        list associated with the supplied controller.  The descriptor list
 *        provides DMA safe/capable memory requirements for this controller.
 *
 * @warning The user must adhere to the alignment requirements specified in
 *          memory descriptor.  In situations where the operating environment
 *          does not offer memory allocation utilities supporting alignment,
 *          then it is the responsibility of the user to manually align the
 *          memory buffer for SCI.  Thus, the user may have to allocate a
 *          larger buffer to meet the alignment.  Additionally, the user will
 *          need to remember the actual memory allocation addresses in order
 *          to ensure the memory can be properly freed when necessary to do
 *          so.
 *
 * @pre This method will return a valid handle, but the MDL may not be
 *      accurate until after the user has invoked the associated
 *      sci_controller_initialize() routine.
 *
 * @param[in]  controller This parameter specifies the controller for which
 *             to retrieve the DMA safe memory descriptor list.
 *
 * @return A pointer to a physical memory descriptor array.
 */
SCI_MEMORY_DESCRIPTOR_LIST_HANDLE_T
sci_controller_get_memory_descriptor_list_handle(
   SCI_CONTROLLER_HANDLE_T  controller
);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _SCI_CONTROLLER_H_

