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

#ifndef __DAPL_STATS_H__
#define __DAPL_STATS_H__

#include "dapl_proto.h"

#define DT_min(a, b)	   ((a < b) ? (a) : (b))
#define DT_max(a, b)        ((a > b) ? (a) : (b))
#define DT_whole(num)      ((unsigned int)(num))
#define DT_hundredths(num) ((unsigned int)(((num) - (unsigned int)(num)) * 100))

typedef struct
{
    unsigned int 	num;
    DT_Mdep_TimeStamp 	total_ts;
    DT_Mdep_TimeStamp 	max_ts;
    DT_Mdep_TimeStamp 	min_ts;
} Performance_Stats_Data_t;


typedef struct
{
    unsigned int 		num_ops;
    int64_t 			bytes;
    unsigned int 		post_ctxt_switch_num;
    unsigned int 		reap_ctxt_switch_num;
    double			cpu_utilization;
    DT_Mdep_TimeStamp 		time_ts;
    Performance_Stats_Data_t 	posts_sans_ctxt;
    Performance_Stats_Data_t 	posts_with_ctxt;
    Performance_Stats_Data_t 	reaps_sans_ctxt;
    Performance_Stats_Data_t 	reaps_with_ctxt;
    Performance_Stats_Data_t	latency;
} Performance_Stats_t;

#endif
