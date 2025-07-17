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
#ifndef _SCI_MEMORY_DESCRIPTOR_LIST_H_
#define _SCI_MEMORY_DESCRIPTOR_LIST_H_

/**
 * @file
 *
 * @brief This file contains all of the basic data types utilized by an
 *        SCI user or implementor.
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <dev/isci/scil/sci_types.h>

/**
 * @name SCI_MDE_ATTRIBUTES
 *
 * These constants depict memory attributes for the Memory
 * Descriptor Entries (MDEs) contained in the MDL.
 */
/*@{*/
#define SCI_MDE_ATTRIBUTE_CACHEABLE              0x0001
#define SCI_MDE_ATTRIBUTE_PHYSICALLY_CONTIGUOUS  0x0002
/*@}*/

/**
 * @struct SCI_PHYSICAL_MEMORY_DESCRIPTOR
 * @brief  This structure defines a description of a memory location for
 *         the SCI implementation.
 */
typedef struct SCI_PHYSICAL_MEMORY_DESCRIPTOR
{
   /**
    * This field contains the virtual address associated with this descriptor
    * element. This field shall be zero when the descriptor is retrieved from
    * the SCI implementation.  The user shall set this field prior
    * sci_controller_start()
    */
   void * virtual_address;

   /**
    * This field contains the physical address associated with this descriptor 
    * element. This field shall be zero when the descriptor is retrieved from
    * the SCI implementation.  The user shall set this field prior
    * sci_controller_start()
    */
   SCI_PHYSICAL_ADDRESS  physical_address;

   /**
    * This field contains the size requirement for this memory descriptor.
    * A value of zero for this field indicates the end of the descriptor
    * list.  The value should be treated as read only for an SCI user.
    */
   U32 constant_memory_size;

   /**
    * This field contains the alignment requirement for this memory
    * descriptor.  A value of zero for this field indicates the end of the
    * descriptor list.  All other values indicate the number of bytes to
    * achieve the necessary alignment.  The value should be treated as
    * read only for an SCI user.
    */
   U32 constant_memory_alignment;

   /**
    * This field contains an indication regarding the desired memory
    * attributes for this memory descriptor entry.
    * Notes:
    * - If the cacheable attribute is set, the user can allocate
    *   memory that is backed by cache for better performance. It
    *   is not required that the memory be backed by cache.
    * - If the physically contiguous attribute is set, then the
    *   entire memory must be physically contiguous across all
    *   page boundaries.
    */
   U16 constant_memory_attributes;

} SCI_PHYSICAL_MEMORY_DESCRIPTOR_T;

/**
 * @brief This method simply rewinds the MDL iterator back to the first memory
 *        descriptor entry in the list.
 *
 * @param[in] mdl This parameter specifies the memory descriptor list that
 *            is to be rewound.
 *
 * @return none
 */
void sci_mdl_first_entry(
   SCI_MEMORY_DESCRIPTOR_LIST_HANDLE_T mdl
);

/**
 * @brief This method simply updates the "current" pointer to the next
 *        sequential memory descriptor.
 *
 * @param[in] mdl This parameter specifies the memory descriptor list for
 *            which to return the next memory descriptor entry in the list.
 *
 * @return none.
 */
void sci_mdl_next_entry(
   SCI_MEMORY_DESCRIPTOR_LIST_HANDLE_T mdl
);

/**
 * @brief This method simply returns the current memory descriptor entry.
 *
 * @param[in] mdl This parameter specifies the memory descriptor list for
 *            which to return the current memory descriptor entry.
 *
 * @return This method returns a pointer to the current physical memory
 *         descriptor in the MDL.
 * @retval NULL This value is returned if there are no descriptors in the
 *         list.
 */
SCI_PHYSICAL_MEMORY_DESCRIPTOR_T * sci_mdl_get_current_entry(
   SCI_MEMORY_DESCRIPTOR_LIST_HANDLE_T mdl
);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _SCI_MEMORY_DESCRIPTOR_LIST_H_

