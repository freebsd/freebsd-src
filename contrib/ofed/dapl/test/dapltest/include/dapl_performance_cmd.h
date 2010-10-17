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

#ifndef __DAPL_PERFORMANCE_CMD_H__
#define __DAPL_PERFORMANCE_CMD_H__

#include "dapl_proto.h"

#define NAME_SZ 256

typedef enum
{
    BLOCKING_MODE,
    POLLING_MODE
} Performance_Mode_Type;

#pragma pack (2)
typedef struct
{
    DAT_UINT32      transfer_type;
    DAT_UINT32      seg_size;
    DAT_UINT32      num_segs;
} Performance_Cmd_Op_t;

typedef struct
{
    DAT_UINT32      		dapltest_version;
    DAT_UINT32      		client_is_little_endian;
    char            		server_name[NAME_SZ];	/* -s */
    char            		dapl_name[NAME_SZ];	/* -D */
    DAT_QOS         		qos;
    DAT_UINT32      		debug;			/* -d */
    Performance_Mode_Type	mode;			/* -m */
    DAT_UINT32      		num_iterations;		/* -i */
    DAT_UINT32      		pipeline_len;		/* -p */
    Performance_Cmd_Op_t 	op;
    DAT_UINT32      		use_rsp;		/* -r */

} Performance_Cmd_t;
#pragma pack ()

#endif
