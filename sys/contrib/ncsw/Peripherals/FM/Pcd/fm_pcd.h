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
 @File          fm_pcd.h

 @Description   FM PCD ...
*//***************************************************************************/
#ifndef __FM_PCD_H
#define __FM_PCD_H

#include "std_ext.h"
#include "error_ext.h"
#include "list_ext.h"
#include "fm_pcd_ext.h"


#define __ERR_MODULE__  MODULE_FM_PCD


/**************************************************************************//**
 @Group         FM_PCD_Runtime_grp FM PCD Runtime Unit
 @{
*//***************************************************************************/

/****************************/
/* Network defines          */
/****************************/
#define UDP_HEADER_SIZE     8

#define ESP_SPI_OFFSET      0
#define ESP_SPI_SIZE        4
#define ESP_SEQ_NUM_OFFSET  ESP_SPI_SIZE
#define ESP_SEQ_NUM_SIZE    4

/****************************/
/* General defines          */
/****************************/
#define ILLEGAL_CLS_PLAN    0xff
#define ILLEGAL_NETENV      0xff
/****************************/
/* Error defines           */
/****************************/
#define FM_PCD_EX_KG_DOUBLE_ECC                     0x80000000
#define FM_PCD_EX_KG_KEYSIZE_OVERFLOW               0x40000000

#define FM_PCD_EX_PLCR_DOUBLE_ECC                   0x20000000
#define FM_PCD_EX_PLCR_INIT_ENTRY_ERROR             0x10000000
#define FM_PCD_EX_PLCR_PRAM_SELF_INIT_COMPLETE      0x08000000
#define FM_PCD_EX_PLCR_ATOMIC_ACTION_COMPLETE       0x04000000

#define FM_PCD_EX_PRS_DOUBLE_ECC                    0x02000000
#define FM_PCD_EX_PRS_SINGLE_ECC                    0x01000000

#define GET_FM_PCD_EXCEPTION_FLAG(bitMask, exception)               \
switch(exception){                                                  \
    case e_FM_PCD_KG_EXCEPTION_DOUBLE_ECC:                          \
        bitMask = FM_PCD_EX_KG_DOUBLE_ECC; break;                   \
    case e_FM_PCD_PLCR_EXCEPTION_DOUBLE_ECC:                        \
        bitMask = FM_PCD_EX_PLCR_DOUBLE_ECC; break;                 \
    case e_FM_PCD_KG_EXCEPTION_KEYSIZE_OVERFLOW:                    \
        bitMask = FM_PCD_EX_KG_KEYSIZE_OVERFLOW; break;             \
    case e_FM_PCD_PLCR_EXCEPTION_INIT_ENTRY_ERROR:                  \
        bitMask = FM_PCD_EX_PLCR_INIT_ENTRY_ERROR; break;           \
    case e_FM_PCD_PLCR_EXCEPTION_PRAM_SELF_INIT_COMPLETE:           \
        bitMask = FM_PCD_EX_PLCR_PRAM_SELF_INIT_COMPLETE; break;    \
    case e_FM_PCD_PLCR_EXCEPTION_ATOMIC_ACTION_COMPLETE:            \
        bitMask = FM_PCD_EX_PLCR_ATOMIC_ACTION_COMPLETE; break;     \
    case e_FM_PCD_PRS_EXCEPTION_DOUBLE_ECC:                         \
        bitMask = FM_PCD_EX_PRS_DOUBLE_ECC; break;                  \
    case e_FM_PCD_PRS_EXCEPTION_SINGLE_ECC:                         \
        bitMask = FM_PCD_EX_PRS_SINGLE_ECC; break;                  \
    default: bitMask = 0;break;}

/***********************************************************************/
/*          SW parser L4 shells patch                                  */
/***********************************************************************/
#ifdef FM_PRS_L4_SHELL_ERRATA_FMANb
#define SW_PRS_L4_PATCH                         \
{   0x31,0x92,0x02,0x1f,0x00,0x32,0x00,0x78,    \
    0x00,0x34,0x32,0xf0,0x00,0x50,0x00,0x0c,    \
    0x28,0x5e,0x83,0x8e,0x29,0x32,0xaf,0x8e,    \
    0x31,0xb2,0x9f,0xff,0x00,0x06,0xaf,0xbf,    \
    0x00,0x06,0x29,0x36,0x00,0x01,0x1b,0xff,    \
    0x32,0xf0,0x00,0x50,0x00,0x08,0x28,0x5e,    \
    0x08,0x99,0x00,0x00,0x9f,0x8e,0x31,0xb2,    \
    0x9f,0xff,0x00,0x06,0x29,0x36,0x00,0x01,    \
    0x1b,0xff,0x32,0xf0,0x00,0x50,0x00,0x04,    \
    0x28,0x5e,0x8f,0x9e,0x29,0x32,0x31,0xb2,    \
    0x8f,0xbf,0x00,0x06,0x29,0x36,0x00,0x01,    \
    0x1b,0xff,0x32,0xf0,0x00,0x50,0x00,0x04,    \
    0x28,0x5e,0x8f,0x9e,0x29,0x32,0x31,0xb2,    \
    0x8f,0xbf,0x00,0x06,0x29,0x36,0x00,0x01,    \
    0x1b,0xff,0x00,0x00,0x00,0x00,0x00,0x00};

#define SW_PRS_L4_PATCH_SIZE                120
#endif /* FM_PRS_L4_SHELL_ERRATA_FMANb */

/****************************/
/* Parser defines           */
/****************************/
/* masks */
#define PRS_ERR_CAP                         0x80000000
#define PRS_ERR_TYPE_DOUBLE                 0x40000000
#define PRS_ERR_SINGLE_ECC_CNT_MASK         0x00FF0000
#define PRS_ERR_ADDR_MASK                   0x000001FF
#define FM_PCD_PRS_RPIMAC_EN                0x00000001
#define FM_PCD_PRS_SINGLE_ECC               0x00004000
#define FM_PCD_PRS_PORT_IDLE_STS            0xffff0000
#define FM_PCD_PRS_DOUBLE_ECC               0x00004000
#define FM_PCD_PRS_PPSC_ALL_PORTS           0xffff0000

/* others */
#define PRS_MAX_CYCLE_LIMIT                 8191
#define PRS_SW_DATA                         0x00000800
#define PRS_REGS_OFFSET                     0x00000840

#define GET_FM_PCD_PRS_PORT_ID(prsPortId,hardwarePortId) \
    prsPortId = (uint8_t)(hardwarePortId & 0x0f)

#define GET_FM_PCD_INDEX_FLAG(bitMask, prsPortId)    \
    bitMask = 0x80000000>>prsPortId

/***********************************************************************/
/*          Keygen defines                                             */
/***********************************************************************/
/* Masks */
#define FM_PCD_KG_KGGCR_EN                      0x80000000
#define KG_SCH_GEN_VALID                        0x80000000
#define KG_SCH_GEN_EXTRACT_TYPE                 0x00008000
#define KG_ERR_CAP                              0x80000000
#define KG_ERR_TYPE_DOUBLE                      0x40000000
#define KG_ERR_ADDR_MASK                        0x00000FFF
#define FM_PCD_KG_DOUBLE_ECC                    0x80000000
#define FM_PCD_KG_KEYSIZE_OVERFLOW              0x40000000
#define KG_SCH_MODE_EN                          0x80000000

/* shifts */
#define FM_PCD_KG_PE_CPP_MASK_SHIFT             16
#define FM_PCD_KG_KGAR_WSEL_SHIFT               8

/* others */
#define KG_DOUBLE_MEANING_REGS_OFFSET           0x100
#define NO_VALIDATION                           0x70
#define KG_ACTION_REG_TO                        1024
#define KG_MAX_PROFILE                          255
#define SCHEME_ALWAYS_DIRECT                    0xFFFFFFFF

typedef struct {
    bool        known;
    uint8_t     id;
} t_FmPcdKgSchemesExtractsEntry;

typedef struct {
    t_FmPcdKgSchemesExtractsEntry extractsArray[FM_PCD_KG_MAX_NUM_OF_EXTRACTS_PER_KEY];
} t_FmPcdKgSchemesExtracts;

/***********************************************************************/
/*          Policer defines                                            */
/***********************************************************************/

/* masks */
#define FM_PCD_PLCR_PEMODE_PI                 0x80000000
#define FM_PCD_PLCR_PEMODE_CBLND              0x40000000
#define FM_PCD_PLCR_PEMODE_ALG_MASK           0x30000000
#define FM_PCD_PLCR_PEMODE_ALG_RFC2698        0x10000000
#define FM_PCD_PLCR_PEMODE_ALG_RFC4115        0x20000000
#define FM_PCD_PLCR_PEMODE_DEFC_MASK          0x0C000000
#define FM_PCD_PLCR_PEMODE_DEFC_Y             0x04000000
#define FM_PCD_PLCR_PEMODE_DEFC_R             0x08000000
#define FM_PCD_PLCR_PEMODE_DEFC_OVERRIDE      0x0C000000
#define FM_PCD_PLCR_PEMODE_OVCLR_MASK         0x03000000
#define FM_PCD_PLCR_PEMODE_OVCLR_Y            0x01000000
#define FM_PCD_PLCR_PEMODE_OVCLR_R            0x02000000
#define FM_PCD_PLCR_PEMODE_OVCLR_G_NC         0x03000000
#define FM_PCD_PLCR_PEMODE_PKT                0x00800000
#define FM_PCD_PLCR_PEMODE_FPP_MASK           0x001F0000
#define FM_PCD_PLCR_PEMODE_FPP_SHIFT          16
#define FM_PCD_PLCR_PEMODE_FLS_MASK           0x0000F000
#define FM_PCD_PLCR_PEMODE_FLS_L2             0x00003000
#define FM_PCD_PLCR_PEMODE_FLS_L3             0x0000B000
#define FM_PCD_PLCR_PEMODE_FLS_L4             0x0000E000
#define FM_PCD_PLCR_PEMODE_FLS_FULL           0x0000F000
#define FM_PCD_PLCR_PEMODE_RBFLS              0x00000800
#define FM_PCD_PLCR_PEMODE_TRA                0x00000004
#define FM_PCD_PLCR_PEMODE_TRB                0x00000002
#define FM_PCD_PLCR_PEMODE_TRC                0x00000001
#define FM_PCD_PLCR_DOUBLE_ECC                0x80000000
#define FM_PCD_PLCR_INIT_ENTRY_ERROR          0x40000000
#define FM_PCD_PLCR_PRAM_SELF_INIT_COMPLETE   0x80000000
#define FM_PCD_PLCR_ATOMIC_ACTION_COMPLETE    0x40000000

#define FM_PCD_PLCR_NIA_VALID                 0x80000000

#define FM_PCD_PLCR_GCR_EN                    0x80000000
#define FM_PCD_PLCR_GCR_STEN                  0x40000000
#define FM_PCD_PLCR_GCR_DAR                   0x20000000
#define FM_PCD_PLCR_GCR_DEFNIA                0x00FFFFFF
#define FM_PCD_PLCR_NIA_ABS                   0x00000100

#define FM_PCD_PLCR_GSR_BSY                   0x80000000
#define FM_PCD_PLCR_GSR_DQS                   0x60000000
#define FM_PCD_PLCR_GSR_RPB                   0x20000000
#define FM_PCD_PLCR_GSR_FQS                   0x0C000000
#define FM_PCD_PLCR_GSR_LPALG                 0x0000C000
#define FM_PCD_PLCR_GSR_LPCA                  0x00003000
#define FM_PCD_PLCR_GSR_LPNUM                 0x000000FF

#define FM_PCD_PLCR_EVR_PSIC                  0x80000000
#define FM_PCD_PLCR_EVR_AAC                   0x40000000

#define FM_PCD_PLCR_PAR_PSI                   0x20000000
#define FM_PCD_PLCR_PAR_PNUM                  0x00FF0000
/* PWSEL Selctive select options */
#define FM_PCD_PLCR_PAR_PWSEL_PEMODE          0x00008000    /* 0 */
#define FM_PCD_PLCR_PAR_PWSEL_PEGNIA          0x00004000    /* 1 */
#define FM_PCD_PLCR_PAR_PWSEL_PEYNIA          0x00002000    /* 2 */
#define FM_PCD_PLCR_PAR_PWSEL_PERNIA          0x00001000    /* 3 */
#define FM_PCD_PLCR_PAR_PWSEL_PECIR           0x00000800    /* 4 */
#define FM_PCD_PLCR_PAR_PWSEL_PECBS           0x00000400    /* 5 */
#define FM_PCD_PLCR_PAR_PWSEL_PEPIR_EIR       0x00000200    /* 6 */
#define FM_PCD_PLCR_PAR_PWSEL_PEPBS_EBS       0x00000100    /* 7 */
#define FM_PCD_PLCR_PAR_PWSEL_PELTS           0x00000080    /* 8 */
#define FM_PCD_PLCR_PAR_PWSEL_PECTS           0x00000040    /* 9 */
#define FM_PCD_PLCR_PAR_PWSEL_PEPTS_ETS       0x00000020    /* 10 */
#define FM_PCD_PLCR_PAR_PWSEL_PEGPC           0x00000010    /* 11 */
#define FM_PCD_PLCR_PAR_PWSEL_PEYPC           0x00000008    /* 12 */
#define FM_PCD_PLCR_PAR_PWSEL_PERPC           0x00000004    /* 13 */
#define FM_PCD_PLCR_PAR_PWSEL_PERYPC          0x00000002    /* 14 */
#define FM_PCD_PLCR_PAR_PWSEL_PERRPC          0x00000001    /* 15 */

#define FM_PCD_PLCR_PAR_PMR_BRN_1TO1          0x0000   /* - Full bit replacement. {PBNUM[0:N-1]
                                                           1-> 2^N specific locations. */
#define FM_PCD_PLCR_PAR_PMR_BRN_2TO2          0x1      /* - {PBNUM[0:N-2],PNUM[N-1]}.
                                                           2-> 2^(N-1) base locations. */
#define FM_PCD_PLCR_PAR_PMR_BRN_4TO4          0x2      /* - {PBNUM[0:N-3],PNUM[N-2:N-1]}.
                                                           4-> 2^(N-2) base locations. */
#define FM_PCD_PLCR_PAR_PMR_BRN_8TO8          0x3      /* - {PBNUM[0:N-4],PNUM[N-3:N-1]}.
                                                           8->2^(N-3) base locations. */
#define FM_PCD_PLCR_PAR_PMR_BRN_16TO16        0x4      /* - {PBNUM[0:N-5],PNUM[N-4:N-1]}.
                                                           16-> 2^(N-4) base locations. */
#define FM_PCD_PLCR_PAR_PMR_BRN_32TO32        0x5      /* {PBNUM[0:N-6],PNUM[N-5:N-1]}.
                                                           32-> 2^(N-5) base locations. */
#define FM_PCD_PLCR_PAR_PMR_BRN_64TO64        0x6      /* {PBNUM[0:N-7],PNUM[N-6:N-1]}.
                                                           64-> 2^(N-6) base locations. */
#define FM_PCD_PLCR_PAR_PMR_BRN_128TO128      0x7      /* {PBNUM[0:N-8],PNUM[N-7:N-1]}.
                                                            128-> 2^(N-7) base locations. */
#define FM_PCD_PLCR_PAR_PMR_BRN_256TO256      0x8      /* - No bit replacement for N=8. {PNUM[N-8:N-1]}.
                                                            When N=8 this option maps all 256 profiles by the DISPATCH bus into one group. */

#define FM_PCD_PLCR_PMR_V                     0x80000000
#define PLCR_ERR_ECC_CAP                      0x80000000
#define PLCR_ERR_ECC_TYPE_DOUBLE              0x40000000
#define PLCR_ERR_ECC_PNUM_MASK                0x00000FF0
#define PLCR_ERR_ECC_OFFSET_MASK              0x0000000F

#define PLCR_ERR_UNINIT_CAP                   0x80000000
#define PLCR_ERR_UNINIT_NUM_MASK              0x000000FF
#define PLCR_ERR_UNINIT_PID_MASK              0x003f0000
#define PLCR_ERR_UNINIT_ABSOLUTE_MASK         0x00008000

/* shifts */
#define PLCR_ERR_ECC_PNUM_SHIFT               4
#define PLCR_ERR_UNINIT_PID_SHIFT             16

#define FM_PCD_PLCR_PMR_BRN_SHIFT             16

/* others */
#define WAIT_FOR_PLCR_EVR_AAC \
{\
    uint32_t count = 0; \
    uint32_t tmpReg32; \
    while (count < FM_PCD_PLCR_POLL) \
    { \
        tmpReg32 = GET_UINT32(p_FmPcdPlcrRegs->fmpl_evr);\
        if (!( tmpReg32 & FM_PCD_PLCR_EVR_AAC)) break;\
        count++;\
    }\
}

#define WAIT_FOR_PLCR_PAR_GO \
{\
    uint32_t count = 0; \
    uint32_t tmpReg32; \
    while (count < FM_PCD_PLCR_POLL) \
    { \
        tmpReg32 = GET_UINT32(p_FmPcdPlcrRegs->fmpl_par);\
        if (!( tmpReg32 & FM_PCD_PLCR_PAR_GO)) break;\
        count++; \
    }\
}

#define PLCR_PORT_WINDOW_SIZE(hardwarePortId)

/****************************/
/* Defaults                 */
/****************************/
#define DEFAULT_plcrAutoRefresh                 FALSE
#define DEFAULT_prsMaxParseCycleLimit           0
#define DEFAULT_fmPcdKgErrorExceptions          (FM_PCD_EX_KG_DOUBLE_ECC | FM_PCD_EX_KG_KEYSIZE_OVERFLOW)
#define DEFAULT_fmPcdPlcrErrorExceptions        (FM_PCD_EX_PLCR_DOUBLE_ECC | FM_PCD_EX_PLCR_INIT_ENTRY_ERROR)
#define DEFAULT_fmPcdPlcrExceptions             0
#define DEFAULT_fmPcdPrsErrorExceptions         (FM_PCD_EX_PRS_DOUBLE_ECC)

#define DEFAULT_fmPcdPrsExceptions              FM_PCD_EX_PRS_SINGLE_ECC
#define DEFAULT_numOfUsedProfilesPerWindow      16
#define DEFAULT_numOfSharedPlcrProfiles         4

/***********************************************************************/
/*          Memory map                                                 */
/***********************************************************************/
#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(push,1)
#endif /* defined(__MWERKS__) && ... */
#define MEM_MAP_START

typedef _Packed struct {
   volatile uint32_t kgoe_sp;
   volatile uint32_t kgoe_cpp;
} _PackedType t_FmPcdKgPortConfigRegs;

typedef _Packed struct {
    volatile uint32_t kgcpe[8];
} _PackedType t_FmPcdKgClsPlanRegs;

typedef _Packed union {
    t_FmPcdKgInterModuleSchemeRegs  schemeRegs;
    t_FmPcdKgPortConfigRegs         portRegs;
    t_FmPcdKgClsPlanRegs            clsPlanRegs;
} _PackedType u_FmPcdKgIndirectAccessRegs;

typedef _Packed struct {
    volatile uint32_t kggcr;
    volatile uint32_t res0;
    volatile uint32_t res1;
    volatile uint32_t kgeer;
    volatile uint32_t kgeeer;
    volatile uint32_t res2;
    volatile uint32_t res3;
    volatile uint32_t kgseer;
    volatile uint32_t kgseeer;
    volatile uint32_t kggsr;
    volatile uint32_t kgtpc;
    volatile uint32_t kgserc;
    volatile uint32_t res4[4];
    volatile uint32_t kgfdor;
    volatile uint32_t kggdv0r;
    volatile uint32_t kggdv1r;
    volatile uint32_t res5[5];
    volatile uint32_t kgfer;
    volatile uint32_t kgfeer;
    volatile uint32_t res6[38];
    u_FmPcdKgIndirectAccessRegs   indirectAccessRegs;
    volatile uint32_t res[42];                  /*(0xfc-sizeof(u_FmPcdKgIndirectAccessRegs))/4 */
    volatile uint32_t kgar;
} _PackedType t_FmPcdKgRegs;

typedef _Packed struct {
/* General Configuration and Status Registers */
    volatile uint32_t fmpl_gcr;         /* 0x000 FMPL_GCR  - FM Policer General Configuration */
    volatile uint32_t fmpl_gsr;         /* 0x004 FMPL_GSR  - FM Policer Global Status Register */
    volatile uint32_t fmpl_evr;         /* 0x008 FMPL_EVR  - FM Policer Event Register */
    volatile uint32_t fmpl_ier;         /* 0x00C FMPL_IER  - FM Policer Interrupt Enable Register */
    volatile uint32_t fmpl_ifr;         /* 0x010 FMPL_IFR  - FM Policer Interrupt Force Register */
    volatile uint32_t fmpl_eevr;        /* 0x014 FMPL_EEVR - FM Policer Error Event Register */
    volatile uint32_t fmpl_eier;        /* 0x018 FMPL_EIER - FM Policer Error Interrupt Enable Register */
    volatile uint32_t fmpl_eifr;        /* 0x01C FMPL_EIFR - FM Policer Error Interrupt Force Register */
/* Global Statistic Counters */
    volatile uint32_t fmpl_rpcnt;       /* 0x020 FMPL_RPC  - FM Policer RED Packets Counter */
    volatile uint32_t fmpl_ypcnt;       /* 0x024 FMPL_YPC  - FM Policer YELLOW Packets Counter */
    volatile uint32_t fmpl_rrpcnt;      /* 0x028 FMPL_RRPC - FM Policer Recolored RED Packet Counter */
    volatile uint32_t fmpl_rypcnt;      /* 0x02C FMPL_RYPC - FM Policer Recolored YELLOW Packet Counter */
    volatile uint32_t fmpl_tpcnt;       /* 0x030 FMPL_TPC  - FM Policer Total Packet Counter */
    volatile uint32_t fmpl_flmcnt;      /* 0x034 FMPL_FLMC - FM Policer Frame Length Mismatch Counter */
    volatile uint32_t fmpl_res0[21];    /* 0x038 - 0x08B Reserved */
/* Profile RAM Access Registers */
    volatile uint32_t fmpl_par;         /* 0x08C FMPL_PAR    - FM Policer Profile Action Register*/
    t_FmPcdPlcrInterModuleProfileRegs profileRegs;
/* Error Capture Registers */
    volatile uint32_t fmpl_serc;        /* 0x100 FMPL_SERC - FM Policer Soft Error Capture */
    volatile uint32_t fmpl_upcr;        /* 0x104 FMPL_UPCR - FM Policer Uninitialized Profile Capture Register */
    volatile uint32_t fmpl_res2;        /* 0x108 Reserved */
/* Debug Registers */
    volatile uint32_t fmpl_res3[61];    /* 0x10C-0x200 Reserved Debug*/
/* Profile Selection Mapping Registers Per Port-ID (n=1-11, 16) */
    volatile uint32_t fmpl_dpmr;        /* 0x200 FMPL_DPMR - FM Policer Default Mapping Register */
    volatile uint32_t fmpl_pmr[63];     /*+default 0x204-0x2FF FMPL_PMR1 - FMPL_PMR63, - FM Policer Profile Mapping Registers.
                                           (for port-ID 1-11, only for supported Port-ID registers) */
} _PackedType t_FmPcdPlcrRegs;

typedef _Packed struct {
    volatile uint32_t rpclim;
    volatile uint32_t rpimac;
    volatile uint32_t pmeec;
    volatile uint32_t res1[5];
    volatile uint32_t pevr;
    volatile uint32_t pever;
    volatile uint32_t pevfr;
    volatile uint32_t perr;
    volatile uint32_t perer;
    volatile uint32_t perfr;
    volatile uint32_t res2[0xA];
    volatile uint32_t ppsc;
    volatile uint32_t res3;
    volatile uint32_t pds;
    volatile uint32_t l2rrs;
    volatile uint32_t l3rrs;
    volatile uint32_t l4rrs;
    volatile uint32_t srrs;
    volatile uint32_t l2rres;
    volatile uint32_t l3rres;
    volatile uint32_t l4rres;
    volatile uint32_t srres;
    volatile uint32_t spcs;
    volatile uint32_t spscs;
    volatile uint32_t hxscs;
    volatile uint32_t mrcs;
    volatile uint32_t mwcs;
    volatile uint32_t mrscs;
    volatile uint32_t mwscs;
    volatile uint32_t fcscs;
} _PackedType t_FmPcdPrsRegs;

#define MEM_MAP_END
#if defined(__MWERKS__) && !defined(__GNUC__)
#pragma pack(pop)
#endif /* defined(__MWERKS__) && ... */


/***********************************************************************/
/*  Driver's internal structures                                       */
/***********************************************************************/

typedef struct {
    t_Handle        h_Manip;
    bool            keepRes;
    e_FmPcdEngine   nextEngine;
    uint8_t         parseCode;
} t_FmPcdInfoForManip;

/**************************************************************************//**
 @Description   A structure of parameters to communicate
                between the port and PCD regarding the KG scheme.
*//***************************************************************************/
typedef struct {
    uint8_t                     netEnvId;    /* in */
    uint8_t                     numOfDistinctionUnits; /* in */
    uint8_t                     unitIds[FM_PCD_MAX_NUM_OF_DISTINCTION_UNITS]; /* in */
    uint32_t                    vector; /* out */
} t_NetEnvParams;

typedef struct {
    volatile bool       lock;
    bool                used;
    uint8_t             owners;
    uint8_t             netEnvId;
    uint8_t             guestId;
    uint8_t             baseEntry;
    uint16_t            sizeOfGrp;
    protocolOpt_t       optArray[FM_PCD_MAX_NUM_OF_OPTIONS(FM_PCD_MAX_NUM_OF_CLS_PLANS)];
} t_FmPcdKgClsPlanGrp;

typedef struct {
    volatile bool       lock;
    bool                valid;
    uint8_t             netEnvId;
    uint8_t             owners;
    uint32_t            matchVector;
    uint32_t            ccUnits;
    bool                nextRelativePlcrProfile;
    uint16_t            relativeProfileId;
    uint16_t            numOfProfiles;
    t_FmPcdKgKeyOrder   orderedArray;
    e_FmPcdEngine       nextEngine;
    e_FmPcdDoneAction   doneAction;
    uint8_t             pointedOwners;
    uint32_t            requiredAction;
    bool                extractedOrs;
    uint8_t             bitOffsetInPlcrProfile;
    bool                directPlcr;
} t_FmPcdKgScheme;

typedef struct {
    bool    allocated;
    uint8_t ownerId;    /* guestId for KG in multi-partition only,
                           portId for PLCR in any environment */
} t_FmPcdAllocMng;

typedef struct {
    t_FmPcdKgRegs                   *p_FmPcdKgRegs;
    uint32_t                        schemeExceptionsBitMask;
    uint8_t                         numOfSchemes;
    uint8_t                         schemesIds[FM_PCD_KG_NUM_OF_SCHEMES];
    t_FmPcdKgScheme                 schemes[FM_PCD_KG_NUM_OF_SCHEMES];
    t_FmPcdKgClsPlanGrp             clsPlanGrps[FM_MAX_NUM_OF_PORTS];
    uint8_t                         emptyClsPlanGrpId;
    t_FmPcdAllocMng                 schemesMng[FM_PCD_KG_NUM_OF_SCHEMES]; /* only for MASTER ! */
    t_FmPcdAllocMng                 clsPlanBlocksMng[FM_PCD_MAX_NUM_OF_CLS_PLANS/CLS_PLAN_NUM_PER_GRP];
} t_FmPcdKg;

typedef struct {
    uint16_t            profilesBase;
    uint16_t            numOfProfiles;
    t_Handle            h_FmPort;
} t_FmPcdPlcrMapParam;

typedef struct {
    bool                valid;
    volatile bool       lock;
    t_FmPcdAllocMng     profilesMng;
    uint8_t             pointedOwners;
    uint32_t            requiredAction;
    e_FmPcdEngine                       nextEngineOnGreen;          /**< Green next engine type */
    u_FmPcdPlcrNextEngineParams         paramsOnGreen;              /**< Green next engine params */

    e_FmPcdEngine                       nextEngineOnYellow;         /**< Yellow next engine type */
    u_FmPcdPlcrNextEngineParams         paramsOnYellow;             /**< Yellow next engine params */

    e_FmPcdEngine                       nextEngineOnRed;            /**< Red next engine type */
    u_FmPcdPlcrNextEngineParams         paramsOnRed;                /**< Red next engine params */
} t_FmPcdPlcrProfile;

typedef struct {
    t_FmPcdPlcrRegs                 *p_FmPcdPlcrRegs;
    t_FmPcdPlcrProfile              profiles[FM_PCD_PLCR_NUM_ENTRIES];
    uint16_t                        numOfSharedProfiles;
    uint16_t                        sharedProfilesIds[FM_PCD_PLCR_NUM_ENTRIES];
    t_FmPcdPlcrMapParam             portsMapping[FM_MAX_NUM_OF_PORTS];
} t_FmPcdPlcr;

typedef struct {
    uint32_t                        *p_SwPrsCode;
    uint32_t                        *p_CurrSwPrs;
    uint8_t                         currLabel;
    t_FmPcdPrsRegs                  *p_FmPcdPrsRegs;
    t_FmPcdPrsLabelParams           labelsTable[FM_PCD_PRS_NUM_OF_LABELS];
    uint32_t                        fmPcdPrsPortIdStatistics;
} t_FmPcdPrs;

typedef struct {
    struct {
        e_NetHeaderType             hdr;
        protocolOpt_t               opt; /* only one option !! */
    } hdrs[FM_PCD_MAX_NUM_OF_INTERCHANGEABLE_HDRS];
} t_FmPcdIntDistinctionUnit;

typedef struct {
    e_NetHeaderType             hdr;
    e_NetHeaderType             aliasHdr;
} t_FmPcdNetEnvAliases;

typedef struct {
    volatile bool               lock;
    bool                        used;
    uint8_t                     owners;
    uint8_t                     clsPlanGrpId;
    t_FmPcdIntDistinctionUnit   units[FM_PCD_MAX_NUM_OF_DISTINCTION_UNITS];
    uint32_t                    unitsVectors[FM_PCD_MAX_NUM_OF_DISTINCTION_UNITS];
    uint32_t                    lcvs[FM_PCD_PRS_NUM_OF_HDRS];
    uint32_t                    macsecVector;
    t_FmPcdNetEnvAliases        aliasHdrs[FM_PCD_MAX_NUM_OF_PRIVATE_HDRS];
} t_FmPcdNetEnv;

typedef struct {
    bool                        plcrAutoRefresh;

    uint16_t                    prsMaxParseCycleLimit;
} t_FmPcdDriverParam;

typedef struct {
    t_Handle                    h_Fm;
    t_Handle                    h_FmMuram;
    uint64_t                    physicalMuramBase;
    volatile bool               lock;
    t_Handle                    h_Spinlock;
    t_Handle                    h_IpcSession; /* relevant for guest only */
    bool                        enabled;
    uint8_t                     guestId;            /**< Guest Partition Id */
    uint8_t                     numOfEnabledGuestPartitionsPcds;
    char                        fmPcdModuleName[MODULE_NAME_SIZE];
    char                        fmPcdIpcHandlerModuleName[MODULE_NAME_SIZE]; /* relevant for guest only - this is the master's name */
    t_FmPcdNetEnv               netEnvs[FM_MAX_NUM_OF_PORTS];
    t_FmPcdKg                   *p_FmPcdKg;
    t_FmPcdPlcr                 *p_FmPcdPlcr;
    t_FmPcdPrs                  *p_FmPcdPrs;

    t_Handle                    h_Hc;

    uint32_t                    exceptions;
    t_FmPcdExceptionCallback    *f_Exception;
    t_FmPcdIdExceptionCallback  *f_FmPcdIndexedException;
    t_Handle                    h_App;

    t_FmPcdDriverParam          *p_FmPcdDriverParam;
} t_FmPcd;


/***********************************************************************/
/*  PCD internal routines                                              */
/***********************************************************************/

/**************************************************************************//**

 @Group         FM_PCD_InterModule_grp FM PCD Inter-Module Unit

 @Description   FM PCD Inter Module functions -
                These are not User API routines but routines that may be called
                from other modules. This will be the case in a single core environment,
                where instead of useing the XX messeging mechanism, the routines may be
                called from other modules. In a multicore environment, the other modules may
                be run by other cores and therefor these routines may not be called directly.

 @{
*//***************************************************************************/

t_Error     PcdGetVectorForOpt(t_FmPcd *p_FmPcd, uint8_t netEnvId, protocolOpt_t opt, uint32_t *p_Vector);
t_Error     PcdGetUnitsVector(t_FmPcd *p_FmPcd, t_NetEnvParams *p_Params);
bool        PcdNetEnvIsUnitWithoutOpts(t_FmPcd *p_FmPcd, uint8_t netEnvId, uint32_t unitVector);
t_Error     PcdGetClsPlanGrpParams(t_FmPcd *p_FmPcd, t_FmPcdKgInterModuleClsPlanGrpParams *p_GrpParams);
void        FmPcdSetClsPlanGrpId(t_FmPcd *p_FmPcd, uint8_t netEnvId, uint8_t clsPlanGrpId);
e_NetHeaderType FmPcdGetAliasHdr(t_FmPcd *p_FmPcd, uint8_t netEnvId, e_NetHeaderType hdr);
uint8_t     FmPcdNetEnvGetUnitIdForSingleHdr(t_Handle h_FmPcd, uint8_t netEnvId, e_NetHeaderType hdr);

t_Handle    KgConfig( t_FmPcd *p_FmPcd, t_FmPcdParams *p_FmPcdParams);
t_Error     KgInit(t_FmPcd *p_FmPcd);
t_Error     KgFree(t_FmPcd *p_FmPcd);
void        KgSetClsPlan(t_Handle h_FmPcd, t_FmPcdKgInterModuleClsPlanSet *p_Set);
bool        KgIsSchemeAlwaysDirect(t_Handle h_FmPcd, uint8_t schemeId);
void        KgEnable(t_FmPcd *p_FmPcd);
void        KgDisable(t_FmPcd *p_FmPcd);
t_Error     KgAllocClsPlanEntries(t_Handle h_FmPcd, uint16_t numOfClsPlanEntries, uint8_t guestId, uint8_t *p_First);
void        KgFreeClsPlanEntries(t_Handle h_FmPcd, uint16_t numOfClsPlanEntries, uint8_t guestId, uint8_t base);

/* only for MULTI partittion */
t_Error     FmPcdKgAllocSchemes(t_Handle h_FmPcd, uint8_t numOfSchemes, uint8_t guestId, uint8_t *p_SchemesIds);
t_Error     FmPcdKgFreeSchemes(t_Handle h_FmPcd, uint8_t numOfSchemes, uint8_t guestId, uint8_t *p_SchemesIds);
/* only for SINGLE partittion */
t_Error     KgBindPortToSchemes(t_Handle h_FmPcd , uint8_t hardwarePortId, uint32_t spReg);

t_Handle    PlcrConfig(t_FmPcd *p_FmPcd, t_FmPcdParams *p_FmPcdParams);
t_Error     PlcrInit(t_FmPcd *p_FmPcd);
t_Error     PlcrFree(t_FmPcd *p_FmPcd);
void        PlcrEnable(t_FmPcd *p_FmPcd);
void        PlcrDisable(t_FmPcd *p_FmPcd);
t_Error     PlcrFreeProfiles(t_FmPcd *p_FmPcd, uint8_t hardwarePortId, uint16_t num, uint16_t base);
t_Error     PlcrAllocProfiles(t_FmPcd *p_FmPcd, uint8_t hardwarePortId, uint16_t numOfProfiles, uint16_t *p_Base);
t_Error     PlcrAllocSharedProfiles(t_FmPcd *p_FmPcd, uint16_t numOfProfiles, uint16_t *profilesIds);
void        PlcrFreeSharedProfiles(t_FmPcd *p_FmPcd, uint16_t numOfProfiles, uint16_t *profilesIds);

t_Handle    PrsConfig(t_FmPcd *p_FmPcd,t_FmPcdParams *p_FmPcdParams);
t_Error     PrsInit(t_FmPcd *p_FmPcd);
void        PrsEnable(t_FmPcd *p_FmPcd);
void        PrsDisable(t_FmPcd *p_FmPcd);
void        PrsFree(t_FmPcd *p_FmPcd );
t_Error     PrsIncludePortInStatistics(t_FmPcd *p_FmPcd, uint8_t hardwarePortId, bool include);

t_Handle    FmPcdCcConfig(t_Handle h_FmPcd, t_FmPcdParams *p_FmPcdParams);
t_Error     FmPcdCcGetGrpParams(t_Handle treeId, uint8_t grpId, uint32_t *p_GrpBits, uint8_t *p_GrpBase);
uint8_t     FmPcdCcGetOffset(t_Handle h_CcNode);
uint8_t     FmPcdCcGetParseCode(t_Handle h_CcNode);
uint16_t    FmPcdCcGetNumOfKeys(t_Handle h_CcNode);

void        FmPcdManipUpdateOwner(t_Handle h_Manip, bool add);
t_Error     FmPcdManipCheckParamsForCcNextEgine(t_FmPcdCcNextEngineParams *p_InfoForManip, uint32_t *requiredAction);
void        FmPcdManipUpdateAdResultForCc(t_Handle h_Manip, t_Handle p_Ad, t_Handle *p_AdNew);
void        FmPcdManipUpdateAdContLookupForCc(t_Handle h_Manip, t_Handle p_Ad, t_Handle *p_AdNew, uint32_t adTableOffset);
uint32_t    FmPcdManipCheckNia(t_Handle h_FmPcd, t_Handle h_Ad);
void        FmPcdManipUpdateOwner(t_Handle h_Manip, bool add);
t_Error     FmPcdManipCheckParamsWithCcNodeParams(t_Handle h_Manip, t_Handle h_FmPcdCcNode);

static __inline__ t_Handle FmPcdGetMuramHandle(t_Handle h_FmPcd)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;
    ASSERT_COND(p_FmPcd);
    return p_FmPcd->h_FmMuram;
}

static __inline__ uint64_t FmPcdGetMuramPhysBase(t_Handle h_FmPcd)
{
    t_FmPcd     *p_FmPcd = (t_FmPcd*)h_FmPcd;
    ASSERT_COND(p_FmPcd);
    return p_FmPcd->physicalMuramBase;
}


#endif /* __FM_PCD_H */
