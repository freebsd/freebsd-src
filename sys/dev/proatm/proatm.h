/*
 * Copyright (c) 2002, 2003 Christian Bucari, Prosum 
 * Copyright (c) 2002, 2003 Vincent Jardin, Xavier Heiny, 6WIND
 * Copyright (c) 2000, 2001 Richard Hodges and Matriplex, inc.
 * Copyright (c) 1996, 1997, 1998, 1999 Mark Tinguely
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *	must display the following acknowledgement:
 *	This product includes software developed by Prosum, 6wind and 
 *  Matriplex, inc
 * 4. The name of the authors may not be used to endorse or promote products 
 *	derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

/* 14-02-2003 */
/* $FreeBSD$ */

#ifndef _PROATM_H
#define _PROATM_H

#if (defined (_KERNEL) || defined(KERNEL))
#define MAXCARDS 10      /* set to impossibly high */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include "opt_atm.h"

#include <sys/sockio.h>

#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/signalvar.h>

#include <sys/mman.h>
#include <machine/clock.h>
#include <machine/cpu.h>    /* bootverbose */

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/if.h>
#include <net/if_arp.h>

#if MCLBYTES != 2048
#error "This proatm driver depends on 2048 byte mbuf clusters."
#endif 

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif 

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <pci.h>
#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include <machine/stdarg.h>

#include <netatm/kern_include.h>

/* HARP-specific definitions *************************************************/

#define PROATM_DEV_NAME	"proatm"

#define PROATM_IFF_MTU	9188 
#define PROATM_MAX_VCI	255		/* 0 - 255 */
#define PROATM_MAX_VPI	3

/*
 * Device VCC Entry
 *
 * Contains the common and PROATM-specific information for each VCC
 * which is opened through a PROATM device.
 */
struct nproatm_vcc {
    struct cmn_vcc  iv_cmn;          /* Common VCC stuff */
    }
;
typedef struct nproatm_vcc Idt_vcc;

#define iv_next		iv_cmn.cv_next
#define iv_toku		iv_cmn.cv_toku
#define iv_upper	iv_cmn.cv_upper
#define iv_vccb		iv_cmn.cv_connvc		/* HARP 3.0 */
#define iv_state	iv_cmn.cv_state
#define iu_pif		iu_cmn.cu_pif
#define iu_unit		iu_cmn.cu_unit
#define iu_flags	iu_cmn.cu_flags
#define iu_mtu		iu_cmn.cu_mtu
#define iu_open_vcc	iu_cmn.cu_open_vcc
#define iu_instvcc	iu_cmn.cu_instvcc		/* HARP 3.0 */
#define iu_vcc		iu_cmn.cu_vcc
#define iu_vcc_pool	iu_cmn.cu_vcc_pool
#define iu_nif_pool	iu_cmn.cu_nif_pool
#define iu_ioctl	iu_cmn.cu_ioctl
#define iu_openvcc	iu_cmn.cu_openvcc
#define iu_closevcc	iu_cmn.cu_closevcc
#define iu_output	iu_cmn.cu_output
#define iu_config	iu_cmn.cu_config


/* fixbuf memory map *******************************************************************
 *
 *
 *  0000 - 1fff:  TSQ  Transmit status queue 1024 entries *  8 bytes each
 *  2000 - 3fff:  RSQ  Receive status queue,  512 entries * 16 bytes each
 *
 */

#define PROATM_TSQ_OFFSET  0x0000
#define PROATM_RSQ_OFFSET	0x2000

#define PROATM_TSQ_SIZE  	0x2000
#define PROATM_RSQ_SIZE  	0x2000

#define IDT_SCQ_ENTRIES	64
#define IDT_SCQ_SIZE	(16*IDT_SCQ_ENTRIES)

/* some #define for driver structures in host memory ************************/

/* TSQ entry */
#define IDT_TSI_EMPTY          	0x80000000
#define IDT_TSI_TIMESTAMP_MASK 	0x00FFFFFF
#define IDT_TSI_TYPE_MASK      	0x60000000
#define IDT_TSI_TAG_MASK        (0x1F<<24)
#define IDT_TSI_TYPE_TMROF     	0x00000000
#define IDT_TSI_TYPE_TSR       	0x20000000
#define IDT_TSI_TYPE_IDLE     	0x40000000
#define IDT_TSI_TYPE_TBD       	0x60000000

/* RSQ entry */
#define IDT_RSQE_TYPE       	0xC0000000 
#define IDT_RSQE_VALID      	0x80000000   
#define IDT_RSQE_POOL       	0x00030000
#define IDT_RSQE_BUFASSIGN  	0x00008000  
#define IDT_RSQE_NZGFC      	0x00004000
#define IDT_RSQE_EOPDU      	0x00002000
#define IDT_RSQE_CBUF      	 	0x00001000
#define IDT_RSQE_CONGESTION 	0x00000800
#define IDT_RSQE_CLP        	0x00000400
#define IDT_RSQE_CRCERR     	0x00000200

/*TBD */
#define IDT_TSR                 0x80000000
#define IDT_TBD_EOPDU           0x40000000
#define IDT_TBD_AAL0            0x00000000
#define IDT_TBD_AAL34           0x04000000
#define IDT_TBD_AAL5            0x08000000
#define IDT_TBD_OAM             0x10000000
#define IDT_TBD_TSIF            0x20000000
#define IDT_TBD_GTSI            0x02000000
#define IDT_TBD_LOW_PRIORITY    0x00000001
#define PROATM_CLOSE_TAG        0x15
#define PROATM_PDU_TAG          0x0a

#define PROATM_CLOSE_CONNECTION    ((void*)0x55)

#define IDT_TBD_VPI_MASK 0x0FF00000
#define IDT_TBD_VCI_MASK 0x000FFFF0
#define IDT_TBD_VC_MASK (IDT_TBD_VPI_MASK | IDT_TBD_VCI_MASK)

/* PROATM SRAM constants ******************************************/
/* TCT size set by CFG reg(b16-17), 8-words entries */ 
/* RCT size set by CFG reg(b16-17) 4-words entries */  
/* RFBQ0 and RFBQ1 */  
#define PROATM_RFBQ_SIZE        0x400  

/* DTST size set by ABRSTD (2x2Kword-tables) (and b0-11 null) */
#define PROATM_ABRSTD_SIZE 		(2<<24)
#define PROATM_DTST_SIZE  		4096
  
/* TST */
#define PROATM_TST_MAX			4096
#define PROATM_TST_RESERVED  	340		/* N. entries reserved for UBR/ABR/VBR */

/* SDCs (12 words, ==>must be aligned on 4-word boundary)*/
#define IDT_STRUCT_SCD_SIZE 	12		/* 12 words */

/* RXFIFO size set by RXFD (4K words) (and b0-13 null) */
#define PROATM_RXFIFO_SIZE		0x1000
#define PROATM_RXFD_SIZE	  	(3<<24)


/* You may change these **********************************************************/

#define PROATM_LBUFS       200  	/* default number of 2k buffers. Must be comprised */
                                    /* between 70 and 510 */
#define PROATM_SBUFS       200  	/* default number of 96-byte buffers. Must be */
                                    /* comprised between 70 and 510 */
#define PROATM_RSQSIZE 	   8192		/* 2048, 4096 or 8192 */
#define PROATM_VPIBITS 	   2		/* 0, 1, 2, or 8 */
#define RCQ_SUPPORT					/* Enable Raw Cell support */

/* 
 	VPI_BASE sets the bottom of the VP range. (normaly 0)
 			 It is a multiple of the number of VP's the card is able to manage
			 For example if PROATM_VPIBITS = 2, VPI_BASE may be 0, 4, 8, 12, etc.
			 Set it to 0 if you don't know what to do.

 	VCI_BASE sets the bottom of the VC range. (normaly 0)
  			 It is a multiple of the number of VC's the card is able to manage.
			 For example if the card has 128 KB memory and PROATM_VPIBITS = 2
			 then the number of VCI bits is 8 and the number of VCs is 256,
			 thus VPI_BASE may be 0, 256, 512, etc.  
			 Set it to 0 if you don't know what to do.

	The VPM register value is dynamicaly computed by using VCI_BASE, VPI_BASE, 
	PROATM_VPIBITS and the memory size
*/

#define VCI_BASE		 	0
#define VPI_BASE			0

/* define for use of clock recovered from receive signal as transmission clock
   the  local oscillator is used by default
*/
#undef PROATM_LOOPT

/* define for full SDH mode
   follows the SONET specifications by default 
*/
#undef PROATM_FULL_SDH


/* IDT77252 Registers *************************************************************/

#define REGDR0    0x00   /* Data Register 0 R/W*/
#define REGDR1    0x04   /* Data Register 1 W */
#define REGDR2    0x08   /* Data Register 2 W */
#define REGDR3    0x0C   /* Data Register 3 W */
#define REGCMD    0x10    /* command          w */
#define REGCFG    0x14    /* configuration  r/w */
#define REGSTAT   0x18    /* status         r/w */
#define REGRSQB   0x1c    /* RSQ base         w */
#define REGRSQT   0x20    /* RSQ tail         r */
#define REGRSQH   0x24    /* RSQ head         w */
#define REGCDC    0x28    /* cell drop cnt  r/c */
#define REGVPEC   0x2c    /* vci/vpi er cnt r/c */ 
#define REGICC    0x30    /* invalid cell   r/c */
#define REGRAWT   0x34    /* raw cell tail    r */
#define REGTMR    0x38    /* timer            r */
#define REGTSTB   0x3c    /* TST base       r/w */
#define REGTSQB   0x40    /* TSQ base         w */
#define REGTSQT   0x44    /* TSQ tail         r */
#define REGTSQH   0x48    /* TSQ head         w */
#define REGGP     0x4c    /* general purp   r/w */
#define REGVMSK   0x50    /* vci/vpi mask     w */
#define REGRXFD	  0x54    /* Receive FIFO descriptor */ 
#define REGRXFT   0x58    /* Receive FIFO Tail */ 
#define REGRXFH   0x5C    /* Receive FIFO  Head */
#define REGRAWHND 0x60    /* Raw Cell Handle register */ 
#define REGRXSTAT 0x64    /* Receive Conn. State register */ 
#define REGABRSTD 0x68    /* ABR Schedule TableDescriptor */
#define REGABRRQ  0x6C    /* ABR Ready Queue Pointer register */
#define REGVBRRQ  0x70    /* VBR Ready Queue Pointer register */
#define REGRTBL   0x74    /* Rate Table Descriptor */
#define REGMXDFCT 0x78    /* Maximum Deficit Count register */
#define REGTXSTAT 0x7C    /* Transmit Conn. State register */
#define REGTCMDQ  0x80    /* Transmit Command Queue register */
#define REGIRCP   0x84    /* Inactive Receive Conn. Pointer */
#define REGFBQP0  0x88    /* Free Buffer Queue Pointer Register 0 */
#define REGFBQP1  0x8C    /* Free Buffer Queue Pointer Register 1 */
#define REGFBQP2  0x90    /* Free Buffer Queue Pointer Register 2 */
#define REGFBQP3  0x94    /* Free Buffer Queue Pointer Register 3 */
#define REGFBQS0  0x98    /* Free Buffer Queue Size Register 0 */
#define REGFBQS1  0x9C    /* Free Buffer Queue Size Register 1 */
#define REGFBQS2  0xA0    /* Free Buffer Queue Size Register 2 */
#define REGFBQS3  0xA4    /* Free Buffer Queue Size Register 3 */
#define REGFBQWP0 0xA8    /* Free Buffer Queue Write Pointer Register 0 */
#define REGFBQWP1 0xAC    /* Free Buffer Queue Write Pointer Register 1 */
#define REGFBQWP2 0xB0    /* Free Buffer Queue Write Pointer Register 2 */
#define REGFBQWP3 0xB4    /* Free Buffer Queue Write Pointer Register 3 */
#define REGNOW    0xB8    /* Current Transmit Schedule Table Address */

/* Commands issued to the COMMAND register **************************************/

#define IDT_CMD_NO_OPERATION      (0<<28)	 
#define IDT_CMD_OPEN_CONNECTION   ((2<<28) | (1<<19))
#define IDT_CMD_CLOSE_CONNECTION  (2<<28)
#define IDT_CMD_WRITE_SRAM        (4<<28)
#define IDT_CMD_READ_SRAM         (5<<28)
#define IDT_CMD_WRITE_FREEBUFQ    (6<<28)
#define IDT_CMD_READ_UTILITY_CS0  ((8<<28) | (1<<8))
#define IDT_CMD_WRITE_UTILITY_CS0 ((9<<28) | (1<<8))

/* Commands issued to TCMDQ register ********************************************/

#define IDT_TCMDQ_START         (1<<24)
#define IDT_TCMDQ_ULACR         (2<<24)
#define IDT_TCMDQ_START_ULACR   (3<<24)
#define IDT_TCMDQ_UPD_INITER    (4<<24)
#define IDT_TCMDQ_HALT          (5<<24)

/* STATUS Register bits ******************************************************/

#define IDT_STAT_FRAC3_MASK 0xF0000000
#define IDT_STAT_FRAC2_MASK 0x0F000000
#define IDT_STAT_FRAC1_MASK 0x00F00000
#define IDT_STAT_FRAC0_MASK 0x000F0000
#define IDT_STAT_TSIF       0x00008000   /* Transmit Status Queue Indicator */
#define IDT_STAT_TXICP      0x00004000   /* Transmit Incomplete PDU */
#define IDT_STAT_TSQF       0x00001000   /* Transmit Status Queue Full */
#define IDT_STAT_TMROF      0x00000800   /* Timer Overflow */
#define IDT_STAT_PHYI       0x00000400   /* PHY Device Interrupt */
#define IDT_STAT_CMDBZ      0x00000200   /* Command Busy */
#define IDT_STAT_FBQ3A      0x00000100   /* Free Buffer Queue Attention */
#define IDT_STAT_FBQ2A      0x00000080   /* Free Buffer Queue Attention */
#define IDT_STAT_RSQF       0x00000040   /* Receive Status Queue Full */
#define IDT_STAT_EOPDU      0x00000020   /* End of PDU */
#define IDT_STAT_RAWCF      0x00000010   /* Raw Cell Flag */
#define IDT_STAT_FBQ1A      0x00000008   /* Free Buffer Queue Attention */
#define IDT_STAT_FBQ0A      0x00000004   /* Free Buffer Queue Attention */
#define IDT_STAT_RSQAF      0x00000002   /* Receive Status Queue Almost Full */

/* Interrupt flags we must take into account */
#define INT_FLAGS ( IDT_STAT_TSQF  |   \
                    IDT_STAT_RSQAF |   \
                    IDT_STAT_RAWCF |   \
                    IDT_STAT_EOPDU |   \
                    IDT_STAT_TMROF |   \
                    IDT_STAT_TXICP |   \
                    IDT_STAT_TSIF  |   \
                    IDT_STAT_FBQ0A |   \
                    IDT_STAT_FBQ1A |   \
                    IDT_STAT_PHYI )

/* Interrupt flags that are cleared by writing 1 */
#define  CLEAR_FLAGS ( IDT_STAT_TSIF | IDT_STAT_TXICP | IDT_STAT_TMROF \
                        | IDT_STAT_PHYI | IDT_STAT_EOPDU | IDT_STAT_RAWCF) 

/* CONFIGURATION Register Bits *************************************************/

#define IDT_CFG_SWRST          0x80000000    /* Software Reset */
#define IDT_CFG_LOOP           0x40000000    /* Enable internal loop back */
#define IDT_CFG_RXPATH         0x20000000    /* Receive Path Enable */
#define IDT_CFG_IDLECLP        0x10000000    /* CLP bit of Null Cells */
#define IDT_CFG_TFIFOSIZE_MASK 0x0C000000    /* Specifies size of tx FIFO */
#define IDT_CFG_RSQSIZE_MASK   0x00C00000    /* Receive Status Queue Size */
#define IDT_CFG_ICACCEPT       0x00200000    /* Invalid Cell Accept */
#define IDT_CFG_IGNOREGFC      0x00100000    /* Ignore General Flow Control */
#define IDT_CFG_VPIBITS_MASK   0x000C0000    /* VPI/VCI Bits Size Select */
#define IDT_CFG_RCTSIZE_MASK   0x00030000    /* Receive Connection Table Size */
#define IDT_CFG_VCERRACCEPT    0x00008000    /* VPI/VCI Error Cell Accept */
#define IDT_CFG_RXINT_MASK     0x00007000    /* End of Receive PDU Interrupt
                                               Handling */
#define IDT_CFG_RAWIE          0x00000800    /* Raw Cell Interrupt Enable */
#define IDT_CFG_RSQAFIE        0x00000400    /* Receive Queue Almost Full
                                               Interrupt Enable */
#define IDT_CFG_CACHE          0x00000100    /* Cache */
#define IDT_CFG_TMRROIE        0x00000080    /* Timer Roll Over Interrupt
                                               Enable */
#define IDT_CFG_FBIE           0x00000040    /* Free Buffer Queue Int Enable */

#define IDT_CFG_TXEN           0x00000020    /* Transmit Operation Enable */
#define IDT_CFG_TXIE           0x00000010    /* Transmit Status Interrupt
                                               Enable */
#define IDT_CFG_TXURIE         0x00000008    /* Transmit Under-run Interrupt
                                               Enable */
#define IDT_CFG_UMODE          0x00000004    /* Utopia Mode (cell/byte) Select */
#define IDT_CFG_TSQFIE         0x00000002    /* Transmit Status Queue Full
                                               Interrupt Enable */
#define IDT_CFG_PHYIE          0x00000001    /* PHY Interrupt Enable */

#define IDT_CFG_RSQSIZE_2048 0x00000000
#define IDT_CFG_RSQSIZE_4096 0x00400000
#define IDT_CFG_RSQSIZE_8192 0x00800000

#define IDT_CFG_VPIBITS_0 0x00000000
#define IDT_CFG_VPIBITS_1 0x00040000
#define IDT_CFG_VPIBITS_2 0x00080000
#define IDT_CFG_VPIBITS_8 0x000C0000

#define IDT_CFG_RCTSIZE_1024_ENTRIES  0x00000000
#define IDT_CFG_RCTSIZE_4096_ENTRIES  0x00010000
#define IDT_CFG_RCTSIZE_16384_ENTRIES 0x00020000
#define IDT_CFG_RCTSIZE_512_ENTRIES   0x00030000

#define IDT_CFG_RXINT_NOINT   0x00000000
#define IDT_CFG_RXINT_NODELAY 0x00001000
#define IDT_CFG_RXINT_2800CLK 0x00002000
#define IDT_CFG_RXINT_4F00CLK 0x00003000
#define IDT_CFG_RXINT_7400CLK 0x00004000

/* GP Register Bits *************************************************/
#define IDT_GP_EECLK	(1<<2)
#define IDT_GP_EECS     (1<<1)
#define IDT_GP_EEDO      1
#define IDT_GP_EEDI     (1<<16)


/* NICStAR structures located in local SRAM ***********************************/

/* TCT - Transmit Connection Table
 *
 * Written by both the NICStAR and the device driver.
 */
    /* number of 32-bit word*/
#define IDT_TCT_ENTRY_SIZE      8
   /* word 1 of TCTE */
#define IDT_TCTE_TYPE_MASK      0xC0000000
#define IDT_TCTE_TYPE_CBR       0x00000000
#define IDT_TCTE_TYPE_ABR       0x80000000
#define IDT_TCTE_TYPE_VBR       0x40000000
#define IDT_TCTE_TYPE_UBR       0x00000000
   /* word 2 of TCTE */
#define IDT_TCTE_TSIF           (1<<14)
   /* word 4 of TCTE */
#define IDT_TCTE_HALT           0x80000000  
#define IDT_TCTE_IDLE           0x40000000  
   /* word 8 of UBR or CBR TCTE */
#define IDT_TCTE_UBR_EN	        0x80000000  
  

/* RCT - Receive Connection Table
 *
 * Written by both the NICStAR and the device driver.
 */

typedef struct idt_rcte
{
   u_int32_t word_1;
   u_int32_t buffer_handle;
   u_int32_t dma_address;
   u_int32_t aal5_crc32;
} idt_rcte;


#define IDT_RCTE_INACTLIM        0xE0000000
#define IDT_RCTE_INACTCOUNT      0x1C000000
#define IDT_RCTE_CIVC            0x00800000
#define IDT_RCTE_FBP             0x00600000
#define IDT_RCTE_NZGFC           0x00100000
#define IDT_RCTE_CONNECTOPEN     0x00080000
#define IDT_RCTE_AALMASK         0x00070000
#define IDT_RCTE_AAL0            0x00000000
#define IDT_RCTE_AAL34           0x00010000
#define IDT_RCTE_AAL5            0x00020000
#define IDT_RCTE_RCQ             0x00030000
#define IDT_RCTE_OAM             0x00040000
#define IDT_RCTE_RAWCELLINTEN    0x00008000
#define IDT_RCTE_RXCONSTCELLADDR 0x00004000
#define IDT_RCTE_BUFSTAT         0x00003000
#define IDT_RCTE_EFCI            0x00000800
#define IDT_RCTE_CLP             0x00000400
#define IDT_RCTE_CRCERROR        0x00000200
#define IDT_RCTE_CELLCOUNT_MASK  0x000001FF

#define IDT_RCT_ENTRY_SIZE 4	/* Number of 32-bit words */

/* TST - Transmit Schedule Table */
#define IDT_TST_OPCODE_MASK 0x60000000

#define IDT_TST_OPCODE_NULL     0x00000000 /* Insert null cell */
#define IDT_TST_OPCODE_FIXED    0x20000000 /* Cell from a fixed rate channel */
#define IDT_TST_OPCODE_VARIABLE 0x40000000 /* Cell from variable rate channel */
#define IDT_TST_OPCODE_END      0x60000000 /* Jump */


/* New #define ******************************************************************/

#define PROATM_MAX_QUEUE   4096 
     
#if (PROATM_RSQSIZE == 2048)
#define PROATM_CFG_RSQSIZE IDT_CFG_RSQSIZE_2048
#elif (PROATM_RSQSIZE == 4096)
#define PROATM_CFG_RSQSIZE IDT_CFG_RSQSIZE_4096
#elif (PROATM_RSQSIZE == 8192)
#define PROATM_CFG_RSQSIZE IDT_CFG_RSQSIZE_8192
#else
#error PROATM_RSQSIZE must be 2048, 4096 or 8192
#endif /* PROATM_RSQSIZE */

#define PROATM_LRG_SIZE 2048    /* must be power of two */

#define PROATM_FIXPAGES  ((PROATM_TSQ_SIZE + PROATM_RSQ_SIZE)/PAGE_SIZE)

#if PROATM_VPIBITS == 0
#define PROATM_CFG_VPIBITS IDT_CFG_VPIBITS_0
#elif PROATM_VPIBITS == 1
#define PROATM_CFG_VPIBITS IDT_CFG_VPIBITS_1
#elif PROATM_VPIBITS == 2
#define PROATM_CFG_VPIBITS IDT_CFG_VPIBITS_2
#elif PROATM_VPIBITS == 8
#define PROATM_CFG_VPIBITS IDT_CFG_VPIBITS_8
#else
#error PROATM_VPIBITS must be 0, 1, 2 or 8
#endif /* PROATM_VPIBITS */


/* free buf. queues *************************************************************/

/* registers #34-#37 */
#define IDT_FBQPR_MASK      0x000007FF
#define IDT_FBQPW_MASK      0x07FF0000

#define diff(fbqp)  (int32_t) ((int32_t)((((fbqp) & IDT_FBQPW_MASK)>>16) - \
                                       ((fbqp) & IDT_FBQPR_MASK))/2)
#define idt_fbqc_get(fbqp)  (diff(fbqp) >= 0 ? diff(fbqp) : 0x400 + diff(fbqp))

/* FBQS registers */
/* unit = 512/16 = 32 */
#define IDT_B0THLD    (2<<28)
#define IDT_B1THLD    (2<<28)
#define IDT_B0SIZE    2 /*(MHLEN/48)*/     /* /sys/sys/mbuf.h */
#define IDT_B1SIZE    (MCLBYTES/48) /* /sys/i386/include/param.h */

/* Configuration Register */
#ifdef  RCQ_SUPPORT
#define IDT_DEFAULT_CFG  ( \
            IDT_CFG_RXPATH | \
			IDT_CFG_FBIE | \
			PROATM_CFG_RSQSIZE | \
			PROATM_CFG_VPIBITS | \
			IDT_CFG_RXINT_NODELAY | \
			IDT_CFG_ICACCEPT | \
			IDT_CFG_RAWIE | \
			IDT_CFG_IGNOREGFC | \
			IDT_CFG_VCERRACCEPT | \
			IDT_CFG_RSQAFIE | \
			IDT_CFG_TMRROIE  | \
			IDT_CFG_TXEN | \
			IDT_CFG_TXIE | \
			IDT_CFG_TSQFIE | \
			IDT_CFG_PHYIE )
#else
#define IDT_DEFAULT_CFG  ( \
            IDT_CFG_RXPATH | \
			IDT_CFG_FBIE | \
			PROATM_CFG_RSQSIZE | \
			PROATM_CFG_VPIBITS | \
			IDT_CFG_RXINT_NODELAY | \
			IDT_CFG_RSQAFIE | \
			IDT_CFG_TMRROIE  | \
			IDT_CFG_TXEN | \
			IDT_CFG_TXIE | \
			IDT_CFG_TSQFIE | \
			IDT_CFG_PHYIE )
#endif

/* ESI stuff ******************************************************************/

#define EPROM_PROSUM_MAC_ADDR_OFFSET 36
#define EPROM_IDT_MAC_ADDR_OFFSET 0x6C 
#define PROSUM_MAC_0 0x00
#define PROSUM_MAC_1 0xC0
#define PROSUM_MAC_2 0xFD

/****************************************************************************/

#if XDEBUG
#define LOGVCS 1
#else
#define LOGVCS 0
#endif

static int32_t proatm_sysctl_logbufs = 0;              /* periodic buffer status messages */
static int32_t proatm_sysctl_logvcs = LOGVCS;          /* log VC open & close events */
static int32_t proatm_sysctl_buflarge = PROATM_LBUFS;  /* desired large buffer queue */
                                                   /* between 64 and 510 */
static int32_t proatm_sysctl_bufsmall = PROATM_SBUFS;  /* desired small buffer queue */
                                                   /* between 64 and 510 */

SYSCTL_NODE(_hw, OID_AUTO, proatm, CTLFLAG_RW, 0, "PROATM");
SYSCTL_INT(_hw_proatm, OID_AUTO, log_bufstat, CTLFLAG_RW, 
           & proatm_sysctl_logbufs, 0, "Log buffer status");
SYSCTL_INT(_hw_proatm, OID_AUTO, log_vcs, CTLFLAG_RW, 
           & proatm_sysctl_logvcs, 0, "Log VC open/close");
SYSCTL_INT(_hw_proatm, OID_AUTO, bufs_large, CTLFLAG_RW, 
           & proatm_sysctl_buflarge, PROATM_LBUFS, "Large buffer queue");
SYSCTL_INT(_hw_proatm, OID_AUTO, bufs_small, CTLFLAG_RW, 
           & proatm_sysctl_bufsmall, PROATM_SBUFS, "Small buffer queue");

/* common VCI values **************************************************************/
/*
0/0  Idle cells
0/1  Meta signalling
x/1  Meta signalling
0/2  Broadcast signalling
x/2  Broadcast signalling
x/3  Segment OAM F4 flow
x/4  End-end OAM F4 flow
0/5  p-p signalling
x/5  p-p signalling
x/6  rate management
0/14 SPANS
0/15 SPANS
0/16 ILMI
0/18 PNNI
*/

/* AAL types */
#define IDTAAL0      0
#define IDTAAL1      1
#define IDTAAL3_4    3
#define IDTAAL5      5

#define NICCBR       1
#define NICVBR       2
#define NICABR       3
#define NICUBR       4
#define NICUBR0		 5


/* RCQ - Raw Cell Queue
 *
 * Written by the NICStAR, read by the device driver.
 */

typedef struct cell_payload
{
   u_int32_t word[12];
} cell_payload;

typedef struct rcqe
{
   u_int32_t word_1;
   u_int32_t word_2;
   u_int32_t word_3;
   u_int32_t word_4;
   cell_payload payload;
} rcqe;

#define IDT_RCQE_SIZE 64		/* bytes */

/* TXQUEUE structure *************************************************************/

typedef struct {
    struct mbuf     *mget;        /* head of mbuf queue, pull mbufs from here */
    struct mbuf     *mput;        /* tail of mbuf queue, put mbufs here */
    u_int32_t          scd;          /* segmentation channel descriptor address */
    u_int32_t          *scq_base;    /* segmentation channel queue base address */
    u_int32_t          *scq_next;    /* next address */
    u_int32_t          *scq_last;    /* last address written */
    int32_t             scq_cur;      /* current number entries in SCQ buffer known by the SAR */
    int32_t             rate;         /* cells per second allocated to this queue */
}  TX_QUEUE;

/*  To avoid expensive SRAM reads, scq_cur tracks the number of SCQ entries
 *  in use.  Only proatm_transmit_top may increase this, and only proatm_intr_tsq
 *  may decrease it.
 *
 *  mbuf chains on the queue use the fields:
 *  m_next     is the usual pointer to next mbuf
 *  m_nextpkt  is the next packet on the queue
 *  m_pkthdr.rcvif is a pointer to the connection
 *  m_pkthdr.header is a pointer to the TX queue
 *
 *  The TX_QUEUEs has to be R/W only at splimp().
 */

/* CONNECTION structure *****************************************************************/

typedef struct {
    struct vccb     *vccinf;
    u_int8_t          vpi;
    u_int16_t         vci;
	int32_t				number;				 /* connection number */
    TX_QUEUE        *queue;              /* transmit queue for this connection */
    struct mbuf     *recv;               /* current receive mbuf, or NULL */
    int32_t             rlen;                /* current receive length */
    int32_t             maxpdu;              /* largest PDU we will ever see */
    int32_t             traf_pcr;
    int32_t             traf_scr;
    int32_t             traf_mbs;
    u_int8_t          aal;                 /* AAL for this connection */
    u_int8_t          class;               /* NICCBR, NICVBR, NICUBR or NICUBR0 */
    u_int8_t          flg_clp:1;           /* CLP flag for outbound cells */
	u_int8_t			flg_active:1;        /* connection is active */
    u_int8_t          flg_closing:1;       /* close pending */
    u_int8_t          flg_open:1;          /* O if closed */
    u_int8_t          lacr;
    u_int8_t          init_er;

} CONNECTION;

#define GET_RDTSC(var) {__asm__ volatile("rdtsc":"=A"(var)); }

#endif /* KERNEL */

/* SUNI management and statistics *******************************************************/

struct proatm_stats_oc3 {
	u_int32_t		oc3_sect_bip8;	/* Section 8-bit intrlv parity errors */
	u_int32_t		oc3_path_bip8;	/* Path 8-bit intrlv parity errors */
	u_int32_t		oc3_line_bip24;	/* Line 24-bit intrlv parity errors */
	u_int32_t		oc3_line_febe;	/* Line far-end block errors */
	u_int32_t		oc3_path_febe;	/* Path far-end block errors */
	u_int32_t		oc3_hec_corr;	/* Correctable HEC errors */
	u_int32_t		oc3_hec_uncorr;	/* Uncorrectable HEC errors */
	u_int32_t		oc3_rx_cells;	/* Receive cells */
	u_int32_t		oc3_tx_cells;	/* Transmit cells */
};
typedef struct proatm_stats_oc3 Proatm_Stats_oc3;

#if (defined (_KERNEL) || defined(KERNEL))
#define    READ_ONE(y, x) proatm_util_rd(proatm, (x), &t); y = t  & 0xff;

#define    READ_TWO(y, x) proatm_util_rd(proatm, (x+1), &t); y = (t & 0xff) << 8; \
             proatm_util_rd(proatm, (x), &t); y |= t & 0xff; 

#define    READ_THREE(y, x)proatm_util_rd(proatm,(x+2), &t); y = (t & 0xff) << 16; \
              proatm_util_rd(proatm,(x+1), &t); y = (t & 0xff) << 8; \
              proatm_util_rd(proatm,(x), &t); y |= t & 0xff; 

/* Some SUNI Register numbers */
#define    SUNI_MASTER_REG     0x00        /* Master reset and ID */
#define    SUNI_IS_REG         0x02        /* Master Interrupt Status */
#define    SUNI_MSTR_CTRL_REG  0x05        /* Master Control */
#define    SUNI_CLOCK_REG      0x06        /* Clock synth/control/status */
#define    SUNI_RSOP_REG       0x10        /* RSOP control/Interrupt Status */
#define    SUNI_RSOP_SIS_REG   0x11        /* RSOP status/interrupt status */
#define    SUNI_SECT_BIP_REG   0x12        /* RSOP section BIP 8 LSB */
#define    SUNI_RLOP_REG       0x18        /* RLOP control/Interrupt Status */
#define    SUNI_LINE_BIP_REG   0x1A        /* RLOP line BIP 8/24 LSB */
#define    SUNI_LINE_FEBE_REG  0x1D        /* RLOP line FEBE LSB */
#define    SUNI_RPOP_IS_REG    0x31        /* RPOP Interrupt Status */
#define    SUNI_PATH_BIP_REG   0x38        /* RPOP path BIP 8 LSB */
#define    SUNI_PATH_FEBE_REG  0x3A        /* RPOP path FEBE LSB */
#define	   SUNI_TPOP_MSB_REG   0x46        /* TPOP arbitrary pointer MSB */
#define    SUNI_RACP_REG       0x50        /* RACP control/status */
#define    SUNI_HECS_REG       0x54        /* RACP correctable HCS error count */
#define    SUNI_UHECS_REG      0x55        /* RACP uncorrectable HCS err count */
#define    SUNI_RACP_RX_REG    0x56        /* RACP receive cell counter */
#define    SUNI_TACP_REG       0x60        /* TACP control/status */
#define    SUNI_TACP_TX_REG    0x64        /* TACP transmit cell counter */

#endif /* KERNEL */

/* IDT77105 management and statistics ***************************************************/

struct proatm_stats_utp25 {
	u_int32_t utp25_symbol_errors;            /* wire symbol errors */
	u_int32_t utp25_tx_cells;                 /* cells transmitted */
	u_int32_t utp25_rx_cells;                 /* cells received */
	u_int32_t utp25_rx_hec_errors;            /* Header Error Check errors on receive */
};
typedef struct proatm_stats_utp25 Proatm_Stats_utp25;

#if (defined (_KERNEL) || defined(KERNEL))

/* The IDT77105 registers */
#define IDT77105_MCR_REG       0x00        /* Master Control Register */
#define IDT77105_ISTAT_REG     0x01        /* Interrupt Status */
#define IDT77105_DIAG_REG      0x02        /* Diagnostic Control */
#define IDT77105_LEDHEC_REG    0x03        /* LED Driver & HEC Status/Control */
#define IDT77105_CTRLO_REG     0x04        /* Low Byte Counter Register */
#define IDT77105_CTRHI_REG     0x05        /* High Byte Counter Register */
#define IDT77105_CTRSEL_REG    0x06        /* Counter Register Read Select */

#endif /* KERNEL */

/* Proam Soft Drivers Statistics *********************************************************/

struct proatm_stats_atm {
	u_int32_t		atm_xmit;	/* Cells transmitted */
	u_int32_t		atm_rcvd;	/* Cells received */
	u_int32_t		atm_pad[2];	/* Pad to quad-word boundary */
};
typedef	struct proatm_stats_atm	Proatm_Stats_atm;

struct proatm_stats_aal0 {
	u_int32_t		aal0_xmit;	/* Cells transmitted */
	u_int32_t		aal0_rcvd;	/* Cells received */
	u_int32_t		aal0_drops;	/* Cells dropped */
	u_int32_t		aal0_pad;	/* Pad to quad-word boundary */
};
typedef	struct proatm_stats_aal0	Proatm_Stats_aal0;

struct proatm_stats_aal4 {
	u_int32_t		aal4_xmit;	/* Cells transmitted */
	u_int32_t		aal4_rcvd;	/* Cells received */
	u_int32_t		aal4_crc;	/* Cells with payload CRC errors */
	u_int32_t		aal4_sar_cs;	/* Unknown proto */
	u_int32_t		aal4_drops;	/* Cell drops */
	u_int32_t		aal4_pdu_xmit;	/* CS PDUs transmitted */
	u_int32_t		aal4_pdu_rcvd;	/* CS PDUs received */
	u_int32_t		aal4_pdu_errs;	/* CS PDUs with PDU errors */
	u_int32_t		aal4_pdu_drops;	/* CS PDUs dropped */
};
typedef struct proatm_stats_aal4	Proatm_Stats_aal4;

struct proatm_stats_aal5 {
	u_int32_t		aal5_xmit;	/* Cells transmitted */
	u_int32_t		aal5_rcvd;	/* Cells received */
	u_int32_t		aal5_crc_len;	/* Cells with CRC/length errors */
	u_int32_t		aal5_drops;	/* Cell drops */
	u_int32_t		aal5_pdu_xmit;	/* CS PDUs transmitted */
	u_int32_t		aal5_pdu_rcvd;	/* CS PDUs received */
	u_int32_t		aal5_pdu_crc;	/* CS PDUs with CRC errors */
	u_int32_t		aal5_pdu_drops;	/* CS PDUs dropped */
};
typedef	struct proatm_stats_aal5	Proatm_Stats_aal5;

struct proatm_stats_driver {
	/*
	 * VCM sats [OK]
	 */
	u_int32_t		drv_vc_maxpdu;	/* Requested PDU size too large */
	u_int32_t		drv_vc_badrng;	/* VPI and/or VCI too large */
    u_int32_t		drv_vc_outofbw;	/* Not enough bandwidth for requested CBR VC */

	/*
	 * Receive stats [OK]
	 */
	u_int32_t		drv_rv_nocx;	/* No SAR CONNECTION */
	u_int32_t		drv_rv_nopkthdr;	/* No M_PKTHDR within first mbuf */
	u_int32_t		drv_rv_invchain;	/* Invalid mbuf chain */
	u_int32_t		drv_rv_toobigpdu;	/* Received len > cx's maxpdu */
	u_int32_t		drv_rv_novcc;	/* No HARP vpi/vci connection */
	u_int32_t		drv_rv_nobufs;	/* Not enough buffer space */
	u_int32_t		drv_rv_null;	/* Trying to pass null PDU up stack */
	u_int32_t		drv_rv_intrq;	/* No room in atm_intrq */
	/*
	 * RAW receive stats [OK]
	 */
	u_int32_t		drv_rv_rnotrdy;	/* RAW cell received, buffers not ready */
	u_int32_t		drv_rv_rnobufs;	/* Not enough buffer space for raw cells */

	/*
	 * Transmit stats [OK]
	 */
    u_int32_t		drv_xm_txicp;	/* Transmit Error Incomplete PDU */
    u_int32_t		drv_xm_ntbd;	/* Number of TBDs transmited to the SAR */
    u_int32_t		drv_xm_idlevbr;	/* Number of idle VBR VCs */
    u_int32_t		drv_xm_closing;	/* Number close VC requests in progress */
	u_int32_t		drv_xm_qufree;	/* Number of free txqueue = txqueue_free_count */
	u_int32_t		drv_xm_cbrbw;	/* CBR bandwidth reserved on the board */
	u_int32_t		drv_xm_ubr0free;	/* Number of free slots of the UBR0 queue */
};
typedef struct proatm_stats_driver Proatm_Stats_drv;

struct	proatm_stats {
	Proatm_Stats_oc3	proatm_st_oc3;		/* OC3 layer stats */
	Proatm_Stats_utp25	proatm_st_utp25;	/* UTP25 layer stats */
	Proatm_Stats_atm	proatm_st_atm;		/* ATM layer stats */
	Proatm_Stats_aal0	proatm_st_aal0;		/* AAL0 layer stats */
	Proatm_Stats_aal4	proatm_st_aal4;		/* AAL3/4 layer stats */
	Proatm_Stats_aal5	proatm_st_aal5;		/* AAL5 layer stats */
	Proatm_Stats_drv	proatm_st_drv;		/* Driver stats */
};
typedef	struct proatm_stats	Proatm_stats;

#if (defined (_KERNEL) || defined(KERNEL))

#define BYTES_PER_CELL 48

/* Device softc structure ****************************************************************/

typedef struct {
    Cmn_unit        iu_cmn;                /* Common unit stuff */
    int32_t         unit;                  /* unit number of this device */
    vm_offset_t     virt_baseaddr;         /* nicstar register virtual address */
    vm_offset_t     fixbuf;                /* buffer that holds TSQ, RSQ, variable SCQ */
    int32_t         sram_size;		       /* In words, 0x4000 or 0x10000 */
	u_int32_t 		rct;				   /* SRAM address of the RCT */
    int32_t         rct_size;		       /* Number of RCT entries */
	u_int32_t		rate;				   /* SRAM address of the Rate tables */
	u_int32_t		tst;				   /* SRAM address of the TST */
	u_int32_t		tst_num;			   /* Number of usable TST entries */
	u_int32_t		dtst;				   /* SRAM address of the DTST */
	u_int32_t		scd;				   /* SRAM address of the SCDs */
	int32_t			scd_size;			   /* number of SCD entries */
	int32_t			scd_ubr0;			   /* SCD reserved for unspecified-pcr connections */
	u_int32_t		rxfifo;				   /* SRAM address of the Receive FIFO */
    int32_t         vpibits;               /* Number of VPI bits */
    int32_t         vcibits;               /* Number of VCI bits */
	u_int32_t		vpm;				   /* mask register value */
	int32_t			max_connection;		   /* Max number of connections */
    int32_t         conn_maxvpi;           /* number of VPI values */
    int32_t         conn_maxvci;           /* number of VCI values */
    u_int32_t       timer_wrap;            /* keep track of wrapped timers */
    u_int32_t       rsqh;                  /* Receive Status Queue, reg is write-only */
    struct arpcom   proatm_ac;             /* ifnet for device */
    CONNECTION      *connection;           /* table of connections, indexed by vpi*vci */
    u_int32_t   	max_pcr;
    int32_t         cellrate_tcur;         /* current CBR TX cellrate */
    int32_t         txslots_cur;           /* current number of TST slots in use */
    TX_QUEUE        txqueue[PROATM_MAX_QUEUE];
    TX_QUEUE        txqueue_ubr0;		   /* tx queue for unspecified-pcr connections */ 
    TX_QUEUE        *txqueue_free[PROATM_MAX_QUEUE];
    int32_t         txqueue_free_count;
    TX_QUEUE        *tst_slot[PROATM_TST_MAX];
    vm_offset_t     scq_cluster_base;          	   /* base of memory for SCQ TX queues */
    int32_t         scq_cluster_size;         	   /* size of memory for SCQ TX queues */
    u_int8_t        scq_ubr0_cluster[IDT_SCQ_SIZE*2];
    u_int32_t       raw_headp;             /* head of raw cell queue, physical */
    struct mbuf     *raw_headm;            /* head of raw cell queue, virtual */
	u_int32_t		raw_hnd[2];			   /* raw cell queue tail pointer and rawcell handle*/
	rcqe			*raw_ch;		   	   /* raw cell head pointer */
    u_int32_t       *tsq_base;             /* virtual TSQ base address */
    u_int32_t       *tsq_head;             /* virtual TSQ head pointer */
    int32_t             tsq_size;              /* number of TSQ entries (1024) */
#define PROATM_MCHECK_COUNT 1024	/* 1024 pointers on mbufs */
    struct mbuf     * *mcheck;
    struct callout_handle stat_ch;
    struct resource *mem;
    struct resource *irq;
    void            *irqcookie;
    bus_space_tag_t bustag;
    bus_space_handle_t bushandle;
    int32_t         pci_rev;               /* hardware revision ID */
    char            hardware [16];         /* hardware description string */
    volatile int32_t    tst_free_entries;
    u_int32_t       flg_25:1;              /* flag indicates 25.6 Mbps instead of 155 Mbps */
    u_int32_t       flg_igcrc:1;           /* ignore receive CRC errors */
    u_int32_t       flg_pad:30;

    Proatm_stats    pu_stats;              /* Statistics */
} proatm_reg_t;

typedef         proatm_reg_t PROATM;
static          proatm_reg_t *nicstar[MAXCARDS];

#define iu_pif           iu_cmn.cu_pif
#define stats_ipdus      iu_pif.pif_ipdus
#define stats_opdus      iu_pif.pif_opdus
#define stats_ibytes     iu_pif.pif_ibytes
#define stats_obytes     iu_pif.pif_obytes
#define stats_ierrors    iu_pif.pif_ierrors
#define stats_oerrors    iu_pif.pif_oerrors
#define stats_cmderrors  iu_pif.pif_cmderrors

/****************************************************************************************/
#define ALIGN_ADDR(addr, alignment) \
        ((((u_int32_t) (addr)) + (((u_int32_t) (alignment)) - 1)) & ~(((u_int32_t) (alignment)) - 1))

#endif /* KERNEL */

#endif /* _PROATM_H */
