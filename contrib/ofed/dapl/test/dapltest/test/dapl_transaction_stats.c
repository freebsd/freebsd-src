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

void
DT_init_transaction_stats(Transaction_Stats_t * transaction_stats,
			  unsigned int num)
{
	DT_Mdep_LockInit(&transaction_stats->lock);

	transaction_stats->wait_count = num;
	transaction_stats->num_ops = 0;
	transaction_stats->time_ms = 0;
	transaction_stats->bytes_send = 0;
	transaction_stats->bytes_recv = 0;
	transaction_stats->bytes_rdma_read = 0;
	transaction_stats->bytes_rdma_write = 0;
}

void
DT_transaction_stats_set_ready(DT_Tdep_Print_Head * phead,
			       Transaction_Stats_t * transaction_stats)
{
	DT_Mdep_Lock(&transaction_stats->lock);
	transaction_stats->wait_count--;

	DT_Tdep_PT_Debug(1,
			 (phead,
			  "Received Sync Message from server (%d left)\n",
			  transaction_stats->wait_count));
	DT_Mdep_Unlock(&transaction_stats->lock);
}

bool
DT_transaction_stats_wait_for_all(DT_Tdep_Print_Head * phead,
				  Transaction_Stats_t * transaction_stats)
{
	unsigned int loop_count;
	loop_count = 100 * 10;	/* 100 * 10ms * 10  = 10 seconds */
	while (transaction_stats->wait_count != 0 && loop_count != 0) {
		DT_Mdep_Sleep(10);
		loop_count--;
	}
	if (loop_count == 0) {
		DT_Tdep_PT_Printf(phead,
				  "FAIL: %d Server test connections did not report ready.\n",
				  transaction_stats->wait_count);
		return false;
	}
	return true;
}

/*
 *
 */
void
DT_update_transaction_stats(Transaction_Stats_t * transaction_stats,
			    unsigned int num_ops,
			    unsigned int time_ms,
			    unsigned int bytes_send,
			    unsigned int bytes_recv,
			    unsigned int bytes_rdma_read,
			    unsigned int bytes_rdma_write)
{
	DT_Mdep_Lock(&transaction_stats->lock);

	/* look for the longest time... */
	if (time_ms > transaction_stats->time_ms) {
		transaction_stats->time_ms = time_ms;
	}

	transaction_stats->num_ops += num_ops;
	transaction_stats->bytes_send += bytes_send;
	transaction_stats->bytes_recv += bytes_recv;
	transaction_stats->bytes_rdma_read += bytes_rdma_read;
	transaction_stats->bytes_rdma_write += bytes_rdma_write;
	DT_Mdep_Unlock(&transaction_stats->lock);
}

/*
 *
 */
void
DT_print_transaction_stats(DT_Tdep_Print_Head * phead,
			   Transaction_Stats_t * transaction_stats,
			   unsigned int num_threads, unsigned int num_EPs)
{
	double time_s;
	double mbytes_send;
	double mbytes_recv;
	double mbytes_rdma_read;
	double mbytes_rdma_write;
	int total_ops;
	DT_Mdep_Lock(&transaction_stats->lock);
	time_s = (double)(transaction_stats->time_ms) / 1000;
	if (time_s == 0.0) {
		DT_Tdep_PT_Printf(phead,
				  "----- Test completed successfully, but cannot calculate stats as not\n"
				  "----- enough time has lapsed.\n"
				  "----- Try running the test with more iterations.\n");
		goto unlock_and_return;
	}
	mbytes_send = (double)transaction_stats->bytes_send / 1000 / 1000;
	mbytes_recv = (double)transaction_stats->bytes_recv / 1000 / 1000;
	mbytes_rdma_read =
	    (double)transaction_stats->bytes_rdma_read / 1000 / 1000;
	mbytes_rdma_write =
	    (double)transaction_stats->bytes_rdma_write / 1000 / 1000;
	total_ops = transaction_stats->num_ops;

	if (0 == total_ops) {
		DT_Tdep_PT_Printf(phead,
				  "----- Test completed successfully, but no operations!\n");
		goto unlock_and_return;
	}

	DT_Tdep_PT_Printf(phead, "----- Stats ---- : %u threads, %u EPs\n",
			  num_threads, num_EPs);
	DT_Tdep_PT_Printf(phead, "Total WQE        : %7d.%02d WQE/Sec\n",
			  whole(total_ops / time_s),
			  hundredths(total_ops / time_s));
	DT_Tdep_PT_Printf(phead, "Total Time       : %7d.%02d sec\n",
			  whole(time_s), hundredths(time_s));
	DT_Tdep_PT_Printf(phead,
			  "Total Send       : %7d.%02d MB - %7d.%02d MB/Sec\n",
			  whole(mbytes_send), hundredths(mbytes_send),
			  whole(mbytes_send / time_s),
			  hundredths(mbytes_send / time_s));
	DT_Tdep_PT_Printf(phead,
			  "Total Recv       : %7d.%02d MB - %7d.%02d MB/Sec\n",
			  whole(mbytes_recv), hundredths(mbytes_recv),
			  whole(mbytes_recv / time_s),
			  hundredths(mbytes_recv / time_s));
	DT_Tdep_PT_Printf(phead,
			  "Total RDMA Read  : %7d.%02d MB - %7d.%02d MB/Sec\n",
			  whole(mbytes_rdma_read), hundredths(mbytes_rdma_read),
			  whole(mbytes_rdma_read / time_s),
			  hundredths(mbytes_rdma_read / time_s));
	DT_Tdep_PT_Printf(phead,
			  "Total RDMA Write : %7d.%02d MB - %7d.%02d MB/Sec\n",
			  whole(mbytes_rdma_write),
			  hundredths(mbytes_rdma_write),
			  whole(mbytes_rdma_write / time_s),
			  hundredths(mbytes_rdma_write / time_s));

      unlock_and_return:
	DT_Mdep_Unlock(&transaction_stats->lock);
}
