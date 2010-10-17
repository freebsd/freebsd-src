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

void DT_Transaction_Cmd_Endian(Transaction_Cmd_t * cmd, bool to_wire)
{
	unsigned int i;

	cmd->dapltest_version = DT_Endian32(cmd->dapltest_version);
	cmd->num_iterations = DT_Endian32(cmd->num_iterations);
	cmd->num_threads = DT_Endian32(cmd->num_threads);
	cmd->eps_per_thread = DT_Endian32(cmd->eps_per_thread);
	cmd->use_rsp = DT_Endian32(cmd->use_rsp);
	cmd->debug = DT_Endian32(cmd->debug);
	cmd->validate = DT_Endian32(cmd->validate);
	cmd->ReliabilityLevel = DT_Endian32(cmd->ReliabilityLevel);

	if (!to_wire) {
		cmd->num_ops = DT_Endian32(cmd->num_ops);
	}
	for (i = 0; i < cmd->num_ops; i++) {
		cmd->op[i].server_initiated =
		    DT_Endian32(cmd->op[i].server_initiated);
		cmd->op[i].transfer_type =
		    DT_Endian32(cmd->op[i].transfer_type);
		cmd->op[i].num_segs = DT_Endian32(cmd->op[i].num_segs);
		cmd->op[i].seg_size = DT_Endian32(cmd->op[i].seg_size);
		cmd->op[i].reap_send_on_recv =
		    DT_Endian32(cmd->op[i].reap_send_on_recv);
	}
	if (to_wire) {
		cmd->num_ops = DT_Endian32(cmd->num_ops);
	}
}
