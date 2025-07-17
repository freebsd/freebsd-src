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

#include <sys/cdefs.h>
/**
 * @file
 *
 * @brief This file contains the base implementation for the memory
 *        descriptor list.  This is currently comprised of MDL iterator
 *        methods.
 */

#include <dev/isci/scil/sci_base_memory_descriptor_list.h>

//******************************************************************************
//* P U B L I C   M E T H O D S
//******************************************************************************

void sci_mdl_first_entry(
   SCI_MEMORY_DESCRIPTOR_LIST_HANDLE_T mdl
)
{
   SCI_BASE_MEMORY_DESCRIPTOR_LIST_T * base_mdl = (SCI_BASE_MEMORY_DESCRIPTOR_LIST_T*) mdl;

   base_mdl->next_index = 0;

   // If this MDL is managing another MDL, then recursively rewind that MDL
   // object as well.
   if (base_mdl->next_mdl != SCI_INVALID_HANDLE)
      sci_mdl_first_entry(base_mdl->next_mdl);
}

// ---------------------------------------------------------------------------

void sci_mdl_next_entry(
   SCI_MEMORY_DESCRIPTOR_LIST_HANDLE_T mdl
)
{
   SCI_BASE_MEMORY_DESCRIPTOR_LIST_T * base_mdl = (SCI_BASE_MEMORY_DESCRIPTOR_LIST_T*) mdl;

   // If there is at least one more entry left in the array, then change
   // the next pointer to it.
   if (base_mdl->next_index < base_mdl->length)
      base_mdl->next_index++;
   else if (base_mdl->next_index == base_mdl->length)
   {
      // This MDL has exhausted it's set of entries.  If this MDL is managing
      // another MDL, then start iterating through that MDL.
      if (base_mdl->next_mdl != SCI_INVALID_HANDLE)
         sci_mdl_next_entry(base_mdl->next_mdl);
   }
}

// ---------------------------------------------------------------------------

SCI_PHYSICAL_MEMORY_DESCRIPTOR_T * sci_mdl_get_current_entry(
   SCI_MEMORY_DESCRIPTOR_LIST_HANDLE_T mdl
)
{
   SCI_BASE_MEMORY_DESCRIPTOR_LIST_T * base_mdl = (SCI_BASE_MEMORY_DESCRIPTOR_LIST_T*) mdl;

   if (base_mdl->next_index < base_mdl->length)
      return & base_mdl->mde_array[base_mdl->next_index];
   else if (base_mdl->next_index == base_mdl->length)
   {
      // This MDL has exhausted it's set of entries.  If this MDL is managing
      // another MDL, then return it's current entry.
      if (base_mdl->next_mdl != SCI_INVALID_HANDLE)
         return sci_mdl_get_current_entry(base_mdl->next_mdl);
   }

   return NULL;
}

//******************************************************************************
//* P R O T E C T E D   M E T H O D S
//******************************************************************************

void sci_base_mdl_construct(
   SCI_BASE_MEMORY_DESCRIPTOR_LIST_T * mdl,
   SCI_PHYSICAL_MEMORY_DESCRIPTOR_T  * mde_array,
   U32                                 mde_array_length,
   SCI_MEMORY_DESCRIPTOR_LIST_HANDLE_T next_mdl
)
{
   mdl->length     = mde_array_length;
   mdl->mde_array  = mde_array;
   mdl->next_index = 0;
   mdl->next_mdl   = next_mdl;
}

// ---------------------------------------------------------------------------

BOOL sci_base_mde_is_valid(
   SCI_PHYSICAL_MEMORY_DESCRIPTOR_T * mde,
   U32                                alignment,
   U32                                size,
   U16                                attributes
)
{
   // Only need the lower 32 bits to ensure alignment is met.
   U32 physical_address = sci_cb_physical_address_lower(mde->physical_address);

   if (
         ((physical_address & (alignment - 1)) != 0)
      || (mde->constant_memory_alignment != alignment)
      || (mde->constant_memory_size != size)
      || (mde->virtual_address == NULL)
      || (mde->constant_memory_attributes != attributes)
      )
   {
      return FALSE;
   }

   return TRUE;
}

