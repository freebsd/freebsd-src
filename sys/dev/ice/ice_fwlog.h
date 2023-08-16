/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2023, Intel Corporation
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

#ifndef _ICE_FWLOG_H_
#define _ICE_FWLOG_H_
#include "ice_adminq_cmd.h"

struct ice_hw;

/* Only a single log level should be set and all log levels under the set value
 * are enabled, e.g. if log level is set to ICE_FW_LOG_LEVEL_VERBOSE, then all
 * other log levels are included (except ICE_FW_LOG_LEVEL_NONE)
 */
enum ice_fwlog_level {
	ICE_FWLOG_LEVEL_NONE = 0,
	ICE_FWLOG_LEVEL_ERROR = 1,
	ICE_FWLOG_LEVEL_WARNING = 2,
	ICE_FWLOG_LEVEL_NORMAL = 3,
	ICE_FWLOG_LEVEL_VERBOSE = 4,
	ICE_FWLOG_LEVEL_INVALID, /* all values >= this entry are invalid */
};

struct ice_fwlog_module_entry {
	/* module ID for the corresponding firmware logging event */
	u16 module_id;
	/* verbosity level for the module_id */
	u8 log_level;
};

struct ice_fwlog_cfg {
	/* list of modules for configuring log level */
	struct ice_fwlog_module_entry module_entries[ICE_AQC_FW_LOG_ID_MAX];
#define ICE_FWLOG_OPTION_ARQ_ENA		BIT(0)
#define ICE_FWLOG_OPTION_UART_ENA		BIT(1)
	/* set before calling ice_fwlog_init() so the PF registers for firmware
	 * logging on initialization
	 */
#define ICE_FWLOG_OPTION_REGISTER_ON_INIT	BIT(2)
	/* set in the ice_fwlog_get() response if the PF is registered for FW
	 * logging events over ARQ
	 */
#define ICE_FWLOG_OPTION_IS_REGISTERED		BIT(3)
	/* options used to configure firmware logging */
	u16 options;
	/* minimum number of log events sent per Admin Receive Queue event */
	u16 log_resolution;
};

void ice_fwlog_set_support_ena(struct ice_hw *hw);
bool ice_fwlog_supported(struct ice_hw *hw);
enum ice_status ice_fwlog_init(struct ice_hw *hw, struct ice_fwlog_cfg *cfg);
enum ice_status ice_fwlog_set(struct ice_hw *hw, struct ice_fwlog_cfg *cfg);
enum ice_status ice_fwlog_get(struct ice_hw *hw, struct ice_fwlog_cfg *cfg);
enum ice_status
ice_fwlog_update_modules(struct ice_hw *hw,
			 struct ice_fwlog_module_entry *entries,
			 u16 num_entries);
enum ice_status ice_fwlog_register(struct ice_hw *hw);
enum ice_status ice_fwlog_unregister(struct ice_hw *hw);
void
ice_fwlog_event_dump(struct ice_hw *hw, struct ice_aq_desc *desc, void *buf);
#endif /* _ICE_FWLOG_H_ */
