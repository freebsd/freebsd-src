/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2024, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ICE_STATUS_H_
#define _ICE_STATUS_H_

/* Error Codes */
enum ice_status {
	ICE_SUCCESS				= 0,

	/* Generic codes : Range -1..-49 */
	ICE_ERR_PARAM				= -1,
	ICE_ERR_NOT_IMPL			= -2,
	ICE_ERR_NOT_READY			= -3,
	ICE_ERR_NOT_SUPPORTED			= -4,
	ICE_ERR_BAD_PTR				= -5,
	ICE_ERR_INVAL_SIZE			= -6,
	ICE_ERR_DEVICE_NOT_SUPPORTED		= -8,
	ICE_ERR_RESET_FAILED			= -9,
	ICE_ERR_FW_API_VER			= -10,
	ICE_ERR_NO_MEMORY			= -11,
	ICE_ERR_CFG				= -12,
	ICE_ERR_OUT_OF_RANGE			= -13,
	ICE_ERR_ALREADY_EXISTS			= -14,
	ICE_ERR_DOES_NOT_EXIST			= -15,
	ICE_ERR_IN_USE				= -16,
	ICE_ERR_MAX_LIMIT			= -17,
	ICE_ERR_RESET_ONGOING			= -18,
	ICE_ERR_HW_TABLE			= -19,
	ICE_ERR_FW_DDP_MISMATCH			= -20,

	/* NVM specific error codes: Range -50..-59 */
	ICE_ERR_NVM				= -50,
	ICE_ERR_NVM_CHECKSUM			= -51,
	ICE_ERR_BUF_TOO_SHORT			= -52,
	ICE_ERR_NVM_BLANK_MODE			= -53,

	/* ARQ/ASQ specific error codes. Range -100..-109 */
	ICE_ERR_AQ_ERROR			= -100,
	ICE_ERR_AQ_TIMEOUT			= -101,
	ICE_ERR_AQ_FULL				= -102,
	ICE_ERR_AQ_NO_WORK			= -103,
	ICE_ERR_AQ_EMPTY			= -104,
	ICE_ERR_AQ_FW_CRITICAL			= -105,
};

#endif /* _ICE_STATUS_H_ */
