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
 * Helper functions for common, but complicated tasks.
 *
 * <hr>$Revision: 42150 $<hr>
 */
#include "cvmx.h"
#include "cvmx-bootmem.h"
#include "cvmx-fpa.h"
#include "cvmx-pip.h"
#include "cvmx-pko.h"
#include "cvmx-ipd.h"
#include "cvmx-asx.h"
#include "cvmx-gmx.h"
#include "cvmx-spi.h"
#include "cvmx-sysinfo.h"
#include "cvmx-helper.h"
#include "cvmx-version.h"
#include "cvmx-helper-check-defines.h"
#include "cvmx-helper-board.h"
#include "cvmx-helper-errata.h"

#ifdef CVMX_ENABLE_PKO_FUNCTIONS

/**
 * cvmx_override_pko_queue_priority(int ipd_port, uint64_t
 * priorities[16]) is a function pointer. It is meant to allow
 * customization of the PKO queue priorities based on the port
 * number. Users should set this pointer to a function before
 * calling any cvmx-helper operations.
 */
CVMX_SHARED void (*cvmx_override_pko_queue_priority)(int pko_port, uint64_t priorities[16]) = NULL;

/**
 * cvmx_override_ipd_port_setup(int ipd_port) is a function
 * pointer. It is meant to allow customization of the IPD port
 * setup before packet input/output comes online. It is called
 * after cvmx-helper does the default IPD configuration, but
 * before IPD is enabled. Users should set this pointer to a
 * function before calling any cvmx-helper operations.
 */
CVMX_SHARED void (*cvmx_override_ipd_port_setup)(int ipd_port) = NULL;

/* Port count per interface */
static CVMX_SHARED int interface_port_count[4] = {0,0,0,0};
/* Port last configured link info index by IPD/PKO port */
static CVMX_SHARED cvmx_helper_link_info_t port_link_info[CVMX_PIP_NUM_INPUT_PORTS];


/**
 * Return the number of interfaces the chip has. Each interface
 * may have multiple ports. Most chips support two interfaces,
 * but the CNX0XX and CNX1XX are exceptions. These only support
 * one interface.
 *
 * @return Number of interfaces on chip
 */
int cvmx_helper_get_number_of_interfaces(void)
{
    switch (cvmx_sysinfo_get()->board_type) {
#if defined(OCTEON_VENDOR_LANNER)
	case CVMX_BOARD_TYPE_CUST_LANNER_MR955:
	    return 2;
#endif
	default:
	    break;
    }

    if (OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN52XX))
        return 4;
    else
        return 3;
}


/**
 * Return the number of ports on an interface. Depending on the
 * chip and configuration, this can be 1-16. A value of 0
 * specifies that the interface doesn't exist or isn't usable.
 *
 * @param interface Interface to get the port count for
 *
 * @return Number of ports on interface. Can be Zero.
 */
int cvmx_helper_ports_on_interface(int interface)
{
    return interface_port_count[interface];
}


/**
 * Get the operating mode of an interface. Depending on the Octeon
 * chip and configuration, this function returns an enumeration
 * of the type of packet I/O supported by an interface.
 *
 * @param interface Interface to probe
 *
 * @return Mode of the interface. Unknown or unsupported interfaces return
 *         DISABLED.
 */
cvmx_helper_interface_mode_t cvmx_helper_interface_get_mode(int interface)
{
    cvmx_gmxx_inf_mode_t mode;
    if (interface == 2)
        return CVMX_HELPER_INTERFACE_MODE_NPI;

    if (interface == 3)
    {
        if (OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN52XX))
            return CVMX_HELPER_INTERFACE_MODE_LOOP;
        else
            return CVMX_HELPER_INTERFACE_MODE_DISABLED;
    }

    if (interface == 0 && cvmx_sysinfo_get()->board_type == CVMX_BOARD_TYPE_CN3005_EVB_HS5 && cvmx_sysinfo_get()->board_rev_major == 1)
    {
        /* Lie about interface type of CN3005 board.  This board has a switch on port 1 like
        ** the other evaluation boards, but it is connected over RGMII instead of GMII.  Report
        ** GMII mode so that the speed is forced to 1 Gbit full duplex.  Other than some initial configuration
        ** (which does not use the output of this function) there is no difference in setup between GMII and RGMII modes.
        */
        return CVMX_HELPER_INTERFACE_MODE_GMII;
    }

    /* Interface 1 is always disabled on CN31XX and CN30XX */
    if ((interface == 1) && (OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN52XX)))
        return CVMX_HELPER_INTERFACE_MODE_DISABLED;

    mode.u64 = cvmx_read_csr(CVMX_GMXX_INF_MODE(interface));

    if (OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        switch(mode.cn56xx.mode)
        {
            case 0: return CVMX_HELPER_INTERFACE_MODE_DISABLED;
            case 1: return CVMX_HELPER_INTERFACE_MODE_XAUI;
            case 2: return CVMX_HELPER_INTERFACE_MODE_SGMII;
            case 3: return CVMX_HELPER_INTERFACE_MODE_PICMG;
            default:return CVMX_HELPER_INTERFACE_MODE_DISABLED;
        }
    }
    else
    {
        if (!mode.s.en)
            return CVMX_HELPER_INTERFACE_MODE_DISABLED;

        if (mode.s.type)
        {
            if (OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX))
                return CVMX_HELPER_INTERFACE_MODE_SPI;
            else
                return CVMX_HELPER_INTERFACE_MODE_GMII;
        }
        else
            return CVMX_HELPER_INTERFACE_MODE_RGMII;
    }
}


/**
 * @INTERNAL
 * Configure the IPD/PIP tagging and QoS options for a specific
 * port. This function determines the POW work queue entry
 * contents for a port. The setup performed here is controlled by
 * the defines in executive-config.h.
 *
 * @param ipd_port Port to configure. This follows the IPD numbering, not the
 *                 per interface numbering
 *
 * @return Zero on success, negative on failure
 */
static int __cvmx_helper_port_setup_ipd(int ipd_port)
{
    cvmx_pip_port_cfg_t port_config;
    cvmx_pip_port_tag_cfg_t tag_config;

    port_config.u64 = cvmx_read_csr(CVMX_PIP_PRT_CFGX(ipd_port));
    tag_config.u64 = cvmx_read_csr(CVMX_PIP_PRT_TAGX(ipd_port));

    /* Have each port go to a different POW queue */
    port_config.s.qos = ipd_port & 0x7;

    /* Process the headers and place the IP header in the work queue */
    port_config.s.mode = CVMX_HELPER_INPUT_PORT_SKIP_MODE;

    tag_config.s.ip6_src_flag  = CVMX_HELPER_INPUT_TAG_IPV6_SRC_IP;
    tag_config.s.ip6_dst_flag  = CVMX_HELPER_INPUT_TAG_IPV6_DST_IP;
    tag_config.s.ip6_sprt_flag = CVMX_HELPER_INPUT_TAG_IPV6_SRC_PORT;
    tag_config.s.ip6_dprt_flag = CVMX_HELPER_INPUT_TAG_IPV6_DST_PORT;
    tag_config.s.ip6_nxth_flag = CVMX_HELPER_INPUT_TAG_IPV6_NEXT_HEADER;
    tag_config.s.ip4_src_flag  = CVMX_HELPER_INPUT_TAG_IPV4_SRC_IP;
    tag_config.s.ip4_dst_flag  = CVMX_HELPER_INPUT_TAG_IPV4_DST_IP;
    tag_config.s.ip4_sprt_flag = CVMX_HELPER_INPUT_TAG_IPV4_SRC_PORT;
    tag_config.s.ip4_dprt_flag = CVMX_HELPER_INPUT_TAG_IPV4_DST_PORT;
    tag_config.s.ip4_pctl_flag = CVMX_HELPER_INPUT_TAG_IPV4_PROTOCOL;
    tag_config.s.inc_prt_flag  = CVMX_HELPER_INPUT_TAG_INPUT_PORT;
    tag_config.s.tcp6_tag_type = CVMX_HELPER_INPUT_TAG_TYPE;
    tag_config.s.tcp4_tag_type = CVMX_HELPER_INPUT_TAG_TYPE;
    tag_config.s.ip6_tag_type = CVMX_HELPER_INPUT_TAG_TYPE;
    tag_config.s.ip4_tag_type = CVMX_HELPER_INPUT_TAG_TYPE;
    tag_config.s.non_tag_type = CVMX_HELPER_INPUT_TAG_TYPE;
    /* Put all packets in group 0. Other groups can be used by the app */
    tag_config.s.grp = 0;

    cvmx_pip_config_port(ipd_port, port_config, tag_config);

    /* Give the user a chance to override our setting for each port */
    if (cvmx_override_ipd_port_setup)
        cvmx_override_ipd_port_setup(ipd_port);

    return 0;
}


/**
 * This function probes an interface to determine the actual
 * number of hardware ports connected to it. It doesn't setup the
 * ports or enable them. The main goal here is to set the global
 * interface_port_count[interface] correctly. Hardware setup of the
 * ports will be performed later.
 *
 * @param interface Interface to probe
 *
 * @return Zero on success, negative on failure
 */
int cvmx_helper_interface_probe(int interface)
{
    /* At this stage in the game we don't want packets to be moving yet.
        The following probe calls should perform hardware setup
        needed to determine port counts. Receive must still be disabled */
    switch (cvmx_helper_interface_get_mode(interface))
    {
        /* These types don't support ports to IPD/PKO */
        case CVMX_HELPER_INTERFACE_MODE_DISABLED:
        case CVMX_HELPER_INTERFACE_MODE_PCIE:
            interface_port_count[interface] = 0;
            break;
        /* XAUI is a single high speed port */
        case CVMX_HELPER_INTERFACE_MODE_XAUI:
            interface_port_count[interface] = __cvmx_helper_xaui_probe(interface);
            break;
        /* RGMII/GMII/MII are all treated about the same. Most functions
            refer to these ports as RGMII */
        case CVMX_HELPER_INTERFACE_MODE_RGMII:
        case CVMX_HELPER_INTERFACE_MODE_GMII:
            interface_port_count[interface] = __cvmx_helper_rgmii_probe(interface);
            break;
        /* SPI4 can have 1-16 ports depending on the device at the other end */
        case CVMX_HELPER_INTERFACE_MODE_SPI:
            interface_port_count[interface] = __cvmx_helper_spi_probe(interface);
            break;
        /* SGMII can have 1-4 ports depending on how many are hooked up */
        case CVMX_HELPER_INTERFACE_MODE_SGMII:
        case CVMX_HELPER_INTERFACE_MODE_PICMG:
            interface_port_count[interface] = __cvmx_helper_sgmii_probe(interface);
            break;
        /* PCI target Network Packet Interface */
        case CVMX_HELPER_INTERFACE_MODE_NPI:
            interface_port_count[interface] = __cvmx_helper_npi_probe(interface);
            break;
        /* Special loopback only ports. These are not the same as other ports
            in loopback mode */
        case CVMX_HELPER_INTERFACE_MODE_LOOP:
            interface_port_count[interface] = __cvmx_helper_loop_probe(interface);
            break;
    }

    interface_port_count[interface] = __cvmx_helper_board_interface_probe(interface, interface_port_count[interface]);

    /* Make sure all global variables propagate to other cores */
    CVMX_SYNCWS;

    return 0;
}


/**
 * @INTERNAL
 * Setup the IPD/PIP for the ports on an interface. Packet
 * classification and tagging are set for every port on the
 * interface. The number of ports on the interface must already
 * have been probed.
 *
 * @param interface Interface to setup IPD/PIP for
 *
 * @return Zero on success, negative on failure
 */
static int __cvmx_helper_interface_setup_ipd(int interface)
{
    int ipd_port = cvmx_helper_get_ipd_port(interface, 0);
    int num_ports = interface_port_count[interface];

    while (num_ports--)
    {
        __cvmx_helper_port_setup_ipd(ipd_port);
        ipd_port++;
    }
    return 0;
}


/**
 * @INTERNAL
 * Setup global setting for IPD/PIP not related to a specific
 * interface or port. This must be called before IPD is enabled.
 *
 * @return Zero on success, negative on failure.
 */
static int __cvmx_helper_global_setup_ipd(void)
{
    /* Setup the global packet input options */
    cvmx_ipd_config(CVMX_FPA_PACKET_POOL_SIZE/8,
                    CVMX_HELPER_FIRST_MBUFF_SKIP/8,
                    CVMX_HELPER_NOT_FIRST_MBUFF_SKIP/8,
                    (CVMX_HELPER_FIRST_MBUFF_SKIP+8) / 128, /* The +8 is to account for the next ptr */
                    (CVMX_HELPER_NOT_FIRST_MBUFF_SKIP+8) / 128, /* The +8 is to account for the next ptr */
                    CVMX_FPA_WQE_POOL,
                    CVMX_IPD_OPC_MODE_STT,
                    CVMX_HELPER_ENABLE_BACK_PRESSURE);
    return 0;
}


/**
 * @INTERNAL
 * Setup the PKO for the ports on an interface. The number of
 * queues per port and the priority of each PKO output queue
 * is set here. PKO must be disabled when this function is called.
 *
 * @param interface Interface to setup PKO for
 *
 * @return Zero on success, negative on failure
 */
static int __cvmx_helper_interface_setup_pko(int interface)
{
    /* Each packet output queue has an associated priority. The higher the
        priority, the more often it can send a packet. A priority of 8 means
        it can send in all 8 rounds of contention. We're going to make each
        queue one less than the last.
        The vector of priorities has been extended to support CN5xxx CPUs,
        where up to 16 queues can be associated to a port.
        To keep backward compatibility we don't change the initial 8
        priorities and replicate them in the second half.
        With per-core PKO queues (PKO lockless operation) all queues have
        the same priority. */
    uint64_t priorities[16] = {8,7,6,5,4,3,2,1,8,7,6,5,4,3,2,1};

    /* Setup the IPD/PIP and PKO for the ports discovered above. Here packet
        classification, tagging and output priorities are set */
    int ipd_port = cvmx_helper_get_ipd_port(interface, 0);
    int num_ports = interface_port_count[interface];
    while (num_ports--)
    {
        /* Give the user a chance to override the per queue priorities */
        if (cvmx_override_pko_queue_priority)
            cvmx_override_pko_queue_priority(ipd_port, priorities);

        cvmx_pko_config_port(ipd_port, cvmx_pko_get_base_queue_per_core(ipd_port, 0),
                             cvmx_pko_get_num_queues(ipd_port), priorities);
        ipd_port++;
    }
    return 0;
}


/**
 * @INTERNAL
 * Setup global setting for PKO not related to a specific
 * interface or port. This must be called before PKO is enabled.
 *
 * @return Zero on success, negative on failure.
 */
static int __cvmx_helper_global_setup_pko(void)
{
    /* Disable tagwait FAU timeout. This needs to be done before anyone might
        start packet output using tags */
    cvmx_iob_fau_timeout_t fau_to;
    fau_to.u64 = 0;
    fau_to.s.tout_val = 0xfff;
    fau_to.s.tout_enb = 0;
    cvmx_write_csr(CVMX_IOB_FAU_TIMEOUT, fau_to.u64);
    return 0;
}


/**
 * @INTERNAL
 * Setup global backpressure setting.
 *
 * @return Zero on success, negative on failure
 */
static int __cvmx_helper_global_setup_backpressure(void)
{
#if CVMX_HELPER_DISABLE_RGMII_BACKPRESSURE
    /* Disable backpressure if configured to do so */
    /* Disable backpressure (pause frame) generation */
    int num_interfaces = cvmx_helper_get_number_of_interfaces();
    int interface;
    for (interface=0; interface<num_interfaces; interface++)
    {
        switch (cvmx_helper_interface_get_mode(interface))
        {
            case CVMX_HELPER_INTERFACE_MODE_DISABLED:
            case CVMX_HELPER_INTERFACE_MODE_PCIE:
            case CVMX_HELPER_INTERFACE_MODE_NPI:
            case CVMX_HELPER_INTERFACE_MODE_LOOP:
            case CVMX_HELPER_INTERFACE_MODE_XAUI:
                break;
            case CVMX_HELPER_INTERFACE_MODE_RGMII:
            case CVMX_HELPER_INTERFACE_MODE_GMII:
            case CVMX_HELPER_INTERFACE_MODE_SPI:
            case CVMX_HELPER_INTERFACE_MODE_SGMII:
            case CVMX_HELPER_INTERFACE_MODE_PICMG:
                cvmx_gmx_set_backpressure_override(interface, 0xf);
                break;
        }
    }
    //cvmx_dprintf("Disabling backpressure\n");
#endif

    return 0;
}


/**
 * @INTERNAL
 * Enable packet input/output from the hardware. This function is
 * called after all internal setup is complete and IPD is enabled.
 * After this function completes, packets will be accepted from the
 * hardware ports. PKO should still be disabled to make sure packets
 * aren't sent out partially setup hardware.
 *
 * @param interface Interface to enable
 *
 * @return Zero on success, negative on failure
 */
static int __cvmx_helper_packet_hardware_enable(int interface)
{
    int result = 0;
    switch (cvmx_helper_interface_get_mode(interface))
    {
        /* These types don't support ports to IPD/PKO */
        case CVMX_HELPER_INTERFACE_MODE_DISABLED:
        case CVMX_HELPER_INTERFACE_MODE_PCIE:
            /* Nothing to do */
            break;
        /* XAUI is a single high speed port */
        case CVMX_HELPER_INTERFACE_MODE_XAUI:
            result = __cvmx_helper_xaui_enable(interface);
            break;
        /* RGMII/GMII/MII are all treated about the same. Most functions
            refer to these ports as RGMII */
        case CVMX_HELPER_INTERFACE_MODE_RGMII:
        case CVMX_HELPER_INTERFACE_MODE_GMII:
            result = __cvmx_helper_rgmii_enable(interface);
            break;
        /* SPI4 can have 1-16 ports depending on the device at the other end */
        case CVMX_HELPER_INTERFACE_MODE_SPI:
            result = __cvmx_helper_spi_enable(interface);
            break;
        /* SGMII can have 1-4 ports depending on how many are hooked up */
        case CVMX_HELPER_INTERFACE_MODE_SGMII:
        case CVMX_HELPER_INTERFACE_MODE_PICMG:
            result = __cvmx_helper_sgmii_enable(interface);
            break;
        /* PCI target Network Packet Interface */
        case CVMX_HELPER_INTERFACE_MODE_NPI:
            result = __cvmx_helper_npi_enable(interface);
            break;
        /* Special loopback only ports. These are not the same as other ports
            in loopback mode */
        case CVMX_HELPER_INTERFACE_MODE_LOOP:
            result = __cvmx_helper_loop_enable(interface);
            break;
    }
    result |= __cvmx_helper_board_hardware_enable(interface);
    return result;
}


/**
 * Called after all internal packet IO paths are setup. This
 * function enables IPD/PIP and begins packet input and output.
 *
 * @return Zero on success, negative on failure
 */
int cvmx_helper_ipd_and_packet_input_enable(void)
{
    int num_interfaces;
    int interface;

    /* Enable IPD */
    cvmx_ipd_enable();

    /* Time to enable hardware ports packet input and output. Note that at this
        point IPD/PIP must be fully functional and PKO must be disabled */
    num_interfaces = cvmx_helper_get_number_of_interfaces();
    for (interface=0; interface<num_interfaces; interface++)
    {
        if (cvmx_helper_ports_on_interface(interface) > 0)
        {
            //cvmx_dprintf("Enabling packet I/O on interface %d\n", interface);
            __cvmx_helper_packet_hardware_enable(interface);
        }
    }

    /* Finally enable PKO now that the entire path is up and running */
    cvmx_pko_enable();

    if ((OCTEON_IS_MODEL(OCTEON_CN31XX_PASS1) || OCTEON_IS_MODEL(OCTEON_CN30XX_PASS1)) &&
        (cvmx_sysinfo_get()->board_type != CVMX_BOARD_TYPE_SIM))
        __cvmx_helper_errata_fix_ipd_ptr_alignment();
    return 0;
}


/**
 * Initialize the PIP, IPD, and PKO hardware to support
 * simple priority based queues for the ethernet ports. Each
 * port is configured with a number of priority queues based
 * on CVMX_PKO_QUEUES_PER_PORT_* where each queue is lower
 * priority than the previous.
 *
 * @return Zero on success, non-zero on failure
 */
int cvmx_helper_initialize_packet_io_global(void)
{
    int result = 0;
    int interface;
    cvmx_l2c_cfg_t l2c_cfg;
    cvmx_smix_en_t smix_en;
    const int num_interfaces = cvmx_helper_get_number_of_interfaces();

    /* CN52XX pass 1: Due to a bug in 2nd order CDR, it needs to be disabled */
    if (OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_0))
        __cvmx_helper_errata_qlm_disable_2nd_order_cdr(1);

    /* Tell L2 to give the IOB statically higher priority compared to the
        cores. This avoids conditions where IO blocks might be starved under
        very high L2 loads */
    l2c_cfg.u64 = cvmx_read_csr(CVMX_L2C_CFG);
    l2c_cfg.s.lrf_arb_mode = 0;
    l2c_cfg.s.rfb_arb_mode = 0;
    cvmx_write_csr(CVMX_L2C_CFG, l2c_cfg.u64);

    /* Make sure SMI/MDIO is enabled so we can query PHYs */
    smix_en.u64 = cvmx_read_csr(CVMX_SMIX_EN(0));
    if (!smix_en.s.en)
    {
        smix_en.s.en = 1;
        cvmx_write_csr(CVMX_SMIX_EN(0), smix_en.u64);
    }

    /* Newer chips actually have two SMI/MDIO interfaces */
    if (!OCTEON_IS_MODEL(OCTEON_CN3XXX) &&
        !OCTEON_IS_MODEL(OCTEON_CN58XX) &&
        !OCTEON_IS_MODEL(OCTEON_CN50XX))
    {
        smix_en.u64 = cvmx_read_csr(CVMX_SMIX_EN(1));
        if (!smix_en.s.en)
        {
            smix_en.s.en = 1;
            cvmx_write_csr(CVMX_SMIX_EN(1), smix_en.u64);
        }
    }

    cvmx_pko_initialize_global();
    for (interface=0; interface<num_interfaces; interface++)
    {
        result |= cvmx_helper_interface_probe(interface);
        if (cvmx_helper_ports_on_interface(interface) > 0)
            cvmx_dprintf("Interface %d has %d ports (%s)\n",
                     interface, cvmx_helper_ports_on_interface(interface),
                     cvmx_helper_interface_mode_to_string(cvmx_helper_interface_get_mode(interface)));
        result |= __cvmx_helper_interface_setup_ipd(interface);
        result |= __cvmx_helper_interface_setup_pko(interface);
    }

    result |= __cvmx_helper_global_setup_ipd();
    result |= __cvmx_helper_global_setup_pko();

    /* Enable any flow control and backpressure */
    result |= __cvmx_helper_global_setup_backpressure();

#if CVMX_HELPER_ENABLE_IPD
    result |= cvmx_helper_ipd_and_packet_input_enable();
#endif
    return result;
}


/**
 * Does core local initialization for packet io
 *
 * @return Zero on success, non-zero on failure
 */
int cvmx_helper_initialize_packet_io_local(void)
{
    return cvmx_pko_initialize_local();
}


/**
 * Auto configure an IPD/PKO port link state and speed. This
 * function basically does the equivalent of:
 * cvmx_helper_link_set(ipd_port, cvmx_helper_link_get(ipd_port));
 *
 * @param ipd_port IPD/PKO port to auto configure
 *
 * @return Link state after configure
 */
cvmx_helper_link_info_t cvmx_helper_link_autoconf(int ipd_port)
{
    cvmx_helper_link_info_t link_info;
    int interface = cvmx_helper_get_interface_num(ipd_port);
    int index = cvmx_helper_get_interface_index_num(ipd_port);

    if (index >= cvmx_helper_ports_on_interface(interface))
    {
        link_info.u64 = 0;
        return link_info;
    }

    link_info = cvmx_helper_link_get(ipd_port);
    if (link_info.u64 ==  port_link_info[ipd_port].u64)
        return link_info;

    /* If we fail to set the link speed, port_link_info will not change */
    cvmx_helper_link_set(ipd_port, link_info);

    /* port_link_info should be the current value, which will be different
        than expect if cvmx_helper_link_set() failed */
    return port_link_info[ipd_port];
}


/**
 * Return the link state of an IPD/PKO port as returned by
 * auto negotiation. The result of this function may not match
 * Octeon's link config if auto negotiation has changed since
 * the last call to cvmx_helper_link_set().
 *
 * @param ipd_port IPD/PKO port to query
 *
 * @return Link state
 */
cvmx_helper_link_info_t cvmx_helper_link_get(int ipd_port)
{
    cvmx_helper_link_info_t result;
    int interface = cvmx_helper_get_interface_num(ipd_port);
    int index = cvmx_helper_get_interface_index_num(ipd_port);

    /* The default result will be a down link unless the code below
        changes it */
    result.u64 = 0;

    if (index >= cvmx_helper_ports_on_interface(interface))
        return result;

    switch (cvmx_helper_interface_get_mode(interface))
    {
        case CVMX_HELPER_INTERFACE_MODE_DISABLED:
        case CVMX_HELPER_INTERFACE_MODE_PCIE:
            /* Network links are not supported */
            break;
        case CVMX_HELPER_INTERFACE_MODE_XAUI:
            result = __cvmx_helper_xaui_link_get(ipd_port);
            break;
        case CVMX_HELPER_INTERFACE_MODE_GMII:
            if (index == 0)
                result = __cvmx_helper_rgmii_link_get(ipd_port);
            else
            {
                result.s.full_duplex = 1;
                result.s.link_up = 1;
                result.s.speed = 1000;
            }
            break;
        case CVMX_HELPER_INTERFACE_MODE_RGMII:
            result = __cvmx_helper_rgmii_link_get(ipd_port);
            break;
        case CVMX_HELPER_INTERFACE_MODE_SPI:
            result = __cvmx_helper_spi_link_get(ipd_port);
            break;
        case CVMX_HELPER_INTERFACE_MODE_SGMII:
        case CVMX_HELPER_INTERFACE_MODE_PICMG:
            result = __cvmx_helper_sgmii_link_get(ipd_port);
            break;
        case CVMX_HELPER_INTERFACE_MODE_NPI:
        case CVMX_HELPER_INTERFACE_MODE_LOOP:
            /* Network links are not supported */
            break;
    }
    return result;
}


/**
 * Configure an IPD/PKO port for the specified link state. This
 * function does not influence auto negotiation at the PHY level.
 * The passed link state must always match the link state returned
 * by cvmx_helper_link_get(). It is normally best to use
 * cvmx_helper_link_autoconf() instead.
 *
 * @param ipd_port  IPD/PKO port to configure
 * @param link_info The new link state
 *
 * @return Zero on success, negative on failure
 */
int cvmx_helper_link_set(int ipd_port, cvmx_helper_link_info_t link_info)
{
    int result = -1;
    int interface = cvmx_helper_get_interface_num(ipd_port);
    int index = cvmx_helper_get_interface_index_num(ipd_port);

    if (index >= cvmx_helper_ports_on_interface(interface))
        return -1;

    switch (cvmx_helper_interface_get_mode(interface))
    {
        case CVMX_HELPER_INTERFACE_MODE_DISABLED:
        case CVMX_HELPER_INTERFACE_MODE_PCIE:
            break;
        case CVMX_HELPER_INTERFACE_MODE_XAUI:
            result = __cvmx_helper_xaui_link_set(ipd_port, link_info);
            break;
        /* RGMII/GMII/MII are all treated about the same. Most functions
            refer to these ports as RGMII */
        case CVMX_HELPER_INTERFACE_MODE_RGMII:
        case CVMX_HELPER_INTERFACE_MODE_GMII:
            result = __cvmx_helper_rgmii_link_set(ipd_port, link_info);
            break;
        case CVMX_HELPER_INTERFACE_MODE_SPI:
            result = __cvmx_helper_spi_link_set(ipd_port, link_info);
            break;
        case CVMX_HELPER_INTERFACE_MODE_SGMII:
        case CVMX_HELPER_INTERFACE_MODE_PICMG:
            result = __cvmx_helper_sgmii_link_set(ipd_port, link_info);
            break;
        case CVMX_HELPER_INTERFACE_MODE_NPI:
        case CVMX_HELPER_INTERFACE_MODE_LOOP:
            break;
    }
    /* Set the port_link_info here so that the link status is updated
       no matter how cvmx_helper_link_set is called. We don't change
       the value if link_set failed */
    if (result == 0)
        port_link_info[ipd_port].u64 = link_info.u64;
    return result;
}


/**
 * Configure a port for internal and/or external loopback. Internal loopback
 * causes packets sent by the port to be received by Octeon. External loopback
 * causes packets received from the wire to sent out again.
 *
 * @param ipd_port IPD/PKO port to loopback.
 * @param enable_internal
 *                 Non zero if you want internal loopback
 * @param enable_external
 *                 Non zero if you want external loopback
 *
 * @return Zero on success, negative on failure.
 */
int cvmx_helper_configure_loopback(int ipd_port, int enable_internal, int enable_external)
{
    int result = -1;
    int interface = cvmx_helper_get_interface_num(ipd_port);
    int index = cvmx_helper_get_interface_index_num(ipd_port);

    if (index >= cvmx_helper_ports_on_interface(interface))
        return -1;

    switch (cvmx_helper_interface_get_mode(interface))
    {
        case CVMX_HELPER_INTERFACE_MODE_DISABLED:
        case CVMX_HELPER_INTERFACE_MODE_PCIE:
        case CVMX_HELPER_INTERFACE_MODE_SPI:
        case CVMX_HELPER_INTERFACE_MODE_NPI:
        case CVMX_HELPER_INTERFACE_MODE_LOOP:
            break;
        case CVMX_HELPER_INTERFACE_MODE_XAUI:
            result = __cvmx_helper_xaui_configure_loopback(ipd_port, enable_internal, enable_external);
            break;
        case CVMX_HELPER_INTERFACE_MODE_RGMII:
        case CVMX_HELPER_INTERFACE_MODE_GMII:
            result = __cvmx_helper_rgmii_configure_loopback(ipd_port, enable_internal, enable_external);
            break;
        case CVMX_HELPER_INTERFACE_MODE_SGMII:
        case CVMX_HELPER_INTERFACE_MODE_PICMG:
            result = __cvmx_helper_sgmii_configure_loopback(ipd_port, enable_internal, enable_external);
            break;
    }
    return result;
}

#endif /* CVMX_ENABLE_PKO_FUNCTIONS */
