/* Copyright (c) 2008-2011 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __DTSEC_MII_ACC_H
#define __DTSEC_MII_ACC_H

#include "std_ext.h"


/* MII Management Configuration Register */
#define MIIMCFG_RESET_MGMT          0x80000000
#define MIIMCFG_MGMT_CLOCK_SELECT   0x00000007

/* MII  Management Command Register */
#define MIIMCOM_READ_CYCLE          0x00000001
#define MIIMCOM_SCAN_CYCLE          0x00000002

/* MII  Management Address Register */
#define MIIMADD_PHY_ADDR_SHIFT      8

/* MII Management Indicator Register */
#define MIIMIND_BUSY                0x00000001


#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(push,1)
#endif /* defined(__MWERKS__) && ... */
#define MEM_MAP_START

/*----------------------------------------------------*/
/* MII Configuration Control Memory Map Registers     */
/*----------------------------------------------------*/
typedef _Packed struct t_MiiAccessMemMap
{
    volatile uint32_t miimcfg;    /* MII Mgmt:configuration */
    volatile uint32_t miimcom;    /* MII Mgmt:command       */
    volatile uint32_t miimadd;    /* MII Mgmt:address       */
    volatile uint32_t miimcon;    /* MII Mgmt:control 3     */
    volatile uint32_t miimstat;   /* MII Mgmt:status        */
    volatile uint32_t miimind;    /* MII Mgmt:indicators    */
} _PackedType t_MiiAccessMemMap ;

#define MEM_MAP_END
#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(pop)
#endif /* defined(__MWERKS__) && ... */


#endif /* __DTSEC_MII_ACC_H */
