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

#ifndef __DAPL_FFT_CMD_H__
#define __DAPL_FFT_CMD_H__

#include "dapl_proto.h"

#define MAXCASES	100

typedef enum
{
    NONE,
    HWCONN,
    CQMGT,
    ENDPOINT,
    PTAGMGT,
    MEMMGT,
    CONNMGT,
    CONNMGT_CLIENT,
    DATAXFER,
    DATAXFER_CLIENT,
    QUERYINFO,
    NS,
    ERRHAND,
    UNSUPP,
    STRESS,
    STRESS_CLIENT,
} FFT_Type_e;


typedef struct
{
    FFT_Type_e		fft_type;
    char 		device_name[256];	//-D
    char		server_name[256];
    bool		cases_flag[MAXCASES];
    int 		size;
    int 		num_iter;	//-i
    int 		num_threads;	//-t
    int 		num_vis;	//-v
    DAT_QOS		ReliabilityLevel;	//-R
} FFT_Cmd_t;

#endif
