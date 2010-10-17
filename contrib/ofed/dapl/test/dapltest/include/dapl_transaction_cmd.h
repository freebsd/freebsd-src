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

#ifndef __DAPL_TRANSACTION_CMD_H__
#define __DAPL_TRANSACTION_CMD_H__

#include "dapl_proto.h"

#define MAX_OPS 100
#define NAME_SZ 256

#pragma pack (2)
typedef struct
{
    DAT_UINT32      server_initiated;
    DAT_UINT32      transfer_type;
    DAT_UINT32      num_segs;
    DAT_UINT32      seg_size;
    DAT_UINT32      reap_send_on_recv;
} Transaction_Cmd_Op_t;

typedef struct
{
    DAT_UINT32      dapltest_version;
    DAT_UINT32      client_is_little_endian;
    char            server_name[NAME_SZ];   /* -s */
    DAT_UINT32      num_iterations;	/* -i */
    DAT_UINT32      num_threads;	/* -t */
    DAT_UINT32      eps_per_thread;	/* -w */
    DAT_UINT32      use_cno;		/* NOT USED - remove and bump version*/
    DAT_UINT32      use_rsp;		/* -r */
    DAT_UINT32      debug;		/* -d */
    DAT_UINT32      validate;		/* -V */
    DAT_UINT32      poll;		/* -P */
    char            dapl_name[NAME_SZ]; /* -D */
    DAT_QOS         ReliabilityLevel;
    DAT_UINT32      num_ops;
    Transaction_Cmd_Op_t op[MAX_OPS];
} Transaction_Cmd_t;
#pragma pack ()

#endif
