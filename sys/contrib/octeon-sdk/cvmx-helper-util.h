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
 * Small helper utilities.
 *
 * <hr>$Revision: 41586 $<hr>
 */

#ifndef __CVMX_HELPER_UTIL_H__
#define __CVMX_HELPER_UTIL_H__


#ifdef CVMX_ENABLE_HELPER_FUNCTIONS

/**
 * Convert a interface mode into a human readable string
 *
 * @param mode   Mode to convert
 *
 * @return String
 */
extern const char *cvmx_helper_interface_mode_to_string(cvmx_helper_interface_mode_t mode);

/**
 * Debug routine to dump the packet structure to the console
 *
 * @param work   Work queue entry containing the packet to dump
 * @return
 */
extern int cvmx_helper_dump_packet(cvmx_wqe_t *work);

/**
 * Setup Random Early Drop on a specific input queue
 *
 * @param queue  Input queue to setup RED on (0-7)
 * @param pass_thresh
 *               Packets will begin slowly dropping when there are less than
 *               this many packet buffers free in FPA 0.
 * @param drop_thresh
 *               All incomming packets will be dropped when there are less
 *               than this many free packet buffers in FPA 0.
 * @return Zero on success. Negative on failure
 */
extern int cvmx_helper_setup_red_queue(int queue, int pass_thresh, int drop_thresh);

/**
 * Setup Random Early Drop to automatically begin dropping packets.
 *
 * @param pass_thresh
 *               Packets will begin slowly dropping when there are less than
 *               this many packet buffers free in FPA 0.
 * @param drop_thresh
 *               All incomming packets will be dropped when there are less
 *               than this many free packet buffers in FPA 0.
 * @return Zero on success. Negative on failure
 */
extern int cvmx_helper_setup_red(int pass_thresh, int drop_thresh);


/**
 * Get the version of the CVMX libraries.
 *
 * @return Version string. Note this buffer is allocated statically
 *         and will be shared by all callers.
 */
extern const char *cvmx_helper_get_version(void);


/**
 * @INTERNAL
 * Setup the common GMX settings that determine the number of
 * ports. These setting apply to almost all configurations of all
 * chips.
 *
 * @param interface Interface to configure
 * @param num_ports Number of ports on the interface
 *
 * @return Zero on success, negative on failure
 */
extern int __cvmx_helper_setup_gmx(int interface, int num_ports);

/**
 * Returns the IPD/PKO port number for a port on the given
 * interface.
 *
 * @param interface Interface to use
 * @param port      Port on the interface
 *
 * @return IPD/PKO port number
 */
extern int cvmx_helper_get_ipd_port(int interface, int port);


/**
 * Returns the IPD/PKO port number for the first port on the given
 * interface.
 *
 * @param interface Interface to use
 *
 * @return IPD/PKO port number
 */
static inline int cvmx_helper_get_first_ipd_port(int interface)
{
    return (cvmx_helper_get_ipd_port (interface, 0));
}

/**
 * Returns the IPD/PKO port number for the last port on the given
 * interface.
 *
 * @param interface Interface to use
 *
 * @return IPD/PKO port number
 */
static inline int cvmx_helper_get_last_ipd_port (int interface)
{
    return (cvmx_helper_get_first_ipd_port (interface) +
  	    cvmx_helper_ports_on_interface (interface) - 1);
}


/**
 * Free the packet buffers contained in a work queue entry.
 * The work queue entry is not freed.
 *
 * @param work   Work queue entry with packet to free
 */
static inline void cvmx_helper_free_packet_data(cvmx_wqe_t *work)
{
    uint64_t        number_buffers;
    cvmx_buf_ptr_t  buffer_ptr;
    cvmx_buf_ptr_t  next_buffer_ptr;
    uint64_t        start_of_buffer;

    number_buffers = work->word2.s.bufs;
    if (number_buffers == 0)
        return;
    buffer_ptr = work->packet_ptr;

    /* Since the number of buffers is not zero, we know this is not a dynamic
        short packet. We need to check if it is a packet received with
        IPD_CTL_STATUS[NO_WPTR]. If this is true, we need to free all buffers
        except for the first one. The caller doesn't expect their WQE pointer
        to be freed */
    start_of_buffer = ((buffer_ptr.s.addr >> 7) - buffer_ptr.s.back) << 7;
    if (cvmx_ptr_to_phys(work) == start_of_buffer)
    {
        next_buffer_ptr = *(cvmx_buf_ptr_t*)cvmx_phys_to_ptr(buffer_ptr.s.addr - 8);
        buffer_ptr = next_buffer_ptr;
        number_buffers--;
    }

    while (number_buffers--)
    {
        /* Remember the back pointer is in cache lines, not 64bit words */
        start_of_buffer = ((buffer_ptr.s.addr >> 7) - buffer_ptr.s.back) << 7;
        /* Read pointer to next buffer before we free the current buffer. */
        next_buffer_ptr = *(cvmx_buf_ptr_t*)cvmx_phys_to_ptr(buffer_ptr.s.addr - 8);
        cvmx_fpa_free(cvmx_phys_to_ptr(start_of_buffer), buffer_ptr.s.pool, 0);
        buffer_ptr = next_buffer_ptr;
    }
}

#endif /* CVMX_ENABLE_HELPER_FUNCTIONS */

/**
 * Returns the interface number for an IPD/PKO port number.
 *
 * @param ipd_port IPD/PKO port number
 *
 * @return Interface number
 */
extern int cvmx_helper_get_interface_num(int ipd_port);


/**
 * Returns the interface index number for an IPD/PKO port
 * number.
 *
 * @param ipd_port IPD/PKO port number
 *
 * @return Interface index number
 */
extern int cvmx_helper_get_interface_index_num(int ipd_port);

/**
 * Initialize the internal QLM JTAG logic to allow programming
 * of the JTAG chain by the cvmx_helper_qlm_jtag_*() functions.
 * These functions should only be used at the direction of Cavium
 * Networks. Programming incorrect values into the JTAG chain
 * can cause chip damage.
 */
extern void cvmx_helper_qlm_jtag_init(void);

/**
 * Write up to 32bits into the QLM jtag chain. Bits are shifted
 * into the MSB and out the LSB, so you should shift in the low
 * order bits followed by the high order bits. The JTAG chain is
 * 4 * 268 bits long, or 1072.
 *
 * @param qlm    QLM to shift value into
 * @param bits   Number of bits to shift in (1-32).
 * @param data   Data to shift in. Bit 0 enters the chain first, followed by
 *               bit 1, etc.
 *
 * @return The low order bits of the JTAG chain that shifted out of the
 *         circle.
 */
extern uint32_t cvmx_helper_qlm_jtag_shift(int qlm, int bits, uint32_t data);

/**
 * Shift long sequences of zeros into the QLM JTAG chain. It is
 * common to need to shift more than 32 bits of zeros into the
 * chain. This function is a convience wrapper around
 * cvmx_helper_qlm_jtag_shift() to shift more than 32 bits of
 * zeros at a time.
 *
 * @param qlm    QLM to shift zeros into
 * @param bits
 */
extern void cvmx_helper_qlm_jtag_shift_zeros(int qlm, int bits);

/**
 * Program the QLM JTAG chain into all lanes of the QLM. You must
 * have already shifted in 268*4, or 1072 bits into the JTAG
 * chain. Updating invalid values can possibly cause chip damage.
 *
 * @param qlm    QLM to program
 */
extern void cvmx_helper_qlm_jtag_update(int qlm);

#endif  /* __CVMX_HELPER_H__ */
