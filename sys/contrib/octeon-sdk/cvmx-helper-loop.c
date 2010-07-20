/***********************license start***************
 *  Copyright (c) 2003-2008 Cavium Networks (support@cavium.com). All rights
 *  reserved.
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *
 *      * Neither the name of Cavium Networks nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written
 *        permission.
 *
 *  TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 *  AND WITH ALL FAULTS AND CAVIUM NETWORKS MAKES NO PROMISES, REPRESENTATIONS
 *  OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
 *  RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY
 *  REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT
 *  DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES
 *  OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR
 *  PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET
 *  POSSESSION OR CORRESPONDENCE TO DESCRIPTION.  THE ENTIRE RISK ARISING OUT
 *  OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 *
 *
 *  For any questions regarding licensing please contact marketing@caviumnetworks.com
 *
 ***********************license end**************************************/






/**
 * @file
 *
 * Functions for LOOP initialization, configuration,
 * and monitoring.
 *
 * <hr>$Revision: 41586 $<hr>
 */
#include "executive-config.h"
#include "cvmx-config.h"
#ifdef CVMX_ENABLE_PKO_FUNCTIONS

#include "cvmx.h"
#include "cvmx-helper.h"


/**
 * @INTERNAL
 * Probe a LOOP interface and determine the number of ports
 * connected to it. The LOOP interface should still be down
 * after this call.
 *
 * @param interface Interface to probe
 *
 * @return Number of ports on the interface. Zero to disable.
 */
int __cvmx_helper_loop_probe(int interface)
{
    cvmx_ipd_sub_port_fcs_t ipd_sub_port_fcs;
    int num_ports = 4;
    int port;

    /* We need to disable length checking so packet < 64 bytes and jumbo
        frames don't get errors */
    for (port=0; port<num_ports; port++)
    {
        cvmx_pip_port_cfg_t port_cfg;
        int ipd_port = cvmx_helper_get_ipd_port(interface, port);
        port_cfg.u64 = cvmx_read_csr(CVMX_PIP_PRT_CFGX(ipd_port));
        port_cfg.s.maxerr_en = 0;
        port_cfg.s.minerr_en = 0;
        cvmx_write_csr(CVMX_PIP_PRT_CFGX(ipd_port), port_cfg.u64);
    }

    /* Disable FCS stripping for loopback ports */
    ipd_sub_port_fcs.u64 = cvmx_read_csr(CVMX_IPD_SUB_PORT_FCS);
    ipd_sub_port_fcs.s.port_bit2 = 0;
    cvmx_write_csr(CVMX_IPD_SUB_PORT_FCS, ipd_sub_port_fcs.u64);
    return num_ports;
}


/**
 * @INTERNAL
 * Bringup and enable a LOOP interface. After this call packet
 * I/O should be fully functional. This is called with IPD
 * enabled but PKO disabled.
 *
 * @param interface Interface to bring up
 *
 * @return Zero on success, negative on failure
 */
int __cvmx_helper_loop_enable(int interface)
{
    /* Do nothing. */
    return 0;
}

#endif /* CVMX_ENABLE_PKO_FUNCTIONS */

