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
#ifndef _SCIC_SDS_STP_PACKET_REQUEST_H_
#define _SCIC_SDS_STP_PACKET_REQUEST_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <dev/isci/scil/intel_sas.h>
#include <dev/isci/scil/sci_types.h>
#include <dev/isci/scil/scic_sds_stp_request.h>

/**
 * @file
 *
 * @brief This file contains the structures and constants for PACKET protocol
 *        requests.
 */


/**
 * @enum
 *
 * This is the enumeration of the SATA PIO DATA IN started substate machine.
 */
enum _SCIC_SDS_STP_PACKET_REQUEST_STARTED_SUBSTATES
{
   /**
    * While in this state the IO request object is waiting for the TC completion
    * notification for the H2D Register FIS
    */
   SCIC_SDS_STP_PACKET_REQUEST_STARTED_PACKET_PHASE_AWAIT_TC_COMPLETION_SUBSTATE,

   /**
    * While in this state the IO request object is waiting for either a PIO Setup.
    */
   SCIC_SDS_STP_PACKET_REQUEST_STARTED_PACKET_PHASE_AWAIT_PIO_SETUP_SUBSTATE,

   /**
    * While in this state the IO request object is waiting for TC completion for
    * the Packet DMA DATA fis or Raw Frame.
    */
   SCIC_SDS_STP_PACKET_REQUEST_STARTED_COMMAND_PHASE_AWAIT_TC_COMPLETION_SUBSTATE,

   /**
    * The non-data IO transit to this state in this state after receiving TC
    * completion. While in this state IO request object is waiting for D2H status
    * frame as UF.
    */
   SCIC_SDS_STP_PACKET_REQUEST_STARTED_COMMAND_PHASE_AWAIT_D2H_FIS_SUBSTATE,

   /**
    * The IO transit to this state in this state if the previous TC completion status
    * is not success and the atapi device is suspended due to target device failed the IO.
    * While in this state IO request object is waiting for device coming out of the
    * suspension state then complete the IO.
    */
   SCIC_SDS_STP_PACKET_REQUEST_STARTED_COMPLETION_DELAY_SUBSTATE,

   SCIC_SDS_STP_PACKET_REQUEST_STARTED_MAX_SUBSTATES
};



#if !defined(DISABLE_ATAPI)
extern SCI_BASE_STATE_T scic_sds_stp_packet_request_started_substate_table[];
extern SCIC_SDS_IO_REQUEST_STATE_HANDLER_T
	scic_sds_stp_packet_request_started_substate_handler_table[];
#endif // !defined(DISABLE_ATAPI)

#if !defined(DISABLE_ATAPI)
SCI_STATUS scic_sds_stp_packet_request_construct(
   SCIC_SDS_REQUEST_T * this_request
);
#else  // !defined(DISABLE_ATAPI)
#define scic_sds_stp_packet_request_construct(request) SCI_FAILURE
#endif // !defined(DISABLE_ATAPI)

#if !defined(DISABLE_ATAPI)
void scu_stp_packet_request_command_phase_construct_task_context(
   SCIC_SDS_REQUEST_T * this_request,
   SCU_TASK_CONTEXT_T * task_context
);
#else  // !defined(DISABLE_ATAPI)
#define scu_stp_packet_request_command_phase_construct_task_context(reqeust, tc)
#endif // !defined(DISABLE_ATAPI)

#if !defined(DISABLE_ATAPI)
void scu_stp_packet_request_command_phase_reconstruct_raw_frame_task_context(
   SCIC_SDS_REQUEST_T * this_request,
   SCU_TASK_CONTEXT_T * task_context
);
#else  // !defined(DISABLE_ATAPI)
#define scu_stp_packet_request_command_phase_reconstruct_raw_frame_task_context(reqeust, tc)
#endif // !defined(DISABLE_ATAPI)

#if !defined(DISABLE_ATAPI)
SCI_STATUS scic_sds_stp_packet_request_process_status_fis(
   SCIC_SDS_REQUEST_T * this_request,
   SATA_FIS_REG_D2H_T * status_fis
);
#else  // !defined(DISABLE_ATAPI)
#define scic_sds_stp_packet_request_process_status_fis(reqeust, fis) SCI_FAILURE
#endif // !defined(DISABLE_ATAPI)

#if !defined(DISABLE_ATAPI)
void scic_sds_stp_packet_internal_request_sense_build_sgl(
   SCIC_SDS_REQUEST_T * this_request
);
#else  // !defined(DISABLE_ATAPI)
#define scic_sds_stp_packet_internal_request_sense_build_sgl(request)
#endif // !defined(DISABLE_ATAPI)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _SCIC_SDS_STP_PACKET_REQUEST_H_

