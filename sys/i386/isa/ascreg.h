/* ascreg.h - port and bit definitions for the GI-1904 interface 
 *
 * Copyright (c) 1995 Gunther Schadow.  All rights reserved.
 * Copyright (c) 1995 Luigi Rizzo.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Gunther Schadow.
 *	and Luigi Rizzo
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * $FreeBSD: src/sys/i386/isa/ascreg.h,v 1.5 1999/08/28 00:44:37 peter Exp $
 */

    /***    Registers (base=3EB): ************/
#define	ASC_CFG	(scu->base)
    /*** ASC_CFG 3EB: configuration register. Write only, mirror in RAM
     ***   7   6   5   4   3   2   1   0
     ***   -   -  I_5 I_3 I10 D_3  -  D_1
     ***/
    /*** #define ASC_CNF_MASK  0x3D */ /*  was 0x5a */
#define ASC_CNF_DMA1  0x01 /* was (~0x02 & ASC_CNF_MASK) */
#define ASC_CNF_DMA3  0x04 /* was (~0x08 & ASC_CNF_MASK) */
#define ASC_CNF_IRQ3  0x10 /* was (~0x10 & ASC_CNF_MASK) */
#define ASC_CNF_IRQ5  0x20 /* was (~0x40 & ASC_CNF_MASK) */
#define ASC_CNF_IRQ10 0x08 /* was (~0x40 & ASC_CNF_MASK) */

    /*** ASC_STAT 3EC: command/status; rw, mirror in ram
     ***    7   6   5   4   3   2   1   0
     ***   BSY  -   -   -   -   -   -   -
     ***            [<--  Resolution -->] 13h,10h,0eh,0ch,09h, 07h, 04h, 02h
     ***/
#define ASC_STAT	(scu->base + 1)

#define ASC_RDY_FLAG  0x80
#define ASC_RES_MASK  0x3f
#define ASC_RES_800   0x13
#define ASC_RES_700   0x10
#define ASC_RES_600   0x0e
#define ASC_RES_500   0x0c
#define ASC_RES_400   0x09 /* 0x00 */
#define ASC_RES_300   0x07 /* 0x04 */
#define ASC_RES_200   0x04 /* 0x20 */
#define ASC_RES_100   0x02 /* 0x24 */

    /*** ASC_CMD 3EC: command/status; rw, mirror in ram
     *** W:  7   6   5   4   3   2   1   0
     ***     .   -   -   .   .   .   .   .  
     *** b0: 1: light on & get resolution, 0: light off
     *** b1: 0: load scan len (sub_16, with b4=1, b7=1)
     *** b2: 1/0 : dma stuff
     *** b3: 0/1 : dma stuff
     *** b4: 1   : load scan len (sub_16, with b1=0, b7=1)
     *** b5: ?    
     *** b6: ?    
     *** b7: ?   : set at beginning of sub_16
     ***/
#define ASC_CMD	(scu->base + 1)

#define ASC_LIGHT_ON  0x01
#define ASC_SET_B2    0x04
#define ASC_OPERATE	0x91	/* from linux driver... */
#define ASC_STANDBY	0x05	/* from linux driver... */

    /*** ASC_LEN_L, ASC_LEN_H 3ED, 3EE: transfer length, lsb first ***/
#define ASC_LEN_L	((scu->base)+2)
#define ASC_LEN_H	((scu->base)+3)

    /*** 3EE ASC_PROBE (must read ASC_PROBE_VALUE) ***/
#define ASC_PROBE	((scu->base)+3)
#define ASC_PROBE_VALUE	0xA5

    /*** ASC_BOH 3EF: always write 0 at the moment, read some values ?  ***/
#define ASC_BOH		((scu->base)+4)
