/* Copyright (c) 2008-2011 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**************************************************************************//**

 @File          part_ext.h

 @Description   Definitions for the part (integration) module.
*//***************************************************************************/

#ifndef __PART_EXT_H
#define __PART_EXT_H

#include "std_ext.h"
#include "part_integration_ext.h"


#if !(defined(MPC8306) || \
      defined(MPC8309) || \
      defined(MPC834x) || \
      defined(MPC836x) || \
      defined(MPC832x) || \
      defined(MPC837x) || \
      defined(MPC8568) || \
      defined(MPC8569) || \
      defined(P1020)   || \
      defined(P1021)   || \
      defined(P1022)   || \
      defined(P1023)   || \
      defined(P2020)   || \
      defined(P2040)   || \
      defined(P2041)   || \
      defined(P3041)   || \
      defined(P4080)   || \
      defined(SC4080)  || \
      defined(P5020)   || \
      defined(MSC814x))
#error "unable to proceed without chip-definition"
#endif /* !(defined(MPC834x) || ... */


/**************************************************************************//*
 @Description   Part data structure - must be contained in any integration
                data structure.
*//***************************************************************************/
typedef struct t_Part
{
    uintptr_t   (* f_GetModuleBase)(t_Handle h_Part, e_ModuleId moduleId);
                /**< Returns the address of the module's memory map base. */
    e_ModuleId  (* f_GetModuleIdByBase)(t_Handle h_Part, uintptr_t baseAddress);
                /**< Returns the module's ID according to its memory map base. */
} t_Part;


#endif /* __PART_EXT_H */
