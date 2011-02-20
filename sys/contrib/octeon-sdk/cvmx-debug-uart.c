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


#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
#include <linux/module.h>
#include <asm/octeon/cvmx.h>
#include <asm/octeon/cvmx-debug.h>
#include <asm/octeon/cvmx-uart.h>
#include <asm/octeon/octeon-boot-info.h>
#include <asm/octeon/cvmx-spinlock.h>

int cvmx_debug_uart = 1;

#else
#include <limits.h>
#include "executive-config.h"
#include "cvmx.h"
#include "cvmx-debug.h"
#include "cvmx-uart.h"
#include "cvmx-spinlock.h"

#ifndef __OCTEON_NEWLIB__
#include "../../bootloader/u-boot/include/octeon_mem_map.h"
#else
#include "octeon-boot-info.h"
#endif

#endif


#ifdef __OCTEON_NEWLIB__
#pragma weak cvmx_uart_enable_intr
int cvmx_debug_uart = 1;
#endif


/* Default to second uart port for backward compatibility.  The default (if
   -debug does not set the uart number) can now be overridden with
   CVMX_DEBUG_COMM_UART_NUM. */
#ifndef CVMX_DEBUG_COMM_UART_NUM
# define CVMX_DEBUG_COMM_UART_NUM 1
#endif

static CVMX_SHARED cvmx_spinlock_t cvmx_debug_uart_lock;

/**
 * Interrupt handler for debugger Control-C interrupts.
 *
 * @param irq_number IRQ interrupt number
 * @param registers  CPU registers at the time of the interrupt
 * @param user_arg   Unused user argument
 */
void cvmx_debug_uart_process_debug_interrupt(int irq_number, uint64_t registers[32], void *user_arg)
{
    cvmx_uart_lsr_t lsrval;

    /* Check for a Control-C interrupt from the debugger. This loop will eat
        all input received on the uart */
    lsrval.u64 = cvmx_read_csr(CVMX_MIO_UARTX_LSR(cvmx_debug_uart));
    while (lsrval.s.dr)
    {
        int c = cvmx_read_csr(CVMX_MIO_UARTX_RBR(cvmx_debug_uart));
        if (c == '\003')
        {
            register uint64_t tmp;
#ifndef CVMX_BUILD_FOR_LINUX_KERNEL
            fflush(stderr);
            fflush(stdout);
#endif
            /* Pulse MCD0 signal on Ctrl-C to stop all the cores. Also
                set the MCD0 to be not masked by this core so we know
                the signal is received by someone */
            asm volatile (
                "dmfc0 %0, $22\n"
                "ori   %0, %0, 0x1110\n"
                "dmtc0 %0, $22\n"
                : "=r" (tmp));
        }
        lsrval.u64 = cvmx_read_csr(CVMX_MIO_UARTX_LSR(cvmx_debug_uart));
    }
}


static void cvmx_debug_uart_init(void)
{
    if (cvmx_debug_uart == -1)
	cvmx_debug_uart = CVMX_DEBUG_COMM_UART_NUM;
}

static void cvmx_debug_uart_install_break_handler(void)
{
#ifndef CVMX_BUILD_FOR_LINUX_KERNEL
#ifdef __OCTEON_NEWLIB__
    if (cvmx_uart_enable_intr)
#endif
        cvmx_uart_enable_intr(cvmx_debug_uart, cvmx_debug_uart_process_debug_interrupt);
#endif
}

/* Get a packet from the UART, return 0 on failure and 1 on success. */

static int cvmx_debug_uart_getpacket(char *buffer, size_t size)
{
    while (1)
    {
	unsigned char checksum;
        int timedout = 0;
	size_t count;
	char ch;

        ch = cvmx_uart_read_byte_with_timeout(cvmx_debug_uart, &timedout, __SHRT_MAX__);

        if (timedout)
            return 0;

        /* if this is not the start character, ignore it. */
        if (ch != '$')
            continue;

        retry:
        checksum = 0;
        count = 0;

        /* now, read until a # or end of buffer is found */
        while (count < size)
        {
            ch = cvmx_uart_read_byte(cvmx_debug_uart);
            if (ch == '$')
                goto retry;
            if (ch == '#')
                break;
            checksum = checksum + ch;
            buffer[count] = ch;
            count = count + 1;
        }
        buffer[count] = 0;

        if (ch == '#')
        {
	    char csumchars[2];
	    unsigned xmitcsum;
	    int n;

            csumchars[0] = cvmx_uart_read_byte(cvmx_debug_uart);
            csumchars[1] = cvmx_uart_read_byte(cvmx_debug_uart);
	    n = sscanf(csumchars, "%2x", &xmitcsum);
	    if (n != 1)
		return 1;

            return checksum == xmitcsum;
        }
    }
    return 0;
}

static int cvmx_debug_uart_putpacket(char *packet)
{
    size_t i;
    unsigned char csum;
    unsigned char *ptr = (unsigned char *) packet;
    char csumstr[3];

    for (csum = 0, i = 0; ptr[i]; i++)
      csum += ptr[i];
    sprintf(csumstr, "%02x", csum);

    cvmx_spinlock_lock(&cvmx_debug_uart_lock);
    cvmx_uart_write_byte(cvmx_debug_uart, '$');
    cvmx_uart_write_string(cvmx_debug_uart, packet);
    cvmx_uart_write_byte(cvmx_debug_uart, '#');
    cvmx_uart_write_string(cvmx_debug_uart, csumstr);
    cvmx_spinlock_unlock(&cvmx_debug_uart_lock);

    return 0;
}

static void cvmx_debug_uart_change_core(int oldcore, int newcore)
{
#ifndef CVMX_BUILD_FOR_LINUX_KERNEL
    cvmx_ciu_intx0_t irq_control;

    irq_control.u64 = cvmx_read_csr(CVMX_CIU_INTX_EN0(newcore * 2));
    irq_control.s.uart |= (1<<cvmx_debug_uart);
    cvmx_write_csr(CVMX_CIU_INTX_EN0(newcore * 2), irq_control.u64);

    /* Disable interrupts to this core since he is about to die */
    irq_control.u64 = cvmx_read_csr(CVMX_CIU_INTX_EN0(oldcore * 2));
    irq_control.s.uart &= ~(1<<cvmx_debug_uart);
    cvmx_write_csr(CVMX_CIU_INTX_EN0(oldcore* 2), irq_control.u64);
#endif
}

cvmx_debug_comm_t cvmx_debug_uart_comm =
{
  .init = cvmx_debug_uart_init,
  .install_break_handler = cvmx_debug_uart_install_break_handler,
  .needs_proxy = 1,
  .getpacket = cvmx_debug_uart_getpacket,
  .putpacket = cvmx_debug_uart_putpacket,
  .wait_for_resume = NULL,
  .change_core = cvmx_debug_uart_change_core,
};
