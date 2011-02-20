/***********************license start***************
 * Copyright (c) 2003-2010  Cavium Networks (support@cavium.com). All rights
 * reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.

 *   * Neither the name of Cavium Networks nor the names of
 *     its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.

 * This Software, including technical data, may be subject to U.S. export  control
 * laws, including the U.S. Export Administration Act and its  associated
 * regulations, and may be subject to export or import  regulations in other
 * countries.

 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND CAVIUM  NETWORKS MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY REPRESENTATION OR
 * DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT DEFECTS, AND CAVIUM
 * SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES OF TITLE,
 * MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF
 * VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. THE ENTIRE  RISK ARISING OUT OF USE OR
 * PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 ***********************license end**************************************/







/**
 * @file
 *
 * Source file for the zip (deflate) block
 *
 * <hr>$Revision: 49448 $<hr>
 */

#include "executive-config.h"
#include "cvmx-config.h"
#include "cvmx.h"
#include "cvmx-cmd-queue.h"
#include "cvmx-zip.h"

#ifdef CVMX_ENABLE_PKO_FUNCTIONS

/**
 * Initialize the ZIP block
 *
 * @return Zero on success, negative on failure
 */
int cvmx_zip_initialize(void)
{
    cvmx_zip_cmd_buf_t zip_cmd_buf;
    cvmx_cmd_queue_result_t result;
    result = cvmx_cmd_queue_initialize(CVMX_CMD_QUEUE_ZIP, 0,
                                       CVMX_FPA_OUTPUT_BUFFER_POOL,
                                       CVMX_FPA_OUTPUT_BUFFER_POOL_SIZE);
    if (result != CVMX_CMD_QUEUE_SUCCESS)
        return -1;

    zip_cmd_buf.u64 = 0;
    zip_cmd_buf.s.dwb = CVMX_FPA_OUTPUT_BUFFER_POOL_SIZE/128;
    zip_cmd_buf.s.pool = CVMX_FPA_OUTPUT_BUFFER_POOL;
    zip_cmd_buf.s.size = CVMX_FPA_OUTPUT_BUFFER_POOL_SIZE/8;
    zip_cmd_buf.s.ptr =  cvmx_ptr_to_phys(cvmx_cmd_queue_buffer(CVMX_CMD_QUEUE_ZIP))>>7;
    cvmx_write_csr(CVMX_ZIP_CMD_BUF, zip_cmd_buf.u64);
    cvmx_write_csr(CVMX_ZIP_ERROR, 1);
    cvmx_read_csr(CVMX_ZIP_CMD_BUF); /* Read to make sure setup is complete */
    return 0;
}

/**
 * Shutdown the ZIP block. ZIP must be idle when
 * this function is called.
 *
 * @return Zero on success, negative on failure
 */
int cvmx_zip_shutdown(void)
{
    cvmx_zip_cmd_ctl_t zip_cmd_ctl;

    if (cvmx_cmd_queue_length(CVMX_CMD_QUEUE_ZIP))
    {
        cvmx_dprintf("ERROR: cvmx_zip_shutdown: ZIP not idle.\n");
        return -1;
    }

    zip_cmd_ctl.u64 = cvmx_read_csr(CVMX_ZIP_CMD_CTL);
    zip_cmd_ctl.s.reset = 1;
    cvmx_write_csr(CVMX_ZIP_CMD_CTL, zip_cmd_ctl.u64);
    cvmx_wait(100);

    cvmx_cmd_queue_shutdown(CVMX_CMD_QUEUE_ZIP);
    return 0;
}

/**
 * Submit a command to the ZIP block
 *
 * @param command Zip command to submit
 *
 * @return Zero on success, negative on failure
 */
int cvmx_zip_submit(cvmx_zip_command_t *command)
{
    cvmx_cmd_queue_result_t result = cvmx_cmd_queue_write(CVMX_CMD_QUEUE_ZIP, 1, 8, command->u64);
    if (result == CVMX_CMD_QUEUE_SUCCESS)
        cvmx_write_csr(CVMX_ADDR_DID(CVMX_FULL_DID(7, 0)), 8);
    return result;
}

#endif

