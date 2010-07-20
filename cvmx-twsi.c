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
 * Interface to the TWSI / I2C bus
 *
 * <hr>$Revision: 41586 $<hr>
 *
 */

#include "cvmx.h"
#include "cvmx-twsi.h"





/**
 * Do a twsi read from a 7 bit device address using an (optional) internal address.
 * Up to 8 bytes can be read at a time.
 * 
 * @param twsi_id   which Octeon TWSI bus to use
 * @param dev_addr  Device address (7 bit)
 * @param internal_addr
 *                  Internal address.  Can be 0, 1 or 2 bytes in width
 * @param num_bytes Number of data bytes to read
 * @param ia_width_bytes
 *                  Internal address size in bytes (0, 1, or 2)
 * @param data      Pointer argument where the read data is returned.
 * 
 * @return read data returned in 'data' argument
 *         Number of bytes read on success
 *         -1 on failure
 */
int cvmx_twsix_read_ia(int twsi_id, uint8_t dev_addr, uint16_t internal_addr, int num_bytes, int ia_width_bytes, uint64_t *data)
{
    cvmx_mio_twsx_sw_twsi_t sw_twsi_val;
    cvmx_mio_twsx_sw_twsi_ext_t twsi_ext;

    if (num_bytes < 1 || num_bytes > 8 || !data || ia_width_bytes < 0 || ia_width_bytes > 2)
        return -1;

    twsi_ext.u64 = 0;
    sw_twsi_val.u64 = 0;
    sw_twsi_val.s.v = 1;
    sw_twsi_val.s.r = 1;
    sw_twsi_val.s.sovr = 1;
    sw_twsi_val.s.size = num_bytes - 1;
    sw_twsi_val.s.a = dev_addr;

    if (ia_width_bytes > 0)
    {
        sw_twsi_val.s.op = 1;
        sw_twsi_val.s.ia = (internal_addr >> 3) & 0x1f;
        sw_twsi_val.s.eop_ia = internal_addr & 0x7;
    } 
    if (ia_width_bytes == 2)
    {
        sw_twsi_val.s.eia = 1;
        twsi_ext.s.ia = internal_addr >> 8;
        cvmx_write_csr(CVMX_MIO_TWSX_SW_TWSI_EXT(twsi_id), twsi_ext.u64);
    }

    cvmx_write_csr(CVMX_MIO_TWSX_SW_TWSI(twsi_id), sw_twsi_val.u64);
    while (((cvmx_mio_twsx_sw_twsi_t)(sw_twsi_val.u64 = cvmx_read_csr(CVMX_MIO_TWSX_SW_TWSI(twsi_id)))).s.v)
        ;
    if (!sw_twsi_val.s.r)
        return -1;

    *data = (sw_twsi_val.s.d & (0xFFFFFFFF >> (32 - num_bytes*8)));
    if (num_bytes > 4)
    {
        twsi_ext.u64 = cvmx_read_csr(CVMX_MIO_TWSX_SW_TWSI_EXT(twsi_id));
        *data |= ((unsigned long long)(twsi_ext.s.d & (0xFFFFFFFF >> (32 - num_bytes*8))) << 32);

    }
    return num_bytes;
}




/**
 * Read from a TWSI device (7 bit device address only) without generating any
 * internal addresses.
 * Read from 1-8 bytes and returns them in the data pointer.
 * 
 * @param twsi_id   TWSI interface on Octeon to use
 * @param dev_addr  TWSI device address (7 bit only)
 * @param num_bytes number of bytes to read
 * @param data      Pointer to data read from TWSI device
 * 
 * @return Number of bytes read on success
 *         -1 on error
 */
int cvmx_twsix_read(int twsi_id, uint8_t dev_addr, int num_bytes, uint64_t *data)
{
    cvmx_mio_twsx_sw_twsi_t sw_twsi_val;
    cvmx_mio_twsx_sw_twsi_ext_t twsi_ext;

    if (num_bytes > 8 || num_bytes < 1)
        return -1;

    sw_twsi_val.u64 = 0;
    sw_twsi_val.s.v = 1;
    sw_twsi_val.s.r = 1;
    sw_twsi_val.s.a = dev_addr;
    sw_twsi_val.s.sovr = 1;
    sw_twsi_val.s.size = num_bytes - 1;

    cvmx_write_csr(CVMX_MIO_TWSX_SW_TWSI(twsi_id), sw_twsi_val.u64);
    while (((cvmx_mio_twsx_sw_twsi_t)(sw_twsi_val.u64 = cvmx_read_csr(CVMX_MIO_TWSX_SW_TWSI(twsi_id)))).s.v)
        ;
    if (!sw_twsi_val.s.r)
        return -1;

    *data = (sw_twsi_val.s.d & (0xFFFFFFFF >> (32 - num_bytes*8)));
    if (num_bytes > 4)
    {
        twsi_ext.u64 = cvmx_read_csr(CVMX_MIO_TWSX_SW_TWSI_EXT(twsi_id));
        *data |= ((unsigned long long)(twsi_ext.s.d & (0xFFFFFFFF >> (32 - num_bytes*8))) << 32);

    }
    return num_bytes;
}


/**
 * Perform a twsi write operation to a 7 bit device address.
 * 
 * Note that many eeprom devices have page restrictions regarding address boundaries
 * that can be crossed in one write operation.  This is device dependent, and this routine
 * does nothing in this regard.
 * This command does not generate any internal addressess.
 * 
 * @param twsi_id   Octeon TWSI interface to use
 * @param dev_addr  TWSI device address
 * @param num_bytes Number of bytes to write (between 1 and 8 inclusive)
 * @param data      Data to write
 * 
 * @return 0 on success
 *         -1 on failure
 */
int cvmx_twsix_write(int twsi_id, uint8_t dev_addr, int num_bytes, uint64_t data)
{
    cvmx_mio_twsx_sw_twsi_t sw_twsi_val;

    if (num_bytes > 8 || num_bytes < 1)
        return -1;

    sw_twsi_val.u64 = 0;
    sw_twsi_val.s.v = 1;
    sw_twsi_val.s.a = dev_addr;
    sw_twsi_val.s.d = data & 0xffffffff;
    sw_twsi_val.s.sovr = 1;
    sw_twsi_val.s.size = num_bytes - 1;
    if (num_bytes > 4)
    {
        /* Upper four bytes go into a separate register */
        cvmx_mio_twsx_sw_twsi_ext_t twsi_ext;
        twsi_ext.u64 = 0;
        twsi_ext.s.d = data >> 32;
        cvmx_write_csr(CVMX_MIO_TWSX_SW_TWSI_EXT(twsi_id), twsi_ext.u64);
    }
    cvmx_write_csr(CVMX_MIO_TWSX_SW_TWSI(twsi_id), sw_twsi_val.u64);
    while (((cvmx_mio_twsx_sw_twsi_t)(sw_twsi_val.u64 = cvmx_read_csr(CVMX_MIO_TWSX_SW_TWSI(twsi_id)))).s.v)
        ;
    if (!sw_twsi_val.s.r)
        return -1;

    return 0;
}

/**
 * Write 1-8 bytes to a TWSI device using an internal address.
 * 
 * @param twsi_id   which TWSI interface on Octeon to use
 * @param dev_addr  TWSI device address (7 bit only)
 * @param internal_addr
 *                  TWSI internal address (0, 8, or 16 bits)
 * @param num_bytes Number of bytes to write (1-8)
 * @param ia_width_bytes
 *                  internal address width, in bytes (0, 1, 2)
 * @param data      Data to write.  Data is written MSB first on the twsi bus, and only the lower
 *                  num_bytes bytes of the argument are valid.  (If a 2 byte write is done, only
 *                  the low 2 bytes of the argument is used.
 * 
 * @return Number of bytes read on success,
 *         -1 on error
 */
int cvmx_twsix_write_ia(int twsi_id, uint8_t dev_addr, uint16_t internal_addr, int num_bytes, int ia_width_bytes, uint64_t data)
{
    cvmx_mio_twsx_sw_twsi_t sw_twsi_val;
    cvmx_mio_twsx_sw_twsi_ext_t twsi_ext;
    int to;

    if (num_bytes < 1 || num_bytes > 8 || ia_width_bytes < 0 || ia_width_bytes > 2)
        return -1;

    twsi_ext.u64 = 0;

    sw_twsi_val.u64 = 0;
    sw_twsi_val.s.v = 1;
    sw_twsi_val.s.sovr = 1;
    sw_twsi_val.s.size = num_bytes - 1;
    sw_twsi_val.s.a = dev_addr;
    sw_twsi_val.s.d = 0xFFFFFFFF & data;

    if (ia_width_bytes > 0)
    {
        sw_twsi_val.s.op = 1;
        sw_twsi_val.s.ia = (internal_addr >> 3) & 0x1f;
        sw_twsi_val.s.eop_ia = internal_addr & 0x7;
    } 
    if (ia_width_bytes == 2)
    {
        sw_twsi_val.s.eia = 1;
        twsi_ext.s.ia = internal_addr >> 8;
        cvmx_write_csr(CVMX_MIO_TWSX_SW_TWSI_EXT(twsi_id), twsi_ext.u64);
    }
    if (num_bytes > 4)
        twsi_ext.s.d = data >> 32;


    cvmx_write_csr(CVMX_MIO_TWSX_SW_TWSI_EXT(twsi_id), twsi_ext.u64);
    cvmx_write_csr(CVMX_MIO_TWSX_SW_TWSI(twsi_id), sw_twsi_val.u64);
    while (((cvmx_mio_twsx_sw_twsi_t)(sw_twsi_val.u64 = cvmx_read_csr(CVMX_MIO_TWSX_SW_TWSI(twsi_id)))).s.v)
        ;

    /* Poll until reads succeed, or polling times out */
    to = 100;
    while (to-- > 0)
    {
        uint64_t data;
        if (cvmx_twsix_read(twsi_id, dev_addr, 1, &data) >= 0)
            break;
    }
    if (to <= 0)
        return -1;

    return num_bytes;
}

