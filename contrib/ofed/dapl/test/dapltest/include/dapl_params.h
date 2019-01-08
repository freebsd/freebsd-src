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

#ifndef __DAPL_PARAMS_H__
#define __DAPL_PARAMS_H__

#include "dapl_proto.h"
#include "dapl_server_cmd.h"
#include "dapl_transaction_cmd.h"
#include "dapl_performance_cmd.h"
#include "dapl_limit_cmd.h"
#include "dapl_quit_cmd.h"
#include "dapl_fft_cmd.h"

typedef enum
{
    SERVER_TEST,
    TRANSACTION_TEST,
    PERFORMANCE_TEST,
    LIMIT_TEST,
    QUIT_TEST,
    FFT_TEST
} test_type_e;

typedef struct
{
    test_type_e     test_type;

    union
    {
	Server_Cmd_t        Server_Cmd;
	Transaction_Cmd_t   Transaction_Cmd;
	Performance_Cmd_t   Performance_Cmd;
	Limit_Cmd_t         Limit_Cmd;
	Quit_Cmd_t          Quit_Cmd;
	FFT_Cmd_t	    FFT_Cmd;
    } u;

    /* Needed here due to structure of command processing */
    DAT_QOS         ReliabilityLevel;
    DAT_SOCK_ADDR   server_netaddr;
    void *	    phead;
    bool	    local_is_little_endian;
    bool	    debug;
    double	    cpu_mhz;
} Params_t;

#endif
