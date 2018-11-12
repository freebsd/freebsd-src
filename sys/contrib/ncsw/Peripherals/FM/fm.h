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

/******************************************************************************
 @File          fm.h

 @Description   FM internal structures and definitions.
*//***************************************************************************/
#ifndef __FM_H
#define __FM_H

#include "error_ext.h"
#include "std_ext.h"
#include "fm_ext.h"
#include "fm_ipc.h"


#define __ERR_MODULE__  MODULE_FM

#define FM_MAX_NUM_OF_HW_PORT_IDS           64
#define FM_MAX_NUM_OF_GUESTS                100

/**************************************************************************//**
 @Description       Exceptions
*//***************************************************************************/
#define FM_EX_DMA_BUS_ERROR                 0x80000000      /**< DMA bus error. */
#define FM_EX_DMA_READ_ECC                  0x40000000
#define FM_EX_DMA_SYSTEM_WRITE_ECC          0x20000000
#define FM_EX_DMA_FM_WRITE_ECC              0x10000000
#define FM_EX_FPM_STALL_ON_TASKS            0x08000000      /**< Stall of tasks on FPM */
#define FM_EX_FPM_SINGLE_ECC                0x04000000      /**< Single ECC on FPM */
#define FM_EX_FPM_DOUBLE_ECC                0x02000000
#define FM_EX_QMI_SINGLE_ECC                0x01000000      /**< Single ECC on FPM */
#define FM_EX_QMI_DEQ_FROM_UNKNOWN_PORTID   0x00800000      /**< Dequeu from default queue id */
#define FM_EX_QMI_DOUBLE_ECC                0x00400000
#define FM_EX_BMI_LIST_RAM_ECC              0x00200000
#define FM_EX_BMI_PIPELINE_ECC              0x00100000
#define FM_EX_BMI_STATISTICS_RAM_ECC        0x00080000
#define FM_EX_IRAM_ECC                      0x00040000
#define FM_EX_NURAM_ECC                     0x00020000
#define FM_EX_BMI_DISPATCH_RAM_ECC          0x00010000

#define GET_EXCEPTION_FLAG(bitMask, exception)       switch(exception){ \
    case e_FM_EX_DMA_BUS_ERROR:                                         \
        bitMask = FM_EX_DMA_BUS_ERROR; break;                           \
    case e_FM_EX_DMA_READ_ECC:                                          \
        bitMask = FM_EX_DMA_READ_ECC; break;                            \
    case e_FM_EX_DMA_SYSTEM_WRITE_ECC:                                  \
        bitMask = FM_EX_DMA_SYSTEM_WRITE_ECC; break;                    \
    case e_FM_EX_DMA_FM_WRITE_ECC:                                      \
        bitMask = FM_EX_DMA_FM_WRITE_ECC; break;                        \
    case e_FM_EX_FPM_STALL_ON_TASKS:                                    \
        bitMask = FM_EX_FPM_STALL_ON_TASKS; break;                      \
    case e_FM_EX_FPM_SINGLE_ECC:                                        \
        bitMask = FM_EX_FPM_SINGLE_ECC; break;                          \
    case e_FM_EX_FPM_DOUBLE_ECC:                                        \
        bitMask = FM_EX_FPM_DOUBLE_ECC; break;                          \
    case e_FM_EX_QMI_SINGLE_ECC:                                        \
        bitMask = FM_EX_QMI_SINGLE_ECC; break;                          \
    case e_FM_EX_QMI_DOUBLE_ECC:                                        \
        bitMask = FM_EX_QMI_DOUBLE_ECC; break;                          \
    case e_FM_EX_QMI_DEQ_FROM_UNKNOWN_PORTID:                           \
        bitMask = FM_EX_QMI_DEQ_FROM_UNKNOWN_PORTID; break;             \
    case e_FM_EX_BMI_LIST_RAM_ECC:                                      \
        bitMask = FM_EX_BMI_LIST_RAM_ECC; break;                        \
    case e_FM_EX_BMI_PIPELINE_ECC:                                      \
        bitMask = FM_EX_BMI_PIPELINE_ECC; break;                        \
    case e_FM_EX_BMI_STATISTICS_RAM_ECC:                                \
        bitMask = FM_EX_BMI_STATISTICS_RAM_ECC; break;                  \
    case e_FM_EX_BMI_DISPATCH_RAM_ECC:                                  \
        bitMask = FM_EX_BMI_DISPATCH_RAM_ECC; break;                    \
    case e_FM_EX_IRAM_ECC:                                              \
        bitMask = FM_EX_IRAM_ECC; break;                                \
    case e_FM_EX_MURAM_ECC:                                             \
        bitMask = FM_EX_NURAM_ECC; break;                               \
    default: bitMask = 0;break;}

/**************************************************************************//**
 @Description       defaults
*//***************************************************************************/
#define DEFAULT_exceptions                  (FM_EX_DMA_BUS_ERROR            |\
                                            FM_EX_DMA_READ_ECC              |\
                                            FM_EX_DMA_SYSTEM_WRITE_ECC      |\
                                            FM_EX_DMA_FM_WRITE_ECC          |\
                                            FM_EX_FPM_STALL_ON_TASKS        |\
                                            FM_EX_FPM_SINGLE_ECC            |\
                                            FM_EX_FPM_DOUBLE_ECC            |\
                                            FM_EX_QMI_SINGLE_ECC            |\
                                            FM_EX_QMI_DEQ_FROM_UNKNOWN_PORTID|\
                                            FM_EX_QMI_DOUBLE_ECC            |\
                                            FM_EX_BMI_LIST_RAM_ECC          |\
                                            FM_EX_BMI_PIPELINE_ECC          |\
                                            FM_EX_BMI_STATISTICS_RAM_ECC    |\
                                            FM_EX_BMI_DISPATCH_RAM_ECC      |\
                                            FM_EX_IRAM_ECC                  |\
                                            FM_EX_NURAM_ECC                 )
#define DEFAULT_totalNumOfTasks             (BMI_MAX_NUM_OF_TASKS*3/4)
#define DEFAULT_totalFifoSize               (BMI_MAX_FIFO_SIZE*3/4)
#define DEFAULT_maxNumOfOpenDmas            (BMI_MAX_NUM_OF_DMAS*3/4)
#define DEFAULT_eccEnable                   FALSE
#define DEFAULT_dispLimit                   0
#define DEFAULT_prsDispTh                   16
#define DEFAULT_plcrDispTh                  16
#define DEFAULT_kgDispTh                    16
#define DEFAULT_bmiDispTh                   16
#define DEFAULT_qmiEnqDispTh                16
#define DEFAULT_qmiDeqDispTh                16
#define DEFAULT_fmCtl1DispTh                16
#define DEFAULT_fmCtl2DispTh                16
#define DEFAULT_cacheOverride               e_FM_DMA_NO_CACHE_OR
#ifdef FM_PEDANTIC_DMA
#define DEFAULT_aidOverride                 TRUE
#else
#define DEFAULT_aidOverride                 FALSE
#endif /* FM_PEDANTIC_DMA */
#define DEFAULT_aidMode                     e_FM_DMA_AID_OUT_TNUM
#define DEFAULT_dmaStopOnBusError           FALSE
#define DEFAULT_stopAtBusError              FALSE
#define DEFAULT_axiDbgNumOfBeats            1
#define DEFAULT_dmaCamNumOfEntries          32
#define DEFAULT_dmaCommQLow                 ((DMA_THRESH_MAX_COMMQ+1)/2)
#define DEFAULT_dmaCommQHigh                ((DMA_THRESH_MAX_COMMQ+1)*3/4)
#define DEFAULT_dmaReadIntBufLow            ((DMA_THRESH_MAX_BUF+1)/2)
#define DEFAULT_dmaReadIntBufHigh           ((DMA_THRESH_MAX_BUF+1)*3/4)
#define DEFAULT_dmaWriteIntBufLow           ((DMA_THRESH_MAX_BUF+1)/2)
#define DEFAULT_dmaWriteIntBufHigh          ((DMA_THRESH_MAX_BUF+1)*3/4)
#define DEFAULT_dmaSosEmergency             0
#define DEFAULT_dmaDbgCntMode               e_FM_DMA_DBG_NO_CNT
#define DEFAULT_catastrophicErr             e_FM_CATASTROPHIC_ERR_STALL_PORT
#define DEFAULT_dmaErr                      e_FM_DMA_ERR_CATASTROPHIC
#define DEFAULT_resetOnInit                 FALSE
#define DEFAULT_haltOnExternalActivation    FALSE   /* do not change! if changed, must be disabled for rev1 ! */
#define DEFAULT_haltOnUnrecoverableEccError FALSE   /* do not change! if changed, must be disabled for rev1 ! */
#define DEFAULT_externalEccRamsEnable       FALSE
#define DEFAULT_VerifyUcode                 FALSE
#define DEFAULT_tnumAgingPeriod             0
#define DEFAULT_dmaWatchdog                 0 /* disabled */
#define DEFAULT_mtu                         9600

/**************************************************************************//**
 @Description       Modules registers offsets
*//***************************************************************************/
#define FM_MM_MURAM             0x00000000
#define FM_MM_BMI               0x00080000
#define FM_MM_QMI               0x00080400
#define FM_MM_PRS               0x000c7000
#define FM_MM_KG                0x000C1000
#define FM_MM_DMA               0x000C2000
#define FM_MM_FPM               0x000C3000
#define FM_MM_PLCR              0x000C0000
#define FM_MM_IMEM              0x000C4000

/**************************************************************************//**
 @Description       Interrupt Enable/Mask
*//***************************************************************************/

/**************************************************************************//**
 @Description       Memory Mapped Registers
*//***************************************************************************/

#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(push,1)
#endif /* defined(__MWERKS__) && ... */
#define MEM_MAP_START

typedef _Packed struct
{
    volatile uint32_t   fpmtnc;         /**< FPM TNUM Control */
    volatile uint32_t   fpmpr;          /**< FPM Port_ID FmCtl Association */
    volatile uint32_t   brkc;           /**< FPM Breakpoint Control */
    volatile uint32_t   fpmflc;         /**< FPM Flush Control */
    volatile uint32_t   fpmdis1;        /**< FPM Dispatch Thresholds1 */
    volatile uint32_t   fpmdis2;        /**< FPM Dispatch Thresholds2  */
    volatile uint32_t   fmepi;          /**< FM Error Pending Interrupts */
    volatile uint32_t   fmrie;          /**< FM Error Interrupt Enable */
    volatile uint32_t   fmfpfcev[4];    /**< FPM FMan-Controller Event 1-4 */
    volatile uint8_t    res1[16];       /**< reserved */
    volatile uint32_t   fmfpfcee[4];    /**< PM FMan-Controller Event 1-4 */
    volatile uint8_t    res2[16];       /**< reserved */
    volatile uint32_t   fpmtsc1;        /**< FPM TimeStamp Control1 */
    volatile uint32_t   fpmtsc2;        /**< FPM TimeStamp Control2 */
    volatile uint32_t   fpmtsp;         /**< FPM Time Stamp */
    volatile uint32_t   fpmtsf;         /**< FPM Time Stamp Fraction */
    volatile uint32_t   fmrcr;          /**< FM Rams Control */
    volatile uint32_t   fpmextc;        /**< FPM External Requests Control */
    volatile uint32_t   fpmext1;        /**< FPM External Requests Config1 */
    volatile uint32_t   fpmext2;        /**< FPM External Requests Config2 */
    volatile uint32_t   fpmdrd[16];     /**< FPM Data_Ram Data 0-15 */
    volatile uint32_t   fpmdra;         /**< FPM Data Ram Access */
    volatile uint32_t   fm_ip_rev_1;    /**< FM IP Block Revision 1 */
    volatile uint32_t   fm_ip_rev_2;    /**< FM IP Block Revision 2 */
    volatile uint32_t   fmrstc;         /**< FM Reset Command */
    volatile uint32_t   fmcld;          /**< FM Classifier Debug */
    volatile uint32_t   fmnpi;          /**< FM Normal Pending Interrupts  */
    volatile uint32_t   fmfp_exte;      /**< FPM External Requests Enable */
    volatile uint32_t   fpmem;          /**< FPM Event & Mask */
    volatile uint32_t   fpmcev[4];      /**< FPM CPU Event 1-4 */
    volatile uint8_t    res4[16];       /**< reserved */
    volatile uint32_t   fmfp_ps[0x40];  /**< FPM Port Status */
    volatile uint8_t    reserved1[0x260];
    volatile uint32_t   fpmts[128];     /**< 0x400: FPM Task Status */
} _PackedType t_FmFpmRegs;

#define NUM_OF_DBG_TRAPS    3

typedef _Packed struct
{
   volatile uint32_t   fmbm_init;       /**< BMI Initialization */
   volatile uint32_t   fmbm_cfg1;       /**< BMI Configuration 1 */
   volatile uint32_t   fmbm_cfg2;       /**< BMI Configuration 2 */
   volatile uint32_t   reserved[5];
   volatile uint32_t   fmbm_ievr;       /**< Interrupt Event Register */
   volatile uint32_t   fmbm_ier;        /**< Interrupt Enable Register */
   volatile uint32_t   fmbm_ifr;        /**< Interrupt Force Register */
   volatile uint32_t   reserved1[5];
   volatile uint32_t   fmbm_arb[8];     /**< BMI Arbitration */
   volatile uint32_t   reserved2[12];
   volatile uint32_t   fmbm_dtc[NUM_OF_DBG_TRAPS];      /**< BMI Debug Trap Counter */
   volatile uint32_t   reserved3;
   volatile uint32_t   fmbm_dcv[NUM_OF_DBG_TRAPS][4];   /**< BMI Debug Compare Value */
   volatile uint32_t   fmbm_dcm[NUM_OF_DBG_TRAPS][4];   /**< BMI Debug Compare Mask */
   volatile uint32_t   fmbm_gde;        /**< BMI Global Debug Enable */
   volatile uint32_t   fmbm_pp[63];     /**< BMI Port Parameters */
   volatile uint32_t   reserved4;
   volatile uint32_t   fmbm_pfs[63];    /**< BMI Port FIFO Size */
   volatile uint32_t   reserved5;
   volatile uint32_t   fmbm_ppid[63];   /**< Port Partition ID */
} _PackedType t_FmBmiRegs;

typedef _Packed struct
{
    volatile uint32_t   fmqm_gc;        /**<  General Configuration Register */
    volatile uint32_t   Reserved0;
    volatile uint32_t   fmqm_eie;       /**<  Error Interrupt Event Register */
    volatile uint32_t   fmqm_eien;      /**<  Error Interrupt Enable Register */
    volatile uint32_t   fmqm_eif;       /**<  Error Interrupt Force Register */
    volatile uint32_t   fmqm_ie;        /**<  Interrupt Event Register */
    volatile uint32_t   fmqm_ien;       /**<  Interrupt Enable Register */
    volatile uint32_t   fmqm_if;        /**<  Interrupt Force Register */
    volatile uint32_t   fmqm_gs;        /**<  Global Status Register */
    volatile uint32_t   fmqm_ts;        /**<  Task Status Register */
    volatile uint32_t   fmqm_etfc;      /**<  Enqueue Total Frame Counter */
    volatile uint32_t   fmqm_dtfc;      /**<  Dequeue Total Frame Counter */
    volatile uint32_t   fmqm_dc0;       /**<  Dequeue Counter 0 */
    volatile uint32_t   fmqm_dc1;       /**<  Dequeue Counter 1 */
    volatile uint32_t   fmqm_dc2;       /**<  Dequeue Counter 2 */
    volatile uint32_t   fmqm_dc3;       /**<  Dequeue Counter 3 */
    volatile uint32_t   fmqm_dfdc;      /**<  Dequeue FQID from Default Counter */
    volatile uint32_t   fmqm_dfcc;      /**<  Dequeue FQID from Context Counter */
    volatile uint32_t   fmqm_dffc;      /**<  Dequeue FQID from FD Counter */
    volatile uint32_t   fmqm_dcc;       /**<  Dequeue Confirm Counter */
    volatile uint32_t   Reserved1a[7];
    volatile uint32_t   fmqm_tapc;      /**<  Tnum Aging Period Control */
    volatile uint32_t   fmqm_dmcvc;     /**<  Dequeue MAC Command Valid Counter */
    volatile uint32_t   fmqm_difdcc;    /**<  Dequeue Invalid FD Command Counter */
    volatile uint32_t   fmqm_da1v;      /**<  Dequeue A1 Valid Counter */
    volatile uint32_t   Reserved1b;
    volatile uint32_t   fmqm_dtc;       /**<  0x0080 Debug Trap Counter */
    volatile uint32_t   fmqm_efddd;     /**<  0x0084 Enqueue Frame Descriptor Dynamic Debug */
    volatile uint32_t   Reserved3[2];
    _Packed struct {
        volatile uint32_t   fmqm_dtcfg1;    /**<  0x0090 Debug Trap Configuration 1 Register */
        volatile uint32_t   fmqm_dtval1;    /**<  Debug Trap Value 1 Register */
        volatile uint32_t   fmqm_dtm1;      /**<  Debug Trap Mask 1 Register */
        volatile uint32_t   fmqm_dtc1;      /**<  Debug Trap Counter 1 Register */
        volatile uint32_t   fmqm_dtcfg2;    /**<  Debug Trap Configuration 2 Register */
        volatile uint32_t   fmqm_dtval2;    /**<  Debug Trap Value 2 Register */
        volatile uint32_t   fmqm_dtm2;      /**<  Debug Trap Mask 2 Register */
        volatile uint32_t   Reserved1;
    } _PackedType dbgTraps[NUM_OF_DBG_TRAPS];
} _PackedType t_FmQmiRegs;

typedef _Packed struct
{
    volatile uint32_t   fmdmsr;         /**<    FM DMA status register 0x04 */
    volatile uint32_t   fmdmmr;         /**<    FM DMA mode register 0x08 */
    volatile uint32_t   fmdmtr;         /**<    FM DMA bus threshold register 0x0c */
    volatile uint32_t   fmdmhy;         /**<    FM DMA bus hysteresis register 0x10 */
    volatile uint32_t   fmdmsetr;       /**<    FM DMA SOS emergency Threshold Register 0x14 */
    volatile uint32_t   fmdmtah;        /**<    FM DMA transfer bus address high register 0x18  */
    volatile uint32_t   fmdmtal;        /**<    FM DMA transfer bus address low register 0x1C  */
    volatile uint32_t   fmdmtcid;       /**<    FM DMA transfer bus communication ID register 0x20  */
    volatile uint32_t   fmdmra;         /**<    FM DMA bus internal ram address register 0x24  */
    volatile uint32_t   fmdmrd;         /**<    FM DMA bus internal ram data register 0x28  */
    volatile uint32_t   fmdmwcr;        /**<    FM DMA CAM watchdog counter value 0x2C  */
    volatile uint32_t   fmdmebcr;       /**<    FM DMA CAM base in MURAM register 0x30  */
    volatile uint32_t   fmdmccqdr;      /**<    FM DMA CAM and CMD Queue Debug register 0x34  */
    volatile uint32_t   fmdmccqvr1;     /**<    FM DMA CAM and CMD Queue Value register #1 0x38  */
    volatile uint32_t   fmdmccqvr2;     /**<    FM DMA CAM and CMD Queue Value register #2 0x3C  */
    volatile uint32_t   fmdmcqvr3;      /**<    FM DMA CMD Queue Value register #3 0x40  */
    volatile uint32_t   fmdmcqvr4;      /**<    FM DMA CMD Queue Value register #4 0x44  */
    volatile uint32_t   fmdmcqvr5;      /**<    FM DMA CMD Queue Value register #5 0x48  */
    volatile uint32_t   fmdmsefrc;      /**<    FM DMA Semaphore Entry Full Reject Counter 0x50  */
    volatile uint32_t   fmdmsqfrc;      /**<    FM DMA Semaphore Queue Full Reject Counter 0x54  */
    volatile uint32_t   fmdmssrc;       /**<    FM DMA Semaphore SYNC Reject Counter 0x54  */
    volatile uint32_t   fmdmdcr;        /**<    FM DMA Debug Counter */
    volatile uint32_t   fmdmemsr;       /**<    FM DMA Emrgency Smoother Register */
    volatile uint32_t   reserved;
    volatile uint32_t   fmdmplr[FM_SIZE_OF_LIODN_TABLE/2];
                                        /**<    FM DMA PID-LIODN # register  */
} _PackedType t_FmDmaRegs;

typedef _Packed struct
{
    volatile uint32_t   iadd;           /**<    FM IRAM instruction address register */
    volatile uint32_t   idata;          /**<    FM IRAM instruction data register */
    volatile uint32_t   itcfg;          /**<    FM IRAM timing config register */
    volatile uint32_t   iready;         /**<    FM IRAM ready register */
    volatile uint8_t    res[0x80000-0x10];
} _PackedType t_FMIramRegs;

#define MEM_MAP_END
#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(pop)
#endif /* defined(__MWERKS__) && ... */


/**************************************************************************//**
 @Description       General defines
*//***************************************************************************/

#define FM_DEBUG_STATUS_REGISTER_OFFSET     0x000d1084UL
#define FM_UCODE_DEBUG_INSTRUCTION          0x6ffff805UL


/**************************************************************************//**
 @Description       DMA definitions
*//***************************************************************************/

/* masks */
#define DMA_MODE_AID_OR                     0x20000000
#define DMA_MODE_SBER                       0x10000000
#define DMA_MODE_BER                        0x00200000
#define DMA_MODE_ECC                        0x00000020
#define DMA_MODE_PRIVILEGE_PROT             0x00001000
#define DMA_MODE_SECURE_PROT                0x00000800
#define DMA_MODE_EMERGENCY_READ             0x00080000
#define DMA_MODE_EMERGENCY_WRITE            0x00040000

#define DMA_TRANSFER_PORTID_MASK            0xFF000000
#define DMA_TRANSFER_TNUM_MASK              0x00FF0000
#define DMA_TRANSFER_LIODN_MASK             0x00000FFF

#define DMA_HIGH_LIODN_MASK                 0x0FFF0000
#define DMA_LOW_LIODN_MASK                  0x00000FFF

#define DMA_STATUS_CMD_QUEUE_NOT_EMPTY      0x10000000
#define DMA_STATUS_BUS_ERR                  0x08000000
#define DMA_STATUS_READ_ECC                 0x04000000
#define DMA_STATUS_SYSTEM_WRITE_ECC         0x02000000
#define DMA_STATUS_FM_WRITE_ECC             0x01000000
#define DMA_STATUS_SYSTEM_DPEXT_ECC         0x00800000
#define DMA_STATUS_FM_DPEXT_ECC             0x00400000
#define DMA_STATUS_SYSTEM_DPDAT_ECC         0x00200000
#define DMA_STATUS_FM_DPDAT_ECC             0x00100000
#define DMA_STATUS_FM_SPDAT_ECC             0x00080000

#define FM_LIODN_BASE_MASK                  0x00000FFF

/* shifts */
#define DMA_MODE_CACHE_OR_SHIFT             30
#define DMA_MODE_BUS_PRI_SHIFT              16
#define DMA_MODE_AXI_DBG_SHIFT              24
#define DMA_MODE_CEN_SHIFT                  13
#define DMA_MODE_BUS_PROT_SHIFT             10
#define DMA_MODE_DBG_SHIFT                  7
#define DMA_MODE_EMERGENCY_LEVEL_SHIFT      6
#define DMA_MODE_AID_MODE_SHIFT             4
#define DMA_MODE_MAX_AXI_DBG_NUM_OF_BEATS   16
#define DMA_MODE_MAX_CAM_NUM_OF_ENTRIES     32

#define DMA_THRESH_COMMQ_SHIFT              24
#define DMA_THRESH_READ_INT_BUF_SHIFT       16

#define DMA_LIODN_SHIFT                     16

#define DMA_TRANSFER_PORTID_SHIFT           24
#define DMA_TRANSFER_TNUM_SHIFT             16

/* sizes */
#define DMA_MAX_WATCHDOG                    0xffffffff

/* others */
#define DMA_CAM_SIZEOF_ENTRY                0x40
#define DMA_CAM_ALIGN                       0x1000
#define DMA_CAM_UNITS                       8


/**************************************************************************//**
 @Description       FPM defines
*//***************************************************************************/

/* masks */
#define FPM_EV_MASK_DOUBLE_ECC          0x80000000
#define FPM_EV_MASK_STALL               0x40000000
#define FPM_EV_MASK_SINGLE_ECC          0x20000000
#define FPM_EV_MASK_RELEASE_FM          0x00010000
#define FPM_EV_MASK_DOUBLE_ECC_EN       0x00008000
#define FPM_EV_MASK_STALL_EN            0x00004000
#define FPM_EV_MASK_SINGLE_ECC_EN       0x00002000
#define FPM_EV_MASK_EXTERNAL_HALT       0x00000008
#define FPM_EV_MASK_ECC_ERR_HALT        0x00000004

#define FPM_RAM_CTL_RAMS_ECC_EN         0x80000000
#define FPM_RAM_CTL_IRAM_ECC_EN         0x40000000
#define FPM_RAM_CTL_MURAM_ECC           0x00008000
#define FPM_RAM_CTL_IRAM_ECC            0x00004000
#define FPM_RAM_CTL_MURAM_TEST_ECC      0x20000000
#define FPM_RAM_CTL_IRAM_TEST_ECC       0x10000000
#define FPM_RAM_CTL_RAMS_ECC_EN_SRC_SEL 0x08000000

#define FPM_IRAM_ECC_ERR_EX_EN          0x00020000
#define FPM_MURAM_ECC_ERR_EX_EN         0x00040000

#define FPM_REV1_MAJOR_MASK             0x0000FF00
#define FPM_REV1_MINOR_MASK             0x000000FF

#define FPM_REV2_INTEG_MASK             0x00FF0000
#define FPM_REV2_ERR_MASK               0x0000FF00
#define FPM_REV2_CFG_MASK               0x000000FF

#define FPM_TS_FRACTION_MASK            0x0000FFFF
#define FPM_TS_CTL_EN                   0x80000000

#define FPM_PORT_FM_CTL1                0x00000001
#define FPM_PORT_FM_CTL2                0x00000002
#define FPM_PRC_REALSE_STALLED          0x00800000

#define FPM_PS_STALLED                  0x00800000
#define FPM_PS_FM_CTL1_SEL              0x80000000
#define FPM_PS_FM_CTL2_SEL              0x40000000
#define FPM_PS_FM_CTL_SEL_MASK          (FPM_PS_FM_CTL1_SEL | FPM_PS_FM_CTL2_SEL)

#define FPM_RSTC_FM_RESET               0x80000000
#define FPM_RSTC_10G0_RESET             0x04000000
#define FPM_RSTC_1G0_RESET              0x40000000
#define FPM_RSTC_1G1_RESET              0x20000000
#define FPM_RSTC_1G2_RESET              0x10000000
#define FPM_RSTC_1G3_RESET              0x08000000
#define FPM_RSTC_1G4_RESET              0x02000000


/* shifts */
#define FPM_DISP_LIMIT_SHIFT            24

#define FPM_THR1_PRS_SHIFT              24
#define FPM_THR1_KG_SHIFT               16
#define FPM_THR1_PLCR_SHIFT             8
#define FPM_THR1_BMI_SHIFT              0

#define FPM_THR2_QMI_ENQ_SHIFT          24
#define FPM_THR2_QMI_DEQ_SHIFT          0
#define FPM_THR2_FM_CTL1_SHIFT          16
#define FPM_THR2_FM_CTL2_SHIFT          8

#define FPM_EV_MASK_CAT_ERR_SHIFT       1
#define FPM_EV_MASK_DMA_ERR_SHIFT       0

#define FPM_REV1_MAJOR_SHIFT            8
#define FPM_REV1_MINOR_SHIFT            0

#define FPM_REV2_INTEG_SHIFT            16
#define FPM_REV2_ERR_SHIFT              8
#define FPM_REV2_CFG_SHIFT              0

#define FPM_TS_INT_SHIFT                16

#define FPM_PORT_FM_CTL_PORTID_SHIFT      24

#define FPM_PS_FM_CTL_SEL_SHIFT           30
#define FPM_PRC_ORA_FM_CTL_SEL_SHIFT      16

/* Interrupts defines */
#define FPM_EVENT_FM_CTL_0                0x00008000
#define FPM_EVENT_FM_CTL                  0x0000FF00
#define FPM_EVENT_FM_CTL_BRK              0x00000080

/* others */
#define FPM_MAX_DISP_LIMIT              31

/**************************************************************************//**
 @Description       BMI defines
*//***************************************************************************/
/* masks */
#define BMI_INIT_START                      0x80000000
#define BMI_ERR_INTR_EN_PIPELINE_ECC        0x80000000
#define BMI_ERR_INTR_EN_LIST_RAM_ECC        0x40000000
#define BMI_ERR_INTR_EN_STATISTICS_RAM_ECC  0x20000000
#define BMI_ERR_INTR_EN_DISPATCH_RAM_ECC    0x10000000
#define BMI_NUM_OF_TASKS_MASK               0x3F000000
#define BMI_NUM_OF_EXTRA_TASKS_MASK         0x000F0000
#define BMI_NUM_OF_DMAS_MASK                0x00000F00
#define BMI_NUM_OF_EXTRA_DMAS_MASK          0x0000000F
#define BMI_FIFO_SIZE_MASK                  0x000003FF
#define BMI_EXTRA_FIFO_SIZE_MASK            0x03FF0000
#define BMI_CFG2_DMAS_MASK                  0x0000003F

/* shifts */
#define BMI_CFG2_TASKS_SHIFT            16
#define BMI_CFG2_DMAS_SHIFT             0
#define BMI_CFG1_FIFO_SIZE_SHIFT        16
#define BMI_FIFO_SIZE_SHIFT             0
#define BMI_EXTRA_FIFO_SIZE_SHIFT       16
#define BMI_NUM_OF_TASKS_SHIFT          24
#define BMI_EXTRA_NUM_OF_TASKS_SHIFT    16
#define BMI_NUM_OF_DMAS_SHIFT           8
#define BMI_EXTRA_NUM_OF_DMAS_SHIFT     0

/* others */
#define BMI_FIFO_ALIGN                  0x100


/**************************************************************************//**
 @Description       QMI defines
*//***************************************************************************/
/* masks */
#define QMI_CFG_ENQ_EN                  0x80000000
#define QMI_CFG_DEQ_EN                  0x40000000
#define QMI_CFG_EN_COUNTERS             0x10000000
#define QMI_CFG_SOFT_RESET              0x01000000
#define QMI_CFG_DEQ_MASK                0x0000003F
#define QMI_CFG_ENQ_MASK                0x00003F00

#define QMI_ERR_INTR_EN_DOUBLE_ECC      0x80000000
#define QMI_ERR_INTR_EN_DEQ_FROM_DEF    0x40000000
#define QMI_INTR_EN_SINGLE_ECC          0x80000000

/* shifts */
#define QMI_CFG_ENQ_SHIFT               8
#define QMI_TAPC_TAP                    22


/**************************************************************************//**
 @Description       IRAM defines
*//***************************************************************************/
/* masks */
#define IRAM_IADD_AIE                   0x80000000
#define IRAM_READY                      0x80000000

typedef struct {
    void        (*f_Isr) (t_Handle h_Arg, uint32_t event);
    t_Handle    h_SrcHandle;
} t_FmanCtrlIntrSrc;


typedef struct
{
 /*   uint8_t                     numOfPartitions; */
    bool                        resetOnInit;
#ifdef FM_PARTITION_ARRAY
    uint16_t                    liodnBasePerPort[FM_SIZE_OF_LIODN_TABLE];
#endif
    bool                        enCounters;
    t_FmThresholds              thresholds;
    e_FmDmaCacheOverride        dmaCacheOverride;
    e_FmDmaAidMode              dmaAidMode;
    bool                        dmaAidOverride;
    uint8_t                     dmaAxiDbgNumOfBeats;
    uint8_t                     dmaCamNumOfEntries;
    uint32_t                    dmaWatchdog;
    t_FmDmaThresholds           dmaCommQThresholds;
    t_FmDmaThresholds           dmaWriteBufThresholds;
    t_FmDmaThresholds           dmaReadBufThresholds;
    uint32_t                    dmaSosEmergency;
    e_FmDmaDbgCntMode           dmaDbgCntMode;
    bool                        dmaStopOnBusError;
    bool                        dmaEnEmergency;
    t_FmDmaEmergency            dmaEmergency;
    bool                        dmaEnEmergencySmoother;
    uint32_t                    dmaEmergencySwitchCounter;
    bool                        haltOnExternalActivation;
    bool                        haltOnUnrecoverableEccError;
    e_FmCatastrophicErr         catastrophicErr;
    e_FmDmaErr                  dmaErr;
    bool                        enMuramTestMode;
    bool                        enIramTestMode;
    bool                        externalEccRamsEnable;
    uint16_t                    tnumAgingPeriod;
    t_FmPcdFirmwareParams       firmware;
    bool                        fwVerify;
} t_FmDriverParam;

typedef void (t_FmanCtrlIsr)( t_Handle h_Fm, uint32_t event);

typedef struct
{
/***************************/
/* Master/Guest parameters */
/***************************/
    uint8_t                     fmId;
    e_FmPortType                portsTypes[FM_MAX_NUM_OF_HW_PORT_IDS];
    uint16_t                    fmClkFreq;
/**************************/
/* Master Only parameters */
/**************************/
    bool                        enabledTimeStamp;
    uint8_t                     count1MicroBit;
    uint8_t                     totalNumOfTasks;
    uint32_t                    totalFifoSize;
    uint8_t                     maxNumOfOpenDmas;
    uint8_t                     accumulatedNumOfTasks;
    uint32_t                    accumulatedFifoSize;
    uint8_t                     accumulatedNumOfOpenDmas;
#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
    uint8_t                     accumulatedNumOfDeqTnums;
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */
#ifdef FM_LOW_END_RESTRICTION
    bool                        lowEndRestriction;
#endif /* FM_LOW_END_RESTRICTION */
    uint32_t                    exceptions;
    int                         irq;
    int                         errIrq;
    bool                        ramsEccEnable;
    bool                        explicitEnable;
    bool                        internalCall;
    uint8_t                     ramsEccOwners;
    uint32_t                    extraFifoPoolSize;
    uint8_t                     extraTasksPoolSize;
    uint8_t                     extraOpenDmasPoolSize;
#if defined(FM_MAX_NUM_OF_10G_MACS) && (FM_MAX_NUM_OF_10G_MACS)
    uint16_t                    macMaxFrameLengths10G[FM_MAX_NUM_OF_10G_MACS];
#endif /* defined(FM_MAX_NUM_OF_10G_MACS) && (FM_MAX_NUM_OF_10G_MACS) */
    uint16_t                    macMaxFrameLengths1G[FM_MAX_NUM_OF_1G_MACS];
} t_FmStateStruct;

typedef struct
{
/***************************/
/* Master/Guest parameters */
/***************************/
/* locals for recovery */
    uintptr_t                   baseAddr;

/* un-needed for recovery */
    t_Handle                    h_Pcd;
    char                        fmModuleName[MODULE_NAME_SIZE];
    char                        fmIpcHandlerModuleName[FM_MAX_NUM_OF_GUESTS][MODULE_NAME_SIZE];
    t_Handle                    h_IpcSessions[FM_MAX_NUM_OF_GUESTS];
    t_FmIntrSrc                 intrMng[e_FM_EV_DUMMY_LAST];    /* FM exceptions user callback */
    uint8_t                     guestId;
/**************************/
/* Master Only parameters */
/**************************/
/* locals for recovery */
    t_FmFpmRegs                 *p_FmFpmRegs;
    t_FmBmiRegs                 *p_FmBmiRegs;
    t_FmQmiRegs                 *p_FmQmiRegs;
    t_FmDmaRegs                 *p_FmDmaRegs;
    t_FmExceptionsCallback      *f_Exception;
    t_FmBusErrorCallback        *f_BusError;
    t_Handle                    h_App;                          /* Application handle */
    t_Handle                    h_Spinlock;
    bool                        recoveryMode;
    t_FmStateStruct             *p_FmStateStruct;

/* un-needed for recovery */
    t_FmDriverParam             *p_FmDriverParam;
    t_Handle                    h_FmMuram;
    uint64_t                    fmMuramPhysBaseAddr;
    bool                        independentMode;
    bool                        hcPortInitialized;
    uintptr_t                   camBaseAddr;                    /* save for freeing */
    uintptr_t                   resAddr;
    uintptr_t                   fifoBaseAddr;                   /* save for freeing */
    t_FmanCtrlIntrSrc           fmanCtrlIntr[FM_NUM_OF_FMAN_CTRL_EVENT_REGS];    /* FM exceptions user callback */
    bool                        usedEventRegs[FM_NUM_OF_FMAN_CTRL_EVENT_REGS];
} t_Fm;


#endif /* __FM_H */
