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

void DT_performance_stats_init(Performance_Stats_t * stats)
{
	stats->num_ops = 0;
	stats->bytes = 0;
	stats->post_ctxt_switch_num = 0;
	stats->reap_ctxt_switch_num = 0;

	stats->cpu_utilization = 0.0;
	stats->time_ts = 0;

	stats->posts_sans_ctxt.num = 0;
	stats->posts_sans_ctxt.total_ts = 0;
	stats->posts_sans_ctxt.max_ts = 0;
	stats->posts_sans_ctxt.min_ts = ~0;

	stats->posts_with_ctxt.num = 0;
	stats->posts_with_ctxt.total_ts = 0;
	stats->posts_with_ctxt.max_ts = 0;
	stats->posts_with_ctxt.min_ts = ~0;

	stats->reaps_sans_ctxt.num = 0;
	stats->reaps_sans_ctxt.total_ts = 0;
	stats->reaps_sans_ctxt.max_ts = 0;
	stats->reaps_sans_ctxt.min_ts = ~0;

	stats->reaps_with_ctxt.num = 0;
	stats->reaps_with_ctxt.total_ts = 0;
	stats->reaps_with_ctxt.max_ts = 0;
	stats->reaps_with_ctxt.min_ts = ~0;

	stats->latency.num = 0;
	stats->latency.total_ts = 0;
	stats->latency.max_ts = 0;
	stats->latency.min_ts = ~0;
}

void
DT_performance_stats_record_post(Performance_Stats_t * stats,
				 unsigned long ctxt_switch_num,
				 DT_Mdep_TimeStamp ts)
{
	if (ctxt_switch_num) {
		stats->posts_with_ctxt.num++;
		stats->posts_with_ctxt.total_ts += ts;
		stats->posts_with_ctxt.max_ts =
		    DT_max(stats->posts_with_ctxt.max_ts, ts);
		stats->posts_with_ctxt.min_ts =
		    DT_min(stats->posts_with_ctxt.min_ts, ts);

		stats->post_ctxt_switch_num += ctxt_switch_num;
	} else {
		stats->posts_sans_ctxt.num++;
		stats->posts_sans_ctxt.total_ts += ts;
		stats->posts_sans_ctxt.max_ts =
		    DT_max(stats->posts_sans_ctxt.max_ts, ts);
		stats->posts_sans_ctxt.min_ts =
		    DT_min(stats->posts_sans_ctxt.min_ts, ts);
	}
}

void
DT_performance_stats_record_reap(Performance_Stats_t * stats,
				 unsigned long ctxt_switch_num,
				 DT_Mdep_TimeStamp ts)
{
	if (ctxt_switch_num) {
		stats->reaps_with_ctxt.num++;
		stats->reaps_with_ctxt.total_ts += ts;
		stats->reaps_with_ctxt.max_ts =
		    DT_max(stats->reaps_with_ctxt.max_ts, ts);
		stats->reaps_with_ctxt.min_ts =
		    DT_min(stats->reaps_with_ctxt.min_ts, ts);

		stats->reap_ctxt_switch_num += ctxt_switch_num;
	} else {
		stats->reaps_sans_ctxt.num++;
		stats->reaps_sans_ctxt.total_ts += ts;
		stats->reaps_sans_ctxt.max_ts =
		    DT_max(stats->reaps_sans_ctxt.max_ts, ts);
		stats->reaps_sans_ctxt.min_ts =
		    DT_min(stats->reaps_sans_ctxt.min_ts, ts);
	}
}

void
DT_performance_stats_record_latency(Performance_Stats_t * stats,
				    DT_Mdep_TimeStamp ts)
{
	stats->latency.num++;
	stats->latency.total_ts += ts;
	stats->latency.max_ts = DT_max(stats->latency.max_ts, ts);
	stats->latency.min_ts = DT_min(stats->latency.min_ts, ts);
}

void
DT_performance_stats_data_combine(Performance_Stats_Data_t * dest,
				  Performance_Stats_Data_t * src_a,
				  Performance_Stats_Data_t * src_b)
{
	dest->num = src_a->num + src_b->num;
	dest->total_ts = src_a->total_ts + src_b->total_ts;
	dest->max_ts = DT_max(src_a->max_ts, src_b->max_ts);
	dest->min_ts = DT_min(src_a->min_ts, src_b->min_ts);
}

void
DT_performance_stats_combine(Performance_Stats_t * dest,
			     Performance_Stats_t * src_a,
			     Performance_Stats_t * src_b)
{
	dest->num_ops = src_a->num_ops + src_b->num_ops;

	dest->bytes = src_a->bytes + src_b->bytes;

	dest->post_ctxt_switch_num =
	    src_a->post_ctxt_switch_num + src_b->post_ctxt_switch_num;

	dest->reap_ctxt_switch_num =
	    src_b->reap_ctxt_switch_num + src_b->reap_ctxt_switch_num;

	dest->cpu_utilization = DT_max(src_a->cpu_utilization,
				       src_b->cpu_utilization);
	dest->time_ts = DT_max(src_a->time_ts, src_b->time_ts);

	DT_performance_stats_data_combine(&dest->posts_sans_ctxt,
					  &src_a->posts_sans_ctxt,
					  &src_b->posts_sans_ctxt);

	DT_performance_stats_data_combine(&dest->posts_with_ctxt,
					  &src_a->posts_with_ctxt,
					  &src_b->posts_with_ctxt);

	DT_performance_stats_data_combine(&dest->reaps_sans_ctxt,
					  &src_a->reaps_sans_ctxt,
					  &src_b->reaps_sans_ctxt);

	DT_performance_stats_data_combine(&dest->reaps_with_ctxt,
					  &src_a->reaps_with_ctxt,
					  &src_b->reaps_with_ctxt);

	DT_performance_stats_data_combine(&dest->latency,
					  &src_a->latency, &src_b->latency);
}

double
DT_performance_stats_data_print(DT_Tdep_Print_Head * phead,
				Performance_Stats_Data_t * data, double cpu_mhz)
{
	double average;

	average = (int64_t) data->total_ts / (data->num * cpu_mhz);

	DT_Tdep_PT_Printf(phead,
			  "    Arithmetic mean      : %d.%d us\n"
			  "    maximum              : %d.%d us\n"
			  "    minimum              : %d.%d us\n",
			  DT_whole(average), DT_hundredths(average),
			  DT_whole((int64_t) data->max_ts / cpu_mhz),
			  DT_hundredths((int64_t) data->max_ts / cpu_mhz),
			  DT_whole((int64_t) data->min_ts / cpu_mhz),
			  DT_hundredths((int64_t) data->min_ts / cpu_mhz));
	return average;
}

void
DT_performance_stats_print(Params_t * params_ptr,
			   DT_Tdep_Print_Head * phead,
			   Performance_Stats_t * stats,
			   Performance_Cmd_t * cmd, Performance_Test_t * test)
{
	double cpu_mhz;
	double time_s;
	double mbytes;
	double ops_per_sec;
	double bandwidth;
	double latency;
	double time_per_post;
	double time_per_reap;

	cpu_mhz = params_ptr->cpu_mhz;
	latency = 0;
#if defined(WIN32)
	/*
	 * The Microsoft compiler is unable to do a 64 bit conversion when
	 * working with double. time_ts is a 64 bit value, so we
	 * potentially lose precision, so limit it to the Windows
	 * platform. Trying to do the operation below without casting
	 * a 64 bit value to an unsigned int results in the error when
	 * using Visual C 6.0:
	 *
	 *    Compiler Error C2520: conversion from unsigned __int64 to
	 *    double not implemented, use signed __int64.
	 *
	 * Note that signed __int64 doesn't work either!
	 */
	time_s = (double)((unsigned int)stats->time_ts / (1000000.0 * cpu_mhz));
#else
	time_s = (double)(stats->time_ts / (1000000.0 * cpu_mhz));
#endif
	mbytes = (double)(1.0 * stats->bytes) / 1024 / 1024;

	if (0.0 == time_s) {
		DT_Tdep_PT_Printf(phead, "Error determining time\n");
		return;
	} else if (0 == stats->num_ops) {
		DT_Tdep_PT_Printf(phead,
				  "Error determining number of operations\n");
		return;
	} else if (0.0 == cpu_mhz) {
		DT_Tdep_PT_Printf(phead, "Error determining CPU speed\n");
		return;
	}

	ops_per_sec = stats->num_ops / time_s;
	bandwidth = mbytes / time_s;
	DT_Tdep_PT_Printf(phead,
			  "------------------------- Statistics -------------------------\n");
	DT_Tdep_PT_Printf(phead,
			  "    Mode                 : %s\n"
			  "    Operation Type       : %s\n"
			  "    Number of Operations : %u\n"
			  "    Segment Size         : %u\n"
			  "    Number of Segments   : %u \n"
			  "    Pipeline Length      : %u\n\n",
			  DT_PerformanceModeToString(cmd->mode),
			  DT_TransferTypeToString(cmd->op.transfer_type),
			  cmd->num_iterations, cmd->op.seg_size,
			  cmd->op.num_segs, test->ep_context.pipeline_len);

	DT_Tdep_PT_Printf(phead,
			  "    Total Time           : %d.%d sec\n"
			  "    Total Data Exchanged : %d.%d MB\n"
			  "    CPU Utilization      : %d.%d\n"
			  "    Operation Throughput : %d.%d ops/sec\n"
			  "    Bandwidth            : %d.%d MB/sec\n",
			  DT_whole(time_s), DT_hundredths(time_s),
			  DT_whole(mbytes), DT_hundredths(mbytes),
			  DT_whole(stats->cpu_utilization),
			  DT_hundredths(stats->cpu_utilization),
			  DT_whole(ops_per_sec), DT_hundredths(ops_per_sec),
			  DT_whole(bandwidth), DT_hundredths(bandwidth));

	DT_Tdep_PT_Printf(phead, "\nLatency\n");

	if (stats->latency.num) {
		latency = DT_performance_stats_data_print(phead,
							  &stats->latency,
							  cpu_mhz);
	}
	DT_Tdep_PT_Printf(phead, "\n"
			  "Time Per Post\n"
			  "    %u posts without context switches\n",
			  stats->posts_sans_ctxt.num);

	if (stats->posts_sans_ctxt.num) {
		DT_performance_stats_data_print(phead,
						&stats->posts_sans_ctxt,
						cpu_mhz);
	}

	DT_Tdep_PT_Printf(phead, "\n"
			  "    %u posts with context switches\n",
			  stats->posts_with_ctxt.num);

	if (stats->posts_with_ctxt.num) {
		DT_Tdep_PT_Printf(phead, "    %u number of context switches\n",
				  stats->post_ctxt_switch_num);
		DT_performance_stats_data_print(phead,
						&stats->posts_with_ctxt,
						cpu_mhz);
	}

	DT_Tdep_PT_Printf(phead, "\n"
			  "Time Per Reap\n"
			  "    %u reaps without context switches\n",
			  stats->reaps_sans_ctxt.num);

	if (stats->reaps_sans_ctxt.num) {
		DT_performance_stats_data_print(phead,
						&stats->reaps_sans_ctxt,
						cpu_mhz);
	}

	DT_Tdep_PT_Printf(phead, "\n"
			  "    %u reaps with context switches\n",
			  stats->reaps_with_ctxt.num);

	if (stats->reaps_with_ctxt.num) {
		DT_Tdep_PT_Printf(phead, "\n"
				  "    %u number of context switches\n",
				  stats->reap_ctxt_switch_num);

		DT_performance_stats_data_print(phead,
						&stats->reaps_with_ctxt,
						cpu_mhz);
	}

	time_per_post =
	    (int64_t) (stats->posts_sans_ctxt.total_ts +
		       stats->posts_with_ctxt.total_ts) / (cpu_mhz *
							   (stats->
							    posts_sans_ctxt.
							    num +
							    stats->
							    posts_with_ctxt.
							    num));

	time_per_reap =
	    (int64_t) (stats->reaps_sans_ctxt.total_ts +
		       stats->reaps_with_ctxt.total_ts) / (cpu_mhz *
							   (stats->
							    reaps_sans_ctxt.
							    num +
							    stats->
							    reaps_with_ctxt.
							    num));

	DT_Tdep_PT_Printf(phead, "\nNOTE: 1 MB = 1024 KB = 1048576 B \n");
	DT_Tdep_PT_Printf(phead,
			  "---------------------------------------------------------------------\n");
	DT_Tdep_PT_Printf(phead,
			  "raw: %s, %u, %u, %u, %u, %d.%d, %d.%d, %d.%d, %d.%d, %d.%d, %d.%d \n",
			  DT_TransferTypeToString(cmd->op.transfer_type),
			  cmd->num_iterations,
			  cmd->op.seg_size,
			  cmd->op.num_segs,
			  test->ep_context.pipeline_len,
			  DT_whole(stats->cpu_utilization),
			  DT_hundredths(stats->cpu_utilization),
			  DT_whole(ops_per_sec),
			  DT_hundredths(ops_per_sec),
			  DT_whole(bandwidth),
			  DT_hundredths(bandwidth),
			  DT_whole(latency),
			  DT_hundredths(latency),
			  DT_whole(time_per_post),
			  DT_hundredths(time_per_post),
			  DT_whole(time_per_reap),
			  DT_hundredths(time_per_reap));
	DT_Tdep_PT_Printf(phead,
			  "---------------------------------------------------------------------\n");
}
