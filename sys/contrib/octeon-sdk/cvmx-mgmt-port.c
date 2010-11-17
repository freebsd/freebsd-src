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
 * Support functions for managing the MII management port
 *
 * <hr>$Revision: 42151 $<hr>
 */
#include "cvmx.h"
#include "cvmx-bootmem.h"
#include "cvmx-spinlock.h"
#include "cvmx-mdio.h"
#include "cvmx-mgmt-port.h"
#include "cvmx-sysinfo.h"

/**
 * Format of the TX/RX ring buffer entries
 */
typedef union
{
    uint64_t u64;
    struct
    {
        uint64_t    reserved_62_63  : 2;
        uint64_t    len             : 14;   /* Length of the buffer/packet in bytes */
        uint64_t    code            : 8;    /* The RX error code */
        uint64_t    addr            : 40;   /* Physical address of the buffer */
    } s;
} cvmx_mgmt_port_ring_entry_t;

/**
 * Per port state required for each mgmt port
 */
typedef struct
{
    cvmx_spinlock_t             lock;           /* Used for exclusive access to this structure */
    int                         tx_write_index; /* Where the next TX will write in the tx_ring and tx_buffers */
    int                         rx_read_index;  /* Where the next RX will be in the rx_ring and rx_buffers */
    int                         phy_id;         /* The SMI/MDIO PHY address */
    uint64_t                    mac;            /* Our MAC address */
    cvmx_mgmt_port_ring_entry_t tx_ring[CVMX_MGMT_PORT_NUM_TX_BUFFERS];
    cvmx_mgmt_port_ring_entry_t rx_ring[CVMX_MGMT_PORT_NUM_RX_BUFFERS];
    char                        tx_buffers[CVMX_MGMT_PORT_NUM_TX_BUFFERS][CVMX_MGMT_PORT_TX_BUFFER_SIZE];
    char                        rx_buffers[CVMX_MGMT_PORT_NUM_RX_BUFFERS][CVMX_MGMT_PORT_RX_BUFFER_SIZE];
} cvmx_mgmt_port_state_t;

/**
 * Pointers to each mgmt port's state
 */
CVMX_SHARED cvmx_mgmt_port_state_t *cvmx_mgmt_port_state_ptr = NULL;


/**
 * Return the number of management ports supported by this chip
 *
 * @return Number of ports
 */
int __cvmx_mgmt_port_num_ports(void)
{
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
        return 1;
    else if (OCTEON_IS_MODEL(OCTEON_CN52XX))
        return 2;
    else
        return 0;
}


/**
 * Called to initialize a management port for use. Multiple calls
 * to this function accross applications is safe.
 *
 * @param port   Port to initialize
 *
 * @return CVMX_MGMT_PORT_SUCCESS or an error code
 */
cvmx_mgmt_port_result_t cvmx_mgmt_port_initialize(int port)
{
    char *alloc_name = "cvmx_mgmt_port";
    cvmx_mixx_oring1_t oring1;
    cvmx_mixx_ctl_t mix_ctl;

    if ((port < 0) || (port >= __cvmx_mgmt_port_num_ports()))
        return CVMX_MGMT_PORT_INVALID_PARAM;

    cvmx_mgmt_port_state_ptr = cvmx_bootmem_alloc_named(CVMX_MGMT_PORT_NUM_PORTS * sizeof(cvmx_mgmt_port_state_t), 128, alloc_name);
    if (cvmx_mgmt_port_state_ptr)
    {
        memset(cvmx_mgmt_port_state_ptr, 0, CVMX_MGMT_PORT_NUM_PORTS * sizeof(cvmx_mgmt_port_state_t));
    }
    else
    {
        cvmx_bootmem_named_block_desc_t *block_desc = cvmx_bootmem_find_named_block(alloc_name);
        if (block_desc)
            cvmx_mgmt_port_state_ptr = cvmx_phys_to_ptr(block_desc->base_addr);
        else
        {
            cvmx_dprintf("ERROR: cvmx_mgmt_port_initialize: Unable to get named block %s.\n", alloc_name);
            return CVMX_MGMT_PORT_NO_MEMORY;
        }
    }

    /* Reset the MIX block if the previous user had a different TX ring size, or if
    ** we allocated a new (and blank) state structure. */
    mix_ctl.u64 = cvmx_read_csr(CVMX_MIXX_CTL(port));
    if (!mix_ctl.s.reset)
    {
        oring1.u64 = cvmx_read_csr(CVMX_MIXX_ORING1(port));
        if (oring1.s.osize != CVMX_MGMT_PORT_NUM_TX_BUFFERS || cvmx_mgmt_port_state_ptr[port].tx_ring[0].u64 == 0)
        {
            mix_ctl.u64 = cvmx_read_csr(CVMX_MIXX_CTL(port));
            mix_ctl.s.en = 0;
            cvmx_write_csr(CVMX_MIXX_CTL(port), mix_ctl.u64);
            do
            {
                mix_ctl.u64 = cvmx_read_csr(CVMX_MIXX_CTL(port));
            } while (mix_ctl.s.busy);
            mix_ctl.s.reset = 1;
            cvmx_write_csr(CVMX_MIXX_CTL(port), mix_ctl.u64);
            cvmx_read_csr(CVMX_MIXX_CTL(port));
            memset(cvmx_mgmt_port_state_ptr + port, 0, sizeof(cvmx_mgmt_port_state_t));
        }
    }


    if (cvmx_mgmt_port_state_ptr[port].tx_ring[0].u64 == 0)
    {
        cvmx_mgmt_port_state_t *state = cvmx_mgmt_port_state_ptr + port;
        int i;
        cvmx_mixx_bist_t mix_bist;
        cvmx_agl_gmx_bist_t agl_gmx_bist;
        cvmx_mixx_oring1_t oring1;
        cvmx_mixx_iring1_t iring1;
        cvmx_mixx_ctl_t mix_ctl;

        /* Make sure BIST passed */
        mix_bist.u64 = cvmx_read_csr(CVMX_MIXX_BIST(port));
        if (mix_bist.u64)
            cvmx_dprintf("WARNING: cvmx_mgmt_port_initialize: Managment port MIX failed BIST (0x%016llx)\n", CAST64(mix_bist.u64));

        agl_gmx_bist.u64 = cvmx_read_csr(CVMX_AGL_GMX_BIST);
        if (agl_gmx_bist.u64)
            cvmx_dprintf("WARNING: cvmx_mgmt_port_initialize: Managment port AGL failed BIST (0x%016llx)\n", CAST64(agl_gmx_bist.u64));

        /* Clear all state information */
        memset(state, 0, sizeof(*state));

        /* Take the control logic out of reset */
        mix_ctl.u64 = cvmx_read_csr(CVMX_MIXX_CTL(port));
        mix_ctl.s.reset = 0;
        cvmx_write_csr(CVMX_MIXX_CTL(port), mix_ctl.u64);

        /* Set the PHY address */
        if (cvmx_sysinfo_get()->board_type == CVMX_BOARD_TYPE_SIM)
            state->phy_id = -1;
        else
            state->phy_id = port;  /* Will need to be change to match the board */

        /* Create a default MAC address */
        state->mac = 0x000000dead000000ull;
        state->mac += 0xffffff & CAST64(state);

        /* Setup the TX ring */
        for (i=0; i<CVMX_MGMT_PORT_NUM_TX_BUFFERS; i++)
        {
            state->tx_ring[i].s.len = CVMX_MGMT_PORT_TX_BUFFER_SIZE;
            state->tx_ring[i].s.addr = cvmx_ptr_to_phys(state->tx_buffers[i]);
        }

        /* Tell the HW where the TX ring is */
        oring1.u64 = 0;
        oring1.s.obase = cvmx_ptr_to_phys(state->tx_ring)>>3;
        oring1.s.osize = CVMX_MGMT_PORT_NUM_TX_BUFFERS;
        CVMX_SYNCWS;
        cvmx_write_csr(CVMX_MIXX_ORING1(port), oring1.u64);

        /* Setup the RX ring */
        for (i=0; i<CVMX_MGMT_PORT_NUM_RX_BUFFERS; i++)
        {
            /* This size is -8 due to an errata for CN56XX pass 1 */
            state->rx_ring[i].s.len = CVMX_MGMT_PORT_RX_BUFFER_SIZE - 8;
            state->rx_ring[i].s.addr = cvmx_ptr_to_phys(state->rx_buffers[i]);
        }

        /* Tell the HW where the RX ring is */
        iring1.u64 = 0;
        iring1.s.ibase = cvmx_ptr_to_phys(state->rx_ring)>>3;
        iring1.s.isize = CVMX_MGMT_PORT_NUM_RX_BUFFERS;
        CVMX_SYNCWS;
        cvmx_write_csr(CVMX_MIXX_IRING1(port), iring1.u64);
        cvmx_write_csr(CVMX_MIXX_IRING2(port), CVMX_MGMT_PORT_NUM_RX_BUFFERS);

        /* Disable the external input/output */
        cvmx_mgmt_port_disable(port);

        /* Set the MAC address filtering up */
        cvmx_mgmt_port_set_mac(port, state->mac);

        /* Set the default max size to an MTU of 1500 with L2 and VLAN */
        cvmx_mgmt_port_set_max_packet_size(port, 1518);

        /* Enable the port HW. Packets are not allowed until cvmx_mgmt_port_enable() is called */
        mix_ctl.u64 = 0;
        mix_ctl.s.crc_strip = 1;    /* Strip the ending CRC */
        mix_ctl.s.en = 1;           /* Enable the port */
        mix_ctl.s.nbtarb = 0;       /* Arbitration mode */
        mix_ctl.s.mrq_hwm = 1;      /* MII CB-request FIFO programmable high watermark */
        cvmx_write_csr(CVMX_MIXX_CTL(port), mix_ctl.u64);

        if (OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1_X) || OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_X))
        {
            /* Force compensation values, as they are not determined properly by HW */
            cvmx_agl_gmx_drv_ctl_t drv_ctl;

            drv_ctl.u64 = cvmx_read_csr(CVMX_AGL_GMX_DRV_CTL);
            if (port)
            {
                drv_ctl.s.byp_en1 = 1;
                drv_ctl.s.nctl1 = 6;
                drv_ctl.s.pctl1 = 6;
            }
            else
            {
                drv_ctl.s.byp_en = 1;
                drv_ctl.s.nctl = 6;
                drv_ctl.s.pctl = 6;
            }
            cvmx_write_csr(CVMX_AGL_GMX_DRV_CTL, drv_ctl.u64);
        }
    }
    return CVMX_MGMT_PORT_SUCCESS;
}


/**
 * Shutdown a management port. This currently disables packet IO
 * but leaves all hardware and buffers. Another application can then
 * call initialize() without redoing the hardware setup.
 *
 * @param port   Management port
 *
 * @return CVMX_MGMT_PORT_SUCCESS or an error code
 */
cvmx_mgmt_port_result_t cvmx_mgmt_port_shutdown(int port)
{
    if ((port < 0) || (port >= __cvmx_mgmt_port_num_ports()))
        return CVMX_MGMT_PORT_INVALID_PARAM;

    /* Stop packets from comming in */
    cvmx_mgmt_port_disable(port);

    /* We don't free any memory so the next intialize can reuse the HW setup */
    return CVMX_MGMT_PORT_SUCCESS;
}


/**
 * Enable packet IO on a management port
 *
 * @param port   Management port
 *
 * @return CVMX_MGMT_PORT_SUCCESS or an error code
 */
cvmx_mgmt_port_result_t cvmx_mgmt_port_enable(int port)
{
    cvmx_mgmt_port_state_t *state;
    cvmx_agl_gmx_prtx_cfg_t agl_gmx_prtx;
    cvmx_agl_gmx_inf_mode_t agl_gmx_inf_mode;
    cvmx_agl_gmx_rxx_frm_ctl_t rxx_frm_ctl;

    if ((port < 0) || (port >= __cvmx_mgmt_port_num_ports()))
        return CVMX_MGMT_PORT_INVALID_PARAM;

    state = cvmx_mgmt_port_state_ptr + port;

    cvmx_spinlock_lock(&state->lock);

    rxx_frm_ctl.u64 = 0;
    rxx_frm_ctl.s.pre_align = 1;
    rxx_frm_ctl.s.pad_len = 1;  /* When set, disables the length check for non-min sized pkts with padding in the client data */
    rxx_frm_ctl.s.vlan_len = 1; /* When set, disables the length check for VLAN pkts */
    rxx_frm_ctl.s.pre_free = 1; /* When set, PREAMBLE checking is  less strict */
    rxx_frm_ctl.s.ctl_smac = 0; /* Control Pause Frames can match station SMAC */
    rxx_frm_ctl.s.ctl_mcst = 1; /* Control Pause Frames can match globally assign Multicast address */
    rxx_frm_ctl.s.ctl_bck = 1;  /* Forward pause information to TX block */
    rxx_frm_ctl.s.ctl_drp = 1;  /* Drop Control Pause Frames */
    rxx_frm_ctl.s.pre_strp = 1; /* Strip off the preamble */
    rxx_frm_ctl.s.pre_chk = 1;  /* This port is configured to send PREAMBLE+SFD to begin every frame.  GMX checks that the PREAMBLE is sent correctly */
    cvmx_write_csr(CVMX_AGL_GMX_RXX_FRM_CTL(port), rxx_frm_ctl.u64);

    /* Enable the AGL block */
    agl_gmx_inf_mode.u64 = 0;
    agl_gmx_inf_mode.s.en = 1;
    cvmx_write_csr(CVMX_AGL_GMX_INF_MODE, agl_gmx_inf_mode.u64);

    /* Configure the port duplex and enables */
    agl_gmx_prtx.u64 = cvmx_read_csr(CVMX_AGL_GMX_PRTX_CFG(port));
    agl_gmx_prtx.s.tx_en = 1;
    agl_gmx_prtx.s.rx_en = 1;
    if (cvmx_mgmt_port_get_link(port) < 0)
        agl_gmx_prtx.s.duplex = 0;
    else
        agl_gmx_prtx.s.duplex = 1;
    agl_gmx_prtx.s.en = 1;
    cvmx_write_csr(CVMX_AGL_GMX_PRTX_CFG(port), agl_gmx_prtx.u64);

    cvmx_spinlock_unlock(&state->lock);
    return CVMX_MGMT_PORT_SUCCESS;
}


/**
 * Disable packet IO on a management port
 *
 * @param port   Management port
 *
 * @return CVMX_MGMT_PORT_SUCCESS or an error code
 */
cvmx_mgmt_port_result_t cvmx_mgmt_port_disable(int port)
{
    cvmx_mgmt_port_state_t *state;
    cvmx_agl_gmx_prtx_cfg_t agl_gmx_prtx;

    if ((port < 0) || (port >= __cvmx_mgmt_port_num_ports()))
        return CVMX_MGMT_PORT_INVALID_PARAM;

    state = cvmx_mgmt_port_state_ptr + port;

    cvmx_spinlock_lock(&state->lock);

    agl_gmx_prtx.u64 = cvmx_read_csr(CVMX_AGL_GMX_PRTX_CFG(port));
    agl_gmx_prtx.s.en = 0;
    cvmx_write_csr(CVMX_AGL_GMX_PRTX_CFG(port), agl_gmx_prtx.u64);

    cvmx_spinlock_unlock(&state->lock);
    return CVMX_MGMT_PORT_SUCCESS;
}


/**
 * Send a packet out the management port. The packet is copied so
 * the input buffer isn't used after this call.
 *
 * @param port       Management port
 * @param packet_len Length of the packet to send. It does not include the final CRC
 * @param buffer     Packet data
 *
 * @return CVMX_MGMT_PORT_SUCCESS or an error code
 */
cvmx_mgmt_port_result_t cvmx_mgmt_port_send(int port, int packet_len, void *buffer)
{
    cvmx_mgmt_port_state_t *state;
    cvmx_mixx_oring2_t mix_oring2;

    if ((port < 0) || (port >= __cvmx_mgmt_port_num_ports()))
        return CVMX_MGMT_PORT_INVALID_PARAM;

    /* Max sure the packet size is valid */
    if ((packet_len < 1) || (packet_len > CVMX_MGMT_PORT_TX_BUFFER_SIZE))
        return CVMX_MGMT_PORT_INVALID_PARAM;

    if (buffer == NULL)
        return CVMX_MGMT_PORT_INVALID_PARAM;

    state = cvmx_mgmt_port_state_ptr + port;

    cvmx_spinlock_lock(&state->lock);

    mix_oring2.u64 = cvmx_read_csr(CVMX_MIXX_ORING2(port));
    if (mix_oring2.s.odbell >= CVMX_MGMT_PORT_NUM_TX_BUFFERS - 1)
    {
        /* No room for another packet */
        cvmx_spinlock_unlock(&state->lock);
        return CVMX_MGMT_PORT_NO_MEMORY;
    }
    else
    {
        /* Copy the packet into the output buffer */
        memcpy(state->tx_buffers[state->tx_write_index], buffer, packet_len);
        /* Insert the source MAC */
        memcpy(state->tx_buffers[state->tx_write_index] + 6, ((char*)&state->mac) + 2, 6);
        /* Update the TX ring buffer entry size */
        state->tx_ring[state->tx_write_index].s.len = packet_len;
        /* Increment our TX index */
        state->tx_write_index = (state->tx_write_index + 1) % CVMX_MGMT_PORT_NUM_TX_BUFFERS;
        /* Ring the doorbell, send ing the packet */
        CVMX_SYNCWS;
        cvmx_write_csr(CVMX_MIXX_ORING2(port), 1);
        if (cvmx_read_csr(CVMX_MIXX_ORCNT(port)))
            cvmx_write_csr(CVMX_MIXX_ORCNT(port), cvmx_read_csr(CVMX_MIXX_ORCNT(port)));

        cvmx_spinlock_unlock(&state->lock);
        return CVMX_MGMT_PORT_SUCCESS;
    }
}


/**
 * Receive a packet from the management port.
 *
 * @param port       Management port
 * @param buffer_len Size of the buffer to receive the packet into
 * @param buffer     Buffer to receive the packet into
 *
 * @return The size of the packet, or a negative erorr code on failure. Zero
 *         means that no packets were available.
 */
int cvmx_mgmt_port_receive(int port, int buffer_len, void *buffer)
{
    cvmx_mixx_ircnt_t mix_ircnt;
    cvmx_mgmt_port_state_t *state;
    int result;

    if ((port < 0) || (port >= __cvmx_mgmt_port_num_ports()))
        return CVMX_MGMT_PORT_INVALID_PARAM;

    /* Max sure the buffer size is valid */
    if (buffer_len < 1)
        return CVMX_MGMT_PORT_INVALID_PARAM;

    if (buffer == NULL)
        return CVMX_MGMT_PORT_INVALID_PARAM;

    state = cvmx_mgmt_port_state_ptr + port;

    cvmx_spinlock_lock(&state->lock);

    /* Find out how many RX packets are pending */
    mix_ircnt.u64 = cvmx_read_csr(CVMX_MIXX_IRCNT(port));
    if (mix_ircnt.s.ircnt)
    {
        void *source = state->rx_buffers[state->rx_read_index];
        uint64_t *zero_check = source;
        /* CN56XX pass 1 has an errata where packets might start 8 bytes
            into the buffer instead of at their correct lcoation. If the
            first 8 bytes is zero we assume this has happened */
        if (OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1_X) && (*zero_check == 0))
            source += 8;
        /* Start off with zero bytes received */
        result = 0;
        /* While the completion code signals more data, copy the buffers
            into the user's data */
        while (state->rx_ring[state->rx_read_index].s.code == 16)
        {
            /* Only copy what will fit in the user's buffer */
            int length = state->rx_ring[state->rx_read_index].s.len;
            if (length > buffer_len)
                length = buffer_len;
            memcpy(buffer, source, length);
            /* Reduce the size of the buffer to the remaining space. If we run
                out we will signal an error when the code 15 buffer doesn't fit */
            buffer += length;
            buffer_len -= length;
            result += length;
            /* Update this buffer for reuse in future receives. This size is
                -8 due to an errata for CN56XX pass 1 */
            state->rx_ring[state->rx_read_index].s.code = 0;
            state->rx_ring[state->rx_read_index].s.len = CVMX_MGMT_PORT_RX_BUFFER_SIZE - 8;
            state->rx_read_index = (state->rx_read_index + 1) % CVMX_MGMT_PORT_NUM_RX_BUFFERS;
            /* Zero the beginning of the buffer for use by the errata check */
            *zero_check = 0;
            CVMX_SYNCWS;
            /* Increment the number of RX buffers */
            cvmx_write_csr(CVMX_MIXX_IRING2(port), 1);
            source = state->rx_buffers[state->rx_read_index];
            zero_check = source;
        }

        /* Check for the final good completion code */
        if (state->rx_ring[state->rx_read_index].s.code == 15)
        {
            if (buffer_len >= state->rx_ring[state->rx_read_index].s.len)
            {
                int length = state->rx_ring[state->rx_read_index].s.len;
                memcpy(buffer, source, length);
                result += length;
            }
            else
            {
                /* Not enough room for the packet */
                cvmx_dprintf("ERROR: cvmx_mgmt_port_receive: Packet (%d) larger than supplied buffer (%d)\n", state->rx_ring[state->rx_read_index].s.len, buffer_len);
                result = CVMX_MGMT_PORT_NO_MEMORY;
            }
        }
        else
        {
            cvmx_agl_gmx_prtx_cfg_t agl_gmx_prtx;
            cvmx_dprintf("ERROR: cvmx_mgmt_port_receive: Receive error code %d. Packet dropped(Len %d), \n",
                         state->rx_ring[state->rx_read_index].s.code, state->rx_ring[state->rx_read_index].s.len + result);
            result = -state->rx_ring[state->rx_read_index].s.code;


            /* Check to see if we need to change the duplex. */
            agl_gmx_prtx.u64 = cvmx_read_csr(CVMX_AGL_GMX_PRTX_CFG(port));
            if (cvmx_mgmt_port_get_link(port) < 0)
                agl_gmx_prtx.s.duplex = 0;
            else
                agl_gmx_prtx.s.duplex = 1;
            cvmx_write_csr(CVMX_AGL_GMX_PRTX_CFG(port), agl_gmx_prtx.u64);
        }

        /* Clean out the ring buffer entry. This size is -8 due to an errata
            for CN56XX pass 1 */
        state->rx_ring[state->rx_read_index].s.code = 0;
        state->rx_ring[state->rx_read_index].s.len = CVMX_MGMT_PORT_RX_BUFFER_SIZE - 8;
        state->rx_read_index = (state->rx_read_index + 1) % CVMX_MGMT_PORT_NUM_RX_BUFFERS;
        /* Zero the beginning of the buffer for use by the errata check */
        *zero_check = 0;
        CVMX_SYNCWS;
        /* Increment the number of RX buffers */
        cvmx_write_csr(CVMX_MIXX_IRING2(port), 1);
        /* Decrement the pending RX count */
        cvmx_write_csr(CVMX_MIXX_IRCNT(port), 1);
    }
    else
    {
        /* No packets available */
        result = 0;
    }
    cvmx_spinlock_unlock(&state->lock);
    return result;
}


/**
 * Get the management port link status:
 * 100 = 100Mbps, full duplex
 * 10 = 10Mbps, full duplex
 * 0 = Link down
 * -10 = 10Mpbs, half duplex
 * -100 = 100Mbps, half duplex
 *
 * @param port   Management port
 *
 * @return
 */
int cvmx_mgmt_port_get_link(int port)
{
    cvmx_mgmt_port_state_t *state;
    int phy_status;

    if ((port < 0) || (port >= __cvmx_mgmt_port_num_ports()))
        return CVMX_MGMT_PORT_INVALID_PARAM;

    state = cvmx_mgmt_port_state_ptr + port;

    /* Assume 100Mbps if we don't know the PHY address */
    if (state->phy_id == -1)
        return 100;


    /* read BCM phy MDIO aux status summary register */
    phy_status = cvmx_mdio_read(state->phy_id >> 8, state->phy_id & 0xff,
				    0x19);
    /* check the link status first */
    if ((phy_status & 0x8000) == 0)
      return 0;

    switch ((phy_status >> 8) & 0x7)
      {
      case 0:
	/* link down */
	return 0;
      case 1:
	/* 10 half */
	return -10;
      case 2:
	/* 10 full */
	return 10;
      case 3:
	/* 100 half */
	return -100;
      case 4:
	/* 100 T4 */
	return 100;
      case 5:
	/* 100 full */
	return 100;
      case 6:
	/* 1000 half */
	return -1000;
      case 7:
	/* 1000 full */
	return 1000;
      }
    /* something's amiss if we get here... */
    return 0;
}


/**
 * Set the MAC address for a management port
 *
 * @param port   Management port
 * @param mac    New MAC address. The lower 6 bytes are used.
 *
 * @return CVMX_MGMT_PORT_SUCCESS or an error code
 */
cvmx_mgmt_port_result_t cvmx_mgmt_port_set_mac(int port, uint64_t mac)
{
    cvmx_mgmt_port_state_t *state;
    cvmx_agl_gmx_rxx_adr_ctl_t agl_gmx_rxx_adr_ctl;

    if ((port < 0) || (port >= __cvmx_mgmt_port_num_ports()))
        return CVMX_MGMT_PORT_INVALID_PARAM;

    state = cvmx_mgmt_port_state_ptr + port;

    cvmx_spinlock_lock(&state->lock);

    agl_gmx_rxx_adr_ctl.u64 = 0;
    agl_gmx_rxx_adr_ctl.s.cam_mode = 1; /* Only accept matching MAC addresses */
    agl_gmx_rxx_adr_ctl.s.mcst = 0;     /* Drop multicast */
    agl_gmx_rxx_adr_ctl.s.bcst = 1;     /* Allow broadcast */
    cvmx_write_csr(CVMX_AGL_GMX_RXX_ADR_CTL(port), agl_gmx_rxx_adr_ctl.u64);

    /* Only using one of the CAMs */
    cvmx_write_csr(CVMX_AGL_GMX_RXX_ADR_CAM0(port), (mac >> 40) & 0xff);
    cvmx_write_csr(CVMX_AGL_GMX_RXX_ADR_CAM1(port), (mac >> 32) & 0xff);
    cvmx_write_csr(CVMX_AGL_GMX_RXX_ADR_CAM2(port), (mac >> 24) & 0xff);
    cvmx_write_csr(CVMX_AGL_GMX_RXX_ADR_CAM3(port), (mac >> 16) & 0xff);
    cvmx_write_csr(CVMX_AGL_GMX_RXX_ADR_CAM4(port), (mac >> 8) & 0xff);
    cvmx_write_csr(CVMX_AGL_GMX_RXX_ADR_CAM5(port), (mac >> 0) & 0xff);
    cvmx_write_csr(CVMX_AGL_GMX_RXX_ADR_CAM_EN(port), 1);
    state->mac = mac;

    cvmx_spinlock_unlock(&state->lock);
    return CVMX_MGMT_PORT_SUCCESS;
}


/**
 * Get the MAC address for a management port
 *
 * @param port   Management port
 *
 * @return MAC address
 */
uint64_t cvmx_mgmt_port_get_mac(int port)
{
    if ((port < 0) || (port >= __cvmx_mgmt_port_num_ports()))
        return CVMX_MGMT_PORT_INVALID_PARAM;

    return cvmx_mgmt_port_state_ptr[port].mac;
}

/**
 * Set the multicast list.
 *
 * @param port   Management port
 * @param flags  Interface flags
 *
 * @return
 */
void cvmx_mgmt_port_set_multicast_list(int port, int flags)
{
    cvmx_mgmt_port_state_t *state;
    cvmx_agl_gmx_rxx_adr_ctl_t agl_gmx_rxx_adr_ctl;

    if ((port < 0) || (port >= __cvmx_mgmt_port_num_ports()))
        return;

    state = cvmx_mgmt_port_state_ptr + port;

    cvmx_spinlock_lock(&state->lock);

    agl_gmx_rxx_adr_ctl.u64 = cvmx_read_csr(CVMX_AGL_GMX_RXX_ADR_CTL(port));
    
    /* Allow broadcast MAC addresses */
    if (!agl_gmx_rxx_adr_ctl.s.bcst)
	agl_gmx_rxx_adr_ctl.s.bcst = 1;

    if ((flags & CVMX_IFF_ALLMULTI) || (flags & CVMX_IFF_PROMISC))
	agl_gmx_rxx_adr_ctl.s.mcst = 2; /* Force accept multicast packets */
    else
	agl_gmx_rxx_adr_ctl.s.mcst = 1; /* Force reject multicast packets */

    if (flags & CVMX_IFF_PROMISC)
	agl_gmx_rxx_adr_ctl.s.cam_mode = 0; /* Reject matches if promisc. Since CAM is shut off, should accept everything */
    else
	agl_gmx_rxx_adr_ctl.s.cam_mode = 1; /* Filter packets based on the CAM */

    cvmx_write_csr(CVMX_AGL_GMX_RXX_ADR_CTL(port), agl_gmx_rxx_adr_ctl.u64);

    if (flags & CVMX_IFF_PROMISC)
	cvmx_write_csr(CVMX_AGL_GMX_RXX_ADR_CAM_EN(port), 0);
    else
	cvmx_write_csr(CVMX_AGL_GMX_RXX_ADR_CAM_EN(port), 1);
    
    cvmx_spinlock_unlock(&state->lock);
}


/**
 * Set the maximum packet allowed in. Size is specified
 * including L2 but without FCS. A normal MTU would corespond
 * to 1514 assuming the standard 14 byte L2 header.
 *
 * @param port   Management port
 * @param size_without_fcs
 *               Size in bytes without FCS
 */
void cvmx_mgmt_port_set_max_packet_size(int port, int size_without_fcs)
{
    cvmx_mgmt_port_state_t *state;

    if ((port < 0) || (port >= __cvmx_mgmt_port_num_ports()))
        return;

    state = cvmx_mgmt_port_state_ptr + port;

    cvmx_spinlock_lock(&state->lock);
    cvmx_write_csr(CVMX_AGL_GMX_RXX_FRM_MAX(port), size_without_fcs);
    cvmx_write_csr(CVMX_AGL_GMX_RXX_JABBER(port), (size_without_fcs+7) & 0xfff8);
    cvmx_spinlock_unlock(&state->lock);
}

