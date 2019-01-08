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

#ifndef __DAPL_TRANSACTION_STATS_H__
#define __DAPL_TRANSACTION_STATS_H__

#include "dapl_proto.h"

#define whole(num)       ((unsigned int)(num))
#define hundredths(num)  ((unsigned int)(((num) - (unsigned int)(num)) * 100))

typedef struct
{
    DT_Mdep_LockType	lock;
    unsigned int	wait_count;
    unsigned int	num_ops;
    unsigned int	time_ms;
    unsigned int	bytes_send;
    unsigned int	bytes_recv;
    unsigned int	bytes_rdma_read;
    unsigned int	bytes_rdma_write;
} Transaction_Stats_t;
#endif
