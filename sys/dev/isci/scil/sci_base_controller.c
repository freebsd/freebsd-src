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
 * @brief This file contains the base controller method implementations and
 *        any constants or structures private to the base controller object
 *        or common to all controller derived objects.
 */

#include <dev/isci/scil/sci_base_controller.h>
#include <dev/isci/scil/sci_controller.h>

//******************************************************************************
//* P U B L I C   M E T H O D S
//******************************************************************************

SCI_MEMORY_DESCRIPTOR_LIST_HANDLE_T sci_controller_get_memory_descriptor_list_handle(
   SCI_CONTROLLER_HANDLE_T controller
)
{
   SCI_BASE_CONTROLLER_T * this_controller = (SCI_BASE_CONTROLLER_T*)controller;
   return (SCI_MEMORY_DESCRIPTOR_LIST_HANDLE_T) &this_controller->mdl;
}

//******************************************************************************
//* P R O T E C T E D   M E T H O D S
//******************************************************************************

void sci_base_controller_construct(
   SCI_BASE_CONTROLLER_T               * this_controller,
   SCI_BASE_LOGGER_T                   * logger,
   SCI_BASE_STATE_T                    * state_table,
   SCI_PHYSICAL_MEMORY_DESCRIPTOR_T    * mdes,
   U32                                   mde_count,
   SCI_MEMORY_DESCRIPTOR_LIST_HANDLE_T   next_mdl
)
{
   sci_base_object_construct((SCI_BASE_OBJECT_T *)this_controller, logger);

   sci_base_state_machine_construct(
      &this_controller->state_machine,
      &this_controller->parent,
      state_table,
      SCI_BASE_CONTROLLER_STATE_INITIAL
   );

   sci_base_mdl_construct(&this_controller->mdl, mdes, mde_count, next_mdl);

   sci_base_state_machine_start(&this_controller->state_machine);
}

