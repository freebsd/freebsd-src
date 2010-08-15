/***********************license start***************
 * Copyright (c) 2003-2007, Cavium Networks. All rights reserved.
 *
 * This software file (the "File") is owned and distributed by Cavium
 * Networks ("Cavium") under the following dual licensing option: The dual
 * licensing option gives you, the licensee, the choice between the following
 * alternative licensing terms.  Once you have made an election to use the
 * File under one of the following alternative licensing terms (license
 * types) you are bound by the respective terms and you may distribute the
 * file (or any derivative thereof), to the extent allowed by the respective
 * licensing term, only if you (i) delete this introductory statement
 * regarding the dual licensing option from the file you will distribute,
 * (ii) delete the licensing term that you have elected NOT to use from the
 * file you will distribute and (iii) follow the respective licensing term
 * that you have elected to use with respect to the correct attribution or
 * licensing term that you have to include with your distribution.
 *
 * ***
 * OCTEON SDK License Type 2:
 *
 * IMPORTANT: Read this Agreement carefully before clicking on the "I accept"
 * button to download the Software and/or before using the Software.  This
 * License Agreement (the "Agreement") is a legal agreement between you,
 * either an individual or a single legal entity ("You" or "you"), and Cavium
 * Networks ("Cavium").  This Agreement governs your use of the Cavium
 * software that can be downloaded after accepting this Agreement and/or that
 * is accompanied by this Agreement (the "Software").  You must accept the
 * terms of this Agreement before downloading and/or using the Software.  By
 * clicking on the "I accept" button to download and/or by using the
 * Software, you are indicating that you have read and understood, and assent
 * to be bound by, the terms of this Agreement.  If you do not agree to the
 * terms of the Agreement, you are not granted any rights whatsoever in the
 * Software.  If you are not willing to be bound by these terms and
 * conditions, you should not use or cease all use of the Software.  This
 * Software is the property of Cavium Networks and constitutes the
 * proprietary information of Cavium Networks.  You agree to take reasonable
 * steps to prevent the disclosure, unauthorized use or unauthorized
 * distribution of the Software to any third party.
 *
 * License Grant.  Subject to the terms and conditions of this Agreement,
 * Cavium grants you a nonexclusive, non-transferable, worldwide, fully-paid
 * and royalty-free license to
 *
 * (a) install, reproduce, and execute the executable version of the Software
 * solely for your internal use and only (a) on hardware manufactured by
 * Cavium, or (b) software of Cavium that simulates Cavium hardware;
 *
 * (b) create derivative works of any portions of the Software provided to
 * you by Cavium in source code form, which portions enable features of the
 * Cavium hardware products you or your licensees are entitled to use,
 * provided that any such derivative works must be used only (a) on hardware
 * manufactured by Cavium, or (b) software of Cavium that simulates Cavium
 * hardware; and
 *
 * (c) distribute derivative works you created in accordance with clause (b)
 * above, only in executable form and only if such distribution (i)
 * reproduces the copyright notice that can be found at the very end of this
 * Agreement and (ii) is pursuant to a binding license agreement that
 * contains terms no less restrictive and no less protective of Cavium than
 * this Agreement.  You will immediately notify Cavium if you become aware of
 * any breach of any such license agreement.
 *
 * Restrictions.  The rights granted to you in this Agreement are subject to
 * the following restrictions: Except as expressly set forth in this
 * Agreement (a) you will not license, sell, rent, lease, transfer, assign,
 * display, host, outsource, disclose or otherwise commercially exploit or
 * make the Software, or any derivatives you create under this Agreement,
 * available to any third party; (b) you will not modify or create derivative
 * works of any part of the Software; (c) you will not access or use the
 * Software in order to create similar or competitive products, components,
 * or services; and (d), no part of the Software may be copied (except for
 * the making of a single archival copy), reproduced, distributed,
 * republished, downloaded, displayed, posted or transmitted in any form or
 * by any means.
 *
 * Ownership.  You acknowledge and agree that, subject to the license grant
 * contained in this Agreement and as between you and Cavium (a) Cavium owns
 * all copies of and intellectual property rights to the Software, however
 * made, and retains all rights in and to the Software, including all
 * intellectual property rights therein, and (b) you own all the derivate
 * works of the Software created by you under this Agreement, subject to
 * Cavium's rights in the Software.  There are no implied licenses under this
 * Agreement, and any rights not expressly granted to your hereunder are
 * reserved by Cavium.  You will not, at any time, contest anywhere in the
 * world Cavium's ownership of the intellectual property rights in and to the
 * Software.
 *
 * Disclaimer of Warranties.  The Software is provided to you free of charge,
 * and on an "As-Is" basis.  Cavium provides no technical support, warranties
 * or remedies for the Software.  Cavium and its suppliers disclaim all
 * express, implied or statutory warranties relating to the Software,
 * including but not limited to, merchantability, fitness for a particular
 * purpose, title, and non-infringement.  Cavium does not warrant that the
 * Software and the use thereof will be error-free, that defects will be
 * corrected, or that the Software is free of viruses or other harmful
 * components.  If applicable law requires any warranties with respect to the
 * Software, all such warranties are limited in duration to thirty (30) days
 * from the date of download or first use, whichever comes first.
 *
 * Limitation of Liability.  Neither Cavium nor its suppliers shall be
 * responsible or liable with respect to any subject matter of this Agreement
 * or terms or conditions related thereto under any contract, negligence,
 * strict liability or other theory (a) for loss or inaccuracy of data or
 * cost of procurement of substitute goods, services or technology, or (b)
 * for any indirect, incidental or consequential damages including, but not
 * limited to loss of revenues and loss of profits.  Cavium's aggregate
 * cumulative liability hereunder shall not exceed the greater of Fifty U.S.
 * Dollars (U.S.$50.00) or the amount paid by you for the Software that
 * caused the damage.  Certain states and/or jurisdictions do not allow the
 * exclusion of implied warranties or limitation of liability for incidental
 * or consequential damages, so the exclusions set forth above may not apply
 * to you.
 *
 * Basis of Bargain.  The warranty disclaimer and limitation of liability set
 * forth above are fundamental elements of the basis of the agreement between
 * Cavium and you.  Cavium would not provide the Software without such
 * limitations.  The warranty disclaimer and limitation of liability inure to
 * the benefit of Cavium and Cavium's suppliers.
 *
 * Term and Termination.  This Agreement and the licenses granted hereunder
 * are effective on the date you accept the terms of this Agreement, download
 * the Software, or use the Software, whichever comes first, and shall
 * continue unless this Agreement is terminated pursuant to this section.
 * This Agreement immediately terminates in the event that you materially
 * breach any of the terms hereof.  You may terminate this Agreement at any
 * time, with or without cause, by destroying any copies of the Software in
 * your possession.  Upon termination, the license granted hereunder shall
 * terminate but the Sections titled "Restrictions", "Ownership", "Disclaimer
 * of Warranties", "Limitation of Liability", "Basis of Bargain", "Term and
 * Termination", "Export", and "Miscellaneous" will remain in effect.
 *
 * Export.  The Software and related technology are subject to U.S.  export
 * control laws and may be subject to export or import regulations in other
 * countries.  You agree to strictly comply with all such laws and
 * regulations and acknowledges that you have the responsibility to obtain
 * authorization to export, re-export, or import the Software and related
 * technology, as may be required.  You will indemnify and hold Cavium
 * harmless from any and all claims, losses, liabilities, damages, fines,
 * penalties, costs and expenses (including attorney's fees) arising from or
 * relating to any breach by you of your obligations under this section.
 * Your obligations under this section shall survive the expiration or
 * termination of this Agreement.
 *
 * Miscellaneous.  Neither the rights nor the obligations arising under this
 * Agreement are assignable by you, and any such attempted assignment or
 * transfer shall be void and without effect.  This Agreement shall be
 * governed by and construed in accordance with the laws of the State of
 * California without regard to any conflicts of laws provisions that would
 * require application of the laws of another jurisdiction.  Any action under
 * or relating to this Agreement shall be brought in the state and federal
 * courts located in California, with venue in the courts located in Santa
 * Clara County and each party hereby submits to the personal jurisdiction of
 * such courts; provided, however, that nothing herein will operate to
 * prohibit or restrict Cavium from filing for and obtaining injunctive
 * relief from any court of competent jurisdiction.  The United Nations
 * Convention on Contracts for the International Sale of Goods shall not
 * apply to this Agreement.  In the event that any provision of this
 * Agreement is found to be contrary to law, then such provision shall be
 * construed as nearly as possible to reflect the intention of the parties,
 * with the other provisions remaining in full force and effect.  Any notice
 * to you may be provided by email.  This Agreement constitutes the entire
 * agreement between the parties and supersedes all prior or contemporaneous,
 * agreements, understandings and communications between the parties, whether
 * written or oral, pertaining to the subject matter hereof.  Any
 * modifications of this Agreement must be in writing and agreed to by both
 * parties.
 *
 * Copyright (c) 2003-2007, Cavium Networks. All rights reserved.
 *
 * ***
 *
 * OCTEON SDK License Type 4:
 *
 * Author: Cavium Networks
 *
 * Contact: support@caviumnetworks.com
 * This file is part of the OCTEON SDK
 *
 * Copyright (c) 2007 Cavium Networks
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, Version 2, as published by
 * the Free Software Foundation.
 *
 * This file is distributed in the hope that it will be useful,
 * but AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or NONINFRINGEMENT.
 * See the GNU General Public License for more details.
 * it under the terms of the GNU General Public License, Version 2, as published by
 * the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 * or visit http://www.gnu.org/licenses/.
 *
 * This file may also be available under a different license from Cavium.
 * Contact Cavium Networks for more information
 ***********************license end**************************************/


/*
 * This code is an example of using twsi core in raw mode, bypasing High 
 * Level Controller (HLC). It is recommended to use HLC if only possible as 
 * it is more efficient and robust mechanism. 
 * The example code shows use of twsi for generating long (more that 8 bytes HLC limit) 
 * read - write transactions using 7-bit addressing. Different types of 
 * transactions can be generated if needed. Make sure that commands written to twsi core 
 * follow core state transitions outlinged in OCTEON documentation. The core state is 
 * reported in stat register after the command colpletion. In each state core will accept
 *  only the allowed commands.
 */
 
#include <stdio.h>
#include <cvmx.h>
#include <cvmx-csr-typedefs.h>
#include "cvmx-twsi-raw.h"

/*
 * uint8_t cvmx_twsix_read_ctr(int twsi_id, uint8_t reg)
 * twsi core register read
 * twsi_id - twsi core index
 * reg 0 - 8-bit register
 * returns 8-bit register contetn
 */
uint8_t cvmx_twsix_read_ctr(int twsi_id, uint8_t reg)
{
    cvmx_mio_twsx_sw_twsi_t sw_twsi_val;

    sw_twsi_val.u64 = 0;
    sw_twsi_val.s.v = 1;
    sw_twsi_val.s.op = 6;
    sw_twsi_val.s.eop_ia = reg;
    sw_twsi_val.s.r = 1;
    cvmx_write_csr(CVMX_MIO_TWSX_SW_TWSI(twsi_id), sw_twsi_val.u64);
    while (((cvmx_mio_twsx_sw_twsi_t)(sw_twsi_val.u64 = cvmx_read_csr(CVMX_MIO_TWSX_SW_TWSI(twsi_id)))).s.v)
        ;
    return sw_twsi_val.s.d ;
}

/*
 * uint8_t cvmx_twsix_write_ctr(int twsi_id, uint8_t reg, uint8_t data)
 *
 * twsi core register write
 * twsi_id - twsi core index
 * reg 0 - 8-bit register
 * data - data to write
 * returns 0;
 */

int cvmx_twsix_write_ctr(int twsi_id, uint8_t reg, uint8_t data)
{
    cvmx_mio_twsx_sw_twsi_t sw_twsi_val;

    sw_twsi_val.u64 = 0;
    sw_twsi_val.s.v = 1;
    sw_twsi_val.s.op = 6;
    sw_twsi_val.s.eop_ia = reg;
    sw_twsi_val.s.d = data;
    cvmx_write_csr(CVMX_MIO_TWSX_SW_TWSI(twsi_id), sw_twsi_val.u64);
    while (((cvmx_mio_twsx_sw_twsi_t)(sw_twsi_val.u64 = cvmx_read_csr(CVMX_MIO_TWSX_SW_TWSI(twsi_id)))).s.v)
        ;

    return 0;
}

/*
 * cvmx_twsi_wait_iflg(int twsi_id)
 * cvmx_twsi_wait_stop(int twsi_id)
 *
 * Helper functions. 
 * Busy wait for interrupt flag or stop bit on control register. This implementation is for OS-less 
 * application. With OS services available it could be implemented with semaphore
 * block and interrupt wake up. 
 * TWSI_WAIT for loop must be defined large enough to allow on-wire transaction to finish - that is 
 * about 10 twsi clocks
 */
#define TWSI_WAIT 10000000
static inline int cvmx_twsi_wait_iflg(int twsi_id)
{
    octeon_twsi_ctl_t ctl_reg;
    int wait = TWSI_WAIT;
    do{
        ctl_reg.u8 = cvmx_twsix_read_ctr(twsi_id, TWSI_CTL_REG);
    } while((ctl_reg.s.iflg ==0) && (wait-- >0));
    if(wait == 0) return -1;
    return 0;
}

static inline int cvmx_twsi_wait_stop(int twsi_id)
{
    octeon_twsi_ctl_t ctl_reg;
    int wait = TWSI_WAIT;
    do{
        ctl_reg.u8 = cvmx_twsix_read_ctr(twsi_id, TWSI_CTL_REG);
    } while((ctl_reg.s.stp ==1) && (wait-- >0));
    if(wait == 0) return -1;
    return 0;
}


/*
 * uint8_t octeon_twsi_read_byte(int twsi_id, uint8_t* byte, int ack) 
 * uint8_t octeon_twsi_write_byte(int twsi_id, uint8_t byte)
 *
 * helper functions - read or write byte to data reg and reads the TWSI core status
 */
static uint8_t octeon_twsi_read_byte(int twsi_id, uint8_t* byte, int ack)
{
    octeon_twsi_ctl_t ctl_reg;
    octeon_twsi_data_t data;
    octeon_twsi_stat_t stat;

    /* clear interrupt flag, set aak for requested ACK signal level */
    ctl_reg.u8 =0;
    ctl_reg.s.aak = (ack==0) ?0:1;
    ctl_reg.s.enab =1;
    cvmx_twsix_write_ctr(twsi_id, TWSI_CTL_REG, ctl_reg.u8);

    /* wait for  twsi_ctl[iflg] to be set */
    if(cvmx_twsi_wait_iflg(twsi_id)) goto error;

    /* read the byte */
    data.u8 =cvmx_twsix_read_ctr(twsi_id, TWSI_DATA_REG);
    *byte = data.s.data;
error:
    /* read the status */
    stat.u8 = cvmx_twsix_read_ctr(twsi_id, TWSI_STAT_REG);
    return stat.s.stat;
}

static uint8_t octeon_twsi_write_byte(int twsi_id, uint8_t byte)
{
    octeon_twsi_ctl_t ctl_reg;
    octeon_twsi_data_t data;
    octeon_twsi_stat_t stat;

    /* tx data byte - write to twsi_data reg, then clear twsi_ctl[iflg] */
    data.s.data = byte;
    cvmx_twsix_write_ctr(twsi_id, TWSI_DATA_REG, data.u8);

    ctl_reg.u8 = cvmx_twsix_read_ctr(twsi_id, TWSI_CTL_REG);
    ctl_reg.s.iflg =0;
    cvmx_twsix_write_ctr(twsi_id, TWSI_CTL_REG, ctl_reg.u8);

    /* wait for  twsi_ctl[iflg] to be set */
    if(cvmx_twsi_wait_iflg(twsi_id)) goto error;
error:
     /* read the status */
     stat.u8 = cvmx_twsix_read_ctr(twsi_id, TWSI_STAT_REG);
     return stat.s.stat;
}

/*
 * int octeon_i2c_xfer_msg_raw(struct i2c_msg *msg)
 *
 * Send (read or write) a message with 7-bit address device over direct control of 
 * TWSI core, bypassind HLC. Will try to finish the transaction on failure, so core state
 * expected to be idle with HLC enabled on exit.
 *
 * dev - TWSI controller index (0 for cores with single controler)
 * msg - message to transfer
 * returns 0 on success, TWSI core state on error. Will try to finish the transaction on failure, so core state expected to be idle
 */
int octeon_i2c_xfer_msg_raw(int twsi_id, struct i2c_msg *msg)
{
    int i =0;
    octeon_twsi_ctl_t ctl_reg;
    octeon_twsi_addr_t addr;
    octeon_twsi_stat_t stat;
    int is_read = msg->flags & I2C_M_RD;
    int ret =0;

    /* check the core state, quit if not idle */
    stat.u8 =cvmx_twsix_read_ctr(twsi_id, TWSI_STAT_REG);
    if(stat.s.stat != TWSI_IDLE) {
       msg->len =0; return stat.s.stat;
    }

    /* first send start - set twsi_ctl[sta] to 1 */
    ctl_reg.u8 =0;
    ctl_reg.s.enab =1;
    ctl_reg.s.sta =1;
    ctl_reg.s.iflg =0;
    cvmx_twsix_write_ctr(twsi_id, TWSI_CTL_REG, ctl_reg.u8);
    /* wait for  twsi_ctl[iflg] to be set */
    if(cvmx_twsi_wait_iflg(twsi_id)) goto stop;

    /* Write 7-bit addr to twsi_data; set read bit */
    addr.s.slave_addr7 = msg->addr;
    if(is_read) addr.s.r =1;
    else addr.s.r =0;
    stat.s.stat =octeon_twsi_write_byte(twsi_id, addr.u8);

    /* Data read loop */
    if( is_read) {
    /* any status but ACK_RXED means failure - we try to send stop and go idle */
      if(!(stat.s.stat == TWSI_ADDR_R_TX_ACK_RXED)) {
      ret = stat.s.stat;
      msg->len =0;
      goto stop;
     }
     /* We read data from the buffer and send ACK back.
       The last byte we read with negative ACK */
      for(i =0; i<msg->len-1; i++)
      {
        stat.s.stat =octeon_twsi_read_byte(twsi_id, &msg->buf[i], 1);
        if(stat.s.stat != TWSI_DATA_RX_ACK_TXED)
           goto stop;
      }
      /* last read we send negACK */
      stat.s.stat =octeon_twsi_read_byte(twsi_id, &msg->buf[i], 0);
        if(stat.s.stat != TWSI_DATA_RX_NACK_TXED)
             return stat.s.stat;
    } /* read loop */

    /* Data write loop */
    else {
    /* any status but ACK_RXED means failure - we try to send stop and go idle */
      if(stat.s.stat != TWSI_ADDR_W_TX_ACK_RXED) {
          ret = stat.s.stat;
          msg->len =0;
          goto stop;
      }
      /* We write data to the buffer and check for ACK. */
      for(i =0; i<msg->len; i++)
      {
          stat.s.stat =octeon_twsi_write_byte(twsi_id, msg->buf[i]);
          if(stat.s.stat == TWSI_DATA_TX_NACK_RXED) {
              /* Negative ACK means slave can not RX more */
              msg->len =i-1;
              goto stop;
          }
          else if(stat.s.stat != TWSI_DATA_TX_ACK_RXED) {
              /* lost arbitration? try to send stop and go idle. This current byte likely was not written */
              msg->len = (i-2) >0? (i-2):0;
              goto stop;
          }
      }
    }  /* write loop */

stop:
    ctl_reg.u8 =cvmx_twsix_read_ctr(twsi_id, TWSI_CTL_REG);
    ctl_reg.s.stp =1;
    ctl_reg.s.iflg =0;
    cvmx_twsix_write_ctr(twsi_id, TWSI_CTL_REG, ctl_reg.u8);
    /* wait for  twsi_ctl[stp] to clear */
    cvmx_twsi_wait_stop(twsi_id);
#if 0
    stat.u8 = cvmx_twsix_read_ctr(twsi_id, TWSI_STAT_REG);
    if(stat.s.stat == TWSI_IDLE)
#endif
    /* Leave TWSI core with HLC eabled */
    {
       ctl_reg.u8 =0;
       ctl_reg.s.ce =1;
       ctl_reg.s.enab =1;
       ctl_reg.s.aak =1;
       cvmx_twsix_write_ctr(twsi_id, TWSI_CTL_REG, ctl_reg.u8);
    }

    return ret;
}

