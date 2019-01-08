/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

#include "dapl_proto.h"

void DT_Performance_Cmd_Endian(Performance_Cmd_t * cmd)
{
	cmd->dapltest_version = DT_Endian32(cmd->dapltest_version);
	cmd->qos = DT_Endian32(cmd->qos);
	cmd->num_iterations = DT_Endian32(cmd->num_iterations);
	cmd->debug = DT_Endian32(cmd->debug);

	cmd->op.transfer_type = DT_Endian32(cmd->op.transfer_type);
	cmd->op.seg_size = DT_Endian32(cmd->op.seg_size);
	cmd->op.num_segs = DT_Endian32(cmd->op.num_segs);
}

/*
 *  * Map Performance_Mode_Type values to readable strings
 *   */
const char *DT_PerformanceModeToString(Performance_Mode_Type mode)
{
	if (BLOCKING_MODE == mode) {
		return "blocking";
	} else if (POLLING_MODE == mode) {
		return "polling";
	} else {
		return "error: unkown mode";
	}
}
