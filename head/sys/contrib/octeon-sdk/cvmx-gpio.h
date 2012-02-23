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
 * General Purpose IO interface.
 *
 * <hr>$Revision: 49448 $<hr>
 */

#ifndef __CVMX_GPIO_H__
#define __CVMX_GPIO_H__

#ifdef	__cplusplus
extern "C" {
#endif

/* CSR typedefs have been moved to cvmx-gpio-defs.h */

/**
 * Clear the interrupt rising edge detector for the supplied
 * pins in the mask. Chips which have more than 16 GPIO pins
 * can't use them for interrupts.
 *
 * @param clear_mask Mask of pins to clear
 */
static inline void cvmx_gpio_interrupt_clear(uint16_t clear_mask)
{
    cvmx_gpio_int_clr_t gpio_int_clr;
    gpio_int_clr.u64 = 0;
    gpio_int_clr.s.type = clear_mask;
    cvmx_write_csr(CVMX_GPIO_INT_CLR, gpio_int_clr.u64);
}


/**
 * GPIO Read Data
 *
 * @return Status of the GPIO pins
 */
static inline uint32_t cvmx_gpio_read(void)
{
    cvmx_gpio_rx_dat_t gpio_rx_dat;
    gpio_rx_dat.u64 = cvmx_read_csr(CVMX_GPIO_RX_DAT);
    return gpio_rx_dat.s.dat;
}


/**
 * GPIO Clear pin
 *
 * @param clear_mask Bit mask to indicate which bits to drive to '0'.
 */
static inline void cvmx_gpio_clear(uint32_t clear_mask)
{
    cvmx_gpio_tx_clr_t gpio_tx_clr;
    gpio_tx_clr.u64 = 0;
    gpio_tx_clr.s.clr = clear_mask;
    cvmx_write_csr(CVMX_GPIO_TX_CLR, gpio_tx_clr.u64);
}


/**
 * GPIO Set pin
 *
 * @param set_mask Bit mask to indicate which bits to drive to '1'.
 */
static inline void cvmx_gpio_set(uint32_t set_mask)
{
    cvmx_gpio_tx_set_t gpio_tx_set;
    gpio_tx_set.u64 = 0;
    gpio_tx_set.s.set = set_mask;
    cvmx_write_csr(CVMX_GPIO_TX_SET, gpio_tx_set.u64);
}

#ifdef	__cplusplus
}
#endif

#endif

