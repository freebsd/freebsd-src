/* gscreg.h - port and bit definitions for the Genius GS-4500 interface
 *
 *
 * Copyright (c) 1995 Gunther Schadow.  All rights reserved.
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
 * status register (r)
 */

/* the DMA/IRQ jumper configuration */

#define GSC_CNF_MASK  0x5a

#define GSC_CNF_DMA1 (~0x02 & GSC_CNF_MASK)
#define GSC_CNF_DMA3 (~0x08 & GSC_CNF_MASK)
#define GSC_CNF_IRQ3 (~0x10 & GSC_CNF_MASK)
#define GSC_CNF_IRQ5 (~0x40 & GSC_CNF_MASK)

/* the resolution switch setting */

#define GSC_RES_MASK  0x24

#define GSC_RES_400   0x00
#define GSC_RES_300   0x04
#define GSC_RES_200   0x20
#define GSC_RES_100   0x24

/* other flags */

#define GSC_RDY_FLAG  0x80

#define GSC_IRQ_FLAG  0x01

/*
 * control register (w)
 */

/* power on */

#define GSC_POWER_ON  0x01

/* pixel per line count */

#define GSC_CNT_MASK  0xf0

#define GSC_CNT_3648  0x30
#define GSC_CNT_2544  0x90
#define GSC_CNT_1696  0xb0
#define GSC_CNT_1648  0xe0
#define GSC_CNT_1264  0x80
#define GSC_CNT_840   0xa0
#define GSC_CNT_424   0xf0

/*
 * port addresses
 */

#define GSC_DATA(iob) (iob + (iob == 0x270 ? 0x02 : 0x01))
#define GSC_STAT(iob) (iob + (iob == 0x270 ? 0x03 : 0x02))
#define GSC_CTRL(iob) (iob + (iob == 0x270 ? 0x0a : 0x03))
#define GSC_CLRP(iob) (iob + (iob == 0x270 ? 0x0b : 0x04))
