/*
 * Copyright (c) 1995 John Hay.  All rights reserved.
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
 *	This product includes software developed by [your name]
 *	and [any other names deserving credit ]
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY [your name] AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/i386/isa/ic/hd64570.h,v 1.6 1999/08/28 00:45:13 peter Exp $
 */
#ifndef _HD64570_H_
#define _HD64570_H_

typedef struct msci_channel
  {
  union
    {
    unsigned short us_trb;    /* rw */
    struct
      {
      unsigned char uc_trbl;
      unsigned char uc_trbh;
      }uc_trb;
    }u_trb;
  unsigned char st0;          /* ro */
  unsigned char st1;          /* rw */
  unsigned char st2;          /* rw */
  unsigned char st3;          /* ro */
  unsigned char fst;          /* rw */
  unsigned char unused0;
  unsigned char ie0;          /* rw */
  unsigned char ie1;          /* rw */
  unsigned char ie2;          /* rw */
  unsigned char fie;          /* rw */
  unsigned char cmd;          /* wo */
  unsigned char unused1;
  unsigned char md0;          /* rw */
  unsigned char md1;          /* rw */
  unsigned char md2;          /* rw */
  unsigned char ctl;          /* rw */
  unsigned char sa0;          /* rw */
  unsigned char sa1;          /* rw */
  unsigned char idl;          /* rw */
  unsigned char tmc;          /* rw */
  unsigned char rxs;          /* rw */
  unsigned char txs;          /* rw */
  unsigned char trc0;         /* rw */
  unsigned char trc1;         /* rw */
  unsigned char rrc;          /* rw */
  unsigned char unused2;
  unsigned char cst0;         /* rw */
  unsigned char cst1;         /* rw */
  unsigned char unused3[2];
  }msci_channel;

#define trb     u_trb.us_trb
#define trbl    u_trb.uc_trb.uc_trbl
#define trbh    u_trb.uc_trb.uc_trbh

typedef struct timer_channel
  {
  unsigned short tcnt;        /* rw */
  unsigned short tconr;       /* wo */
  unsigned char tcsr;         /* rw */
  unsigned char tepr;         /* rw */
  unsigned char unused[2];
  }timer_channel;

typedef struct dmac_channel
  {
  unsigned short dar;         /* rw */
  unsigned char darb;         /* rw */
  unsigned char unused0;
  unsigned short sar;         /* rw On odd numbered dmacs (tx) only */
  unsigned char sarb;         /* rw */
#define cpb sarb
  unsigned char unused1;
  unsigned short cda;         /* rw */
  unsigned short eda;         /* rw */
  unsigned short bfl;         /* rw On even numbered dmacs (rx) only */
  unsigned short bcr;         /* rw */
  unsigned char dsr;          /* rw */
  unsigned char dmr;          /* rw */
  unsigned char unused2;
  unsigned char fct;          /* rw */
  unsigned char dir;          /* rw */
  unsigned char dcr;          /* rw */
  unsigned char unused3[10];
  }dmac_channel;

/* x is the channel number. rx channels are even numbered and tx, odd. */
#define DMAC_RXCH(x)            ((x*2) + 0)
#define DMAC_TXCH(x)            ((x*2) + 1)

typedef struct sca_regs
  {
  unsigned char lpr;          /* rw */
  unsigned char unused0;      /* -- */
  /* Wait system */
  unsigned char pabr0;        /* rw */
  unsigned char pabr1;        /* rw */
  unsigned char wcrl;         /* rw */
  unsigned char wcrm;         /* rw */
  unsigned char wcrh;         /* rw */
  unsigned char unused1;
  /* DMAC */
  unsigned char pcr;          /* rw */
  unsigned char dmer;         /* rw */
  unsigned char unused2[6];
  /* Interrupt */
  unsigned char isr0;          /* ro */
  unsigned char isr1;          /* ro */
  unsigned char isr2;          /* ro */
  unsigned char unused3;
  unsigned char ier0;          /* rw */
  unsigned char ier1;          /* rw */
  unsigned char ier2;          /* rw */
  unsigned char unused4;
  unsigned char itcr;          /* rw */
  unsigned char unused5;
  unsigned char ivr;           /* rw */
  unsigned char unused6;
  unsigned char imvr;          /* rw */
  unsigned char unused7[3];
  /* MSCI Channel 0 */
  msci_channel  msci[2];
  timer_channel timer[4];
  dmac_channel  dmac[4];
  }sca_regs;

#define SCA_CMD_TXRESET         0x01
#define SCA_CMD_TXENABLE        0x02
#define SCA_CMD_TXDISABLE       0x03
#define SCA_CMD_TXCRCINIT       0x04
#define SCA_CMD_TXCRCEXCL       0x05
#define SCA_CMS_TXEOM           0x06
#define SCA_CMD_TXABORT         0x07
#define SCA_CMD_MPON            0x08
#define SCA_CMD_TXBCLEAR        0x09

#define SCA_CMD_RXRESET         0x11
#define SCA_CMD_RXENABLE        0x12
#define SCA_CMD_RXDISABLE       0x13
#define SCA_CMD_RXCRCINIT       0x14
#define SCA_CMD_RXMSGREJ        0x15
#define SCA_CMD_MPSEARCH        0x16
#define SCA_CMD_RXCRCEXCL       0x17
#define SCA_CMD_RXCRCCALC       0x18

#define SCA_CMD_NOP             0x00
#define SCA_CMD_RESET           0x21
#define SCA_CMD_SEARCH          0x31

#define SCA_MD0_CRC_1           0x01
#define SCA_MD0_CRC_CCITT       0x02
#define SCA_MD0_CRC_ENABLE      0x04
#define SCA_MD0_AUTO_ENABLE     0x10
#define SCA_MD0_MODE_ASYNC      0x00
#define SCA_MD0_MODE_BYTESYNC1  0x20
#define SCA_MD0_MODE_BISYNC     0x40
#define SCA_MD0_MODE_BYTESYNC2  0x60
#define SCA_MD0_MODE_HDLC       0x80

#define SCA_MD1_NOADDRCHK       0x00
#define SCA_MD1_SNGLADDR1       0x40
#define SCA_MD1_SNGLADDR2       0x80
#define SCA_MD1_DUALADDR        0xC0

#define SCA_MD2_DUPLEX          0x00
#define SCA_MD2_ECHO            0x01
#define SCA_MD2_LOOPBACK        0x03
#define SCA_MD2_ADPLLx8         0x00
#define SCA_MD2_ADPLLx16        0x08
#define SCA_MD2_ADPLLx32        0x10
#define SCA_MD2_NRZ             0x00
#define SCA_MD2_NRZI            0x20
#define SCA_MD2_MANCHESTER      0x80
#define SCA_MD2_FM0             0xC0
#define SCA_MD2_FM1             0xA0

#define SCA_CTL_RTS             0x01
#define SCA_CTL_IDLPAT          0x10
#define SCA_CTL_UDRNC           0x20

#define SCA_RXS_DIV_MASK        0x0F
#define SCA_RXS_DIV1            0x00
#define SCA_RXS_DIV2            0x01
#define SCA_RXS_DIV4            0x02
#define SCA_RXS_DIV8            0x03
#define SCA_RXS_DIV16           0x04
#define SCA_RXS_DIV32           0x05
#define SCA_RXS_DIV64           0x06
#define SCA_RXS_DIV128          0x07
#define SCA_RXS_DIV256          0x08
#define SCA_RXS_DIV512          0x09
#define SCA_RXS_CLK_RXC0        0x00
#define SCA_RXS_CLK_RXC1        0x20
#define SCA_RXS_CLK_INT         0x40
#define SCA_RXS_CLK_ADPLL_OUT   0x60
#define SCA_RXS_CLK_ADPLL_IN    0x70

#define SCA_TXS_DIV_MASK        0x0F
#define SCA_TXS_DIV1            0x00
#define SCA_TXS_DIV2            0x01
#define SCA_TXS_DIV4            0x02
#define SCA_TXS_DIV8            0x03
#define SCA_TXS_DIV16           0x04
#define SCA_TXS_DIV32           0x05
#define SCA_TXS_DIV64           0x06
#define SCA_TXS_DIV128          0x07
#define SCA_TXS_DIV256          0x08
#define SCA_TXS_DIV512          0x09
#define SCA_TXS_CLK_TXC         0x00
#define SCA_TXS_CLK_INT         0x40
#define SCA_TXS_CLK_RX          0x60

#define SCA_ST0_RXRDY           0x01
#define SCA_ST0_TXRDY           0x02
#define SCA_ST0_RXINT           0x40
#define SCA_ST0_TXINT           0x80

#define SCA_ST1_IDLST           0x01
#define SCA_ST1_ABTST           0x02
#define SCA_ST1_DCDCHG          0x04
#define SCA_ST1_CTSCHG          0x08
#define SCA_ST1_FLAG            0x10
#define SCA_ST1_TXIDL           0x40
#define SCA_ST1_UDRN            0x80

/* ST2 and FST look the same */
#define SCA_FST_CRCERR          0x04
#define SCA_FST_OVRN            0x08
#define SCA_FST_RESFRM          0x10
#define SCA_FST_ABRT            0x20
#define SCA_FST_SHRT            0x40
#define SCA_FST_EOM             0x80

#define SCA_ST3_RXENA           0x01
#define SCA_ST3_TXENA           0x02
#define SCA_ST3_DCD             0x04
#define SCA_ST3_CTS             0x08
#define SCA_ST3_ADPLLSRCH       0x10
#define SCA_ST3_TXDATA          0x20

#define SCA_FIE_EOMFE           0x80

#define SCA_IE0_RXRDY           0x01
#define SCA_IE0_TXRDY           0x02
#define SCA_IE0_RXINT           0x40
#define SCA_IE0_TXINT           0x80

#define SCA_IE1_IDLDE           0x01
#define SCA_IE1_ABTDE           0x02
#define SCA_IE1_DCD             0x04
#define SCA_IE1_CTS             0x08
#define SCA_IE1_FLAG            0x10
#define SCA_IE1_IDL             0x40
#define SCA_IE1_UDRN            0x80

#define SCA_IE2_CRCERR          0x04
#define SCA_IE2_OVRN            0x08
#define SCA_IE2_RESFRM          0x10
#define SCA_IE2_ABRT            0x20
#define SCA_IE2_SHRT            0x40
#define SCA_IE2_EOM             0x80

/* This is for RRC, TRC0 and TRC1. */
#define SCA_RCR_MASK            0x1F

#define SCA_IE1_

#define SCA_IV_CHAN0            0x00
#define SCA_IV_CHAN1            0x20

#define SCA_IV_RXRDY            0x04
#define SCA_IV_TXRDY            0x06
#define SCA_IV_RXINT            0x08
#define SCA_IV_TXINT            0x0A

#define SCA_IV_DMACH0           0x00
#define SCA_IV_DMACH1           0x08
#define SCA_IV_DMACH2           0x20
#define SCA_IV_DMACH3           0x28

#define SCA_IV_DMIA             0x14
#define SCA_IV_DMIB             0x16

#define SCA_IV_TIMER0           0x1C
#define SCA_IV_TIMER1           0x1E
#define SCA_IV_TIMER2           0x3C
#define SCA_IV_TIMER3           0x3E

/*
 * DMA registers
 */
#define SCA_DSR_EOT             0x80
#define SCA_DSR_EOM             0x40
#define SCA_DSR_BOF             0x20
#define SCA_DSR_COF             0x10
#define SCA_DSR_DE              0x02
#define SCA_DSR_DWE             0x01

#define SCA_DMR_TMOD            0x10
#define SCA_DMR_NF              0x04
#define SCA_DMR_CNTE            0x02

#define SCA_DMER_EN             0x80

#define SCA_DCR_ABRT            0x01
#define SCA_DCR_FCCLR           0x02  /* Clear frame end intr counter */

#define SCA_DIR_EOT             0x80
#define SCA_DIR_EOM             0x40
#define SCA_DIR_BOF             0x20
#define SCA_DIR_COF             0x10

#define SCA_PCR_BRC             0x10
#define SCA_PCR_CCC             0x08
#define SCA_PCR_PR2             0x04
#define SCA_PCR_PR1             0x02
#define SCA_PCR_PR0             0x01

typedef struct sca_descriptor
  {
  unsigned short cp;
  unsigned short bp;
  unsigned char  bpb;
  unsigned char  unused0;
  unsigned short len;
  unsigned char  stat;
  unsigned char  unused1;
  }sca_descriptor;

#define SCA_DESC_EOT            0x01
#define SCA_DESC_CRC            0x04
#define SCA_DESC_OVRN           0x08
#define SCA_DESC_RESD           0x10
#define SCA_DESC_ABORT          0x20
#define SCA_DESC_SHRTFRM        0x40
#define SCA_DESC_EOM            0x80
#define SCA_DESC_ERRORS         0x7C

/*
***************************************************************************
**                                 END
***************************************************************************
**/
#endif /* _HD64570_H_ */

