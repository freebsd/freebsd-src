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


#ifndef __CVMX_TWSI_RAW_H__
#define __CVMX_TWSI_RAW_H__

/* Addresses for twsi 8-bit registers. Gets written to EOP_IA field of MIO_TWS_SW_TWSI reg
*  when OP = 6 and SLONLY =0 */
#define TWSI_SLAVE_ADD_REG      0
#define TWSI_DATA_REG           1
#define TWSI_CTL_REG            2
#define TWSI_STAT_REG	        3 /* read only */
#define TWSI_CLKCTL_REG 	3 /* write only */
#define TWSI_SLAVE_EXTADD_REG   4       
#define TWSI_RST_REG            7

/* twsi core slave address reg */
typedef union{
    uint8_t u8;
    struct{
        uint8_t slave_addr7     : 7;
        uint8_t gce    : 1;

    }s;
} octeon_twsi_slave_add_t;

/* twsi core 10-bit slave address reg */
typedef union{
    uint8_t u8;
    struct{
        uint8_t slave_addr8     : 8;
    }s;
} octeon_twsi_slave_extadd_t;

/* twsi core control register */
typedef union{
    uint8_t u8;
    struct{
        uint8_t ce     : 1;  /* enable HLC*/
        uint8_t enab   : 1;  /* bus enable */
        uint8_t sta    : 1;  /* start request */
        uint8_t stp    : 1;  /* stop request */
        uint8_t iflg   : 1;  /* interrupt flag - request completed (1) start new (0) */
        uint8_t aak    : 1;  /* assert ack (1) -neg ack at end of Rx sequence */ 
        uint8_t rsv    : 2;  /* not used */
    }s;
} octeon_twsi_ctl_t;

/* clock dividers register */
typedef union{
    uint8_t u8;
    struct{
        uint8_t m_divider   : 4;
        uint8_t n_divider   : 3;
    }s;
} octeon_twsi_clkctl_t;

/* address of the remote slave + r/w bit */
typedef union{
    uint8_t u8;
    struct{
        uint8_t slave_addr7     : 7; 
	uint8_t r		: 1; /* read (1) write (0) bit */
    }s;
} octeon_twsi_addr_t;

/* core state reg */
typedef union{
    uint8_t u8;
    struct{
        uint8_t stat    : 8;
    }s;
} octeon_twsi_stat_t;

 /* data byte reg */
typedef union{
    uint8_t u8;
    struct{
        uint8_t data    : 8;
    }s;
} octeon_twsi_data_t;

/* twsi core states as reported in twsi core stat register */
#define TWSI_BUS_ERROR 			0x00
#define TWSI_START_TXED 		0x08
#define TWSI_ADDR_W_TX_ACK_RXED 	0x18
#define TWSI_ADDR_W_TX_NACK_RXED 	0x20

#define TWSI_DATA_TX_ACK_RXED 		0x28
#define TWSI_DATA_TX_NACK_RXED 	0x30
#define TWSI_ARB_LOST			0x38
#define TWSI_ADDR_R_TX_ACK_RXED		0x40

#define TWSI_ADDR_R_TX_NACK_RXED	0x48
#define TWSI_DATA_RX_ACK_TXED		0x50
#define TWSI_DATA_RX_NACK_TXED		0x58
#define TWSI_SLAVE_ADDR_RX_ACK_TXED	0x60

#define TWSI_ARB_LOST_SLAVE_ADDR_RX	0x68
#define TWSI_GEN_ADDR_RXED_ACK_TXED	0x70
#define TWSI_ARB_LOST_GEN_ADDR_RXED	0x78
#define TWSI_SLAVE_DATA_RX_ACK_TXED	0x80

#define TWSI_SLAVE_DATA_RX_NACK_TXED	0x88
#define TWSI_GEN_DATA_RX_ACK_TXED	0x90
#define TWSI_GEN_DATA_RX_NACK_TXED	0x98
#define TWSI_SLAVE_STOP_OR_START_RXED	0xa0

#define TWSI_SLAVE_ADDR_R_RX_ACK_TXED	0xa8
#define TWSI_ARB_LOST_SLAVE_ADDR_R_RX_ACK_TXED 0xb0
#define TWSI_SLAVE_DATA_TX_ACK_RXED	0xb8
#define TWSI_SLAVE_DATA_TX_NACK_RXED	0xc0

#define TWSI_SLAVE_LAST_DATA_TX_ACK_RXED 0xc8
#define TWSI_SECOND_ADDR_W_TX_ACK_RXED	0xd0
#define TWSI_SECOND_ADDR_W_TX_NACK_RXED	0xd8
#define TWSI_IDLE			0xf8

#ifndef LINUX
/* msg definition similar to Linux */
struct i2c_msg {
        uint16_t addr;     /* slave address                        */
        uint16_t flags;
        uint16_t len;              /* msg length                           */
        uint8_t *buf;              /* pointer to msg data                  */
};
#define I2C_M_TEN       0x10    /* we have a ten bit chip address       */
#define I2C_M_RD        0x01
#endif

int octeon_i2c_xfer_msg_raw(int twsi_id, struct i2c_msg *msg);

#endif
