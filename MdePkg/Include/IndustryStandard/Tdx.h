/** @file
  Intel Trust Domain Extension definitions
  Detailed information is in below document:
  https://software.intel.com/content/dam/develop/external/us/en/documents
  /tdx-module-1eas-v0.85.039.pdf

  Copyright (c) 2020 - 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef MDE_PKG_TDX_H_
#define MDE_PKG_TDX_H_

#define EXIT_REASON_EXTERNAL_INTERRUPT  1
#define EXIT_REASON_TRIPLE_FAULT        2

#define EXIT_REASON_PENDING_INTERRUPT    7
#define EXIT_REASON_NMI_WINDOW           8
#define EXIT_REASON_TASK_SWITCH          9
#define EXIT_REASON_CPUID                10
#define EXIT_REASON_HLT                  12
#define EXIT_REASON_INVD                 13
#define EXIT_REASON_INVLPG               14
#define EXIT_REASON_RDPMC                15
#define EXIT_REASON_RDTSC                16
#define EXIT_REASON_VMCALL               18
#define EXIT_REASON_VMCLEAR              19
#define EXIT_REASON_VMLAUNCH             20
#define EXIT_REASON_VMPTRLD              21
#define EXIT_REASON_VMPTRST              22
#define EXIT_REASON_VMREAD               23
#define EXIT_REASON_VMRESUME             24
#define EXIT_REASON_VMWRITE              25
#define EXIT_REASON_VMOFF                26
#define EXIT_REASON_VMON                 27
#define EXIT_REASON_CR_ACCESS            28
#define EXIT_REASON_DR_ACCESS            29
#define EXIT_REASON_IO_INSTRUCTION       30
#define EXIT_REASON_MSR_READ             31
#define EXIT_REASON_MSR_WRITE            32
#define EXIT_REASON_INVALID_STATE        33
#define EXIT_REASON_MSR_LOAD_FAIL        34
#define EXIT_REASON_MWAIT_INSTRUCTION    36
#define EXIT_REASON_MONITOR_TRAP_FLAG    37
#define EXIT_REASON_MONITOR_INSTRUCTION  39
#define EXIT_REASON_PAUSE_INSTRUCTION    40
#define EXIT_REASON_MCE_DURING_VMENTRY   41
#define EXIT_REASON_TPR_BELOW_THRESHOLD  43
#define EXIT_REASON_APIC_ACCESS          44
#define EXIT_REASON_EOI_INDUCED          45
#define EXIT_REASON_GDTR_IDTR            46
#define EXIT_REASON_LDTR_TR              47
#define EXIT_REASON_EPT_VIOLATION        48
#define EXIT_REASON_EPT_MISCONFIG        49
#define EXIT_REASON_INVEPT               50
#define EXIT_REASON_RDTSCP               51
#define EXIT_REASON_PREEMPTION_TIMER     52
#define EXIT_REASON_INVVPID              53
#define EXIT_REASON_WBINVD               54
#define EXIT_REASON_XSETBV               55
#define EXIT_REASON_APIC_WRITE           56
#define EXIT_REASON_RDRAND               57
#define EXIT_REASON_INVPCID              58
#define EXIT_REASON_VMFUNC               59
#define EXIT_REASON_ENCLS                60
#define EXIT_REASON_RDSEED               61
#define EXIT_REASON_PML_FULL             62
#define EXIT_REASON_XSAVES               63
#define EXIT_REASON_XRSTORS              64

// TDCALL API Function Completion Status Codes
#define TDX_EXIT_REASON_SUCCESS                0x0000000000000000
#define TDX_EXIT_REASON_PAGE_ALREADY_ACCEPTED  0x00000B0A00000000
#define TDX_EXIT_REASON_PAGE_SIZE_MISMATCH     0xC0000B0B00000000
#define TDX_EXIT_REASON_OPERAND_INVALID        0xC000010000000000
#define TDX_EXIT_REASON_OPERAND_BUSY           0x8000020000000000

// TDCALL [TDG.MEM.PAGE.ACCEPT] page size
#define TDCALL_ACCEPT_PAGE_SIZE_4K  0
#define TDCALL_ACCEPT_PAGE_SIZE_2M  1
#define TDCALL_ACCEPT_PAGE_SIZE_1G  2

#define TDCALL_TDVMCALL      0
#define TDCALL_TDINFO        1
#define TDCALL_TDEXTENDRTMR  2
#define TDCALL_TDGETVEINFO   3
#define TDCALL_TDREPORT      4
#define TDCALL_TDSETCPUIDVE  5
#define TDCALL_TDACCEPTPAGE  6

#define TDVMCALL_CPUID    0x0000a
#define TDVMCALL_HALT     0x0000c
#define TDVMCALL_IO       0x0001e
#define TDVMCALL_RDMSR    0x0001f
#define TDVMCALL_WRMSR    0x00020
#define TDVMCALL_MMIO     0x00030
#define TDVMCALL_PCONFIG  0x00041

#define TDVMCALL_GET_TDVMCALL_INFO   0x10000
#define TDVMCALL_MAPGPA              0x10001
#define TDVMCALL_GET_QUOTE           0x10002
#define TDVMCALL_REPORT_FATAL_ERR    0x10003
#define TDVMCALL_SETUP_EVENT_NOTIFY  0x10004

#define TDVMCALL_STATUS_RETRY  0x1

#pragma pack(1)
typedef struct {
  UINT64    Data[6];
} TDCALL_GENERIC_RETURN_DATA;

typedef struct {
  UINT64    Gpaw;
  UINT64    Attributes;
  UINT32    NumVcpus;
  UINT32    MaxVcpus;
  UINT64    Resv[3];
} TDCALL_INFO_RETURN_DATA;

typedef union {
  UINT64    Val;
  struct {
    UINT32    Size      : 3;
    UINT32    Direction : 1;
    UINT32    String    : 1;
    UINT32    Rep       : 1;
    UINT32    Encoding  : 1;
    UINT32    Resv      : 9;
    UINT32    Port      : 16;
    UINT32    Resv2;
  } Io;
} VMX_EXIT_QUALIFICATION;

typedef struct {
  UINT32                    ExitReason;
  UINT32                    Resv;
  VMX_EXIT_QUALIFICATION    ExitQualification;
  UINT64                    GuestLA;
  UINT64                    GuestPA;
  UINT32                    ExitInstructionLength;
  UINT32                    ExitInstructionInfo;
  UINT32                    Resv1;
} TDCALL_VEINFO_RETURN_DATA;

typedef union {
  TDCALL_GENERIC_RETURN_DATA    Generic;
  TDCALL_INFO_RETURN_DATA       TdInfo;
  TDCALL_VEINFO_RETURN_DATA     VeInfo;
} TD_RETURN_DATA;

/* data structure used in TDREPORT_STRUCT */
typedef struct {
  UINT8    Type;
  UINT8    Subtype;
  UINT8    Version;
  UINT8    Rsvd;
} TD_REPORT_TYPE;

typedef struct {
  TD_REPORT_TYPE    ReportType;
  UINT8             Rsvd1[12];
  UINT8             CpuSvn[16];
  UINT8             TeeTcbInfoHash[48];
  UINT8             TeeInfoHash[48];
  UINT8             ReportData[64];
  UINT8             Rsvd2[32];
  UINT8             Mac[32];
} REPORTMACSTRUCT;

typedef struct {
  UINT8    Seam[2];
  UINT8    Rsvd[14];
} TEE_TCB_SVN;

typedef struct {
  UINT8          Valid[8];
  TEE_TCB_SVN    TeeTcbSvn;
  UINT8          Mrseam[48];
  UINT8          Mrsignerseam[48];
  UINT8          Attributes[8];
  UINT8          Rsvd[111];
} TEE_TCB_INFO;

typedef struct {
  UINT8    Attributes[8];
  UINT8    Xfam[8];
  UINT8    Mrtd[48];
  UINT8    Mrconfigid[48];
  UINT8    Mrowner[48];
  UINT8    Mrownerconfig[48];
  UINT8    Rtmrs[4][48];
  UINT8    Rsvd[112];
} TDINFO;

typedef struct {
  REPORTMACSTRUCT    ReportMacStruct;
  TEE_TCB_INFO       TeeTcbInfo;
  UINT8              Rsvd[17];
  TDINFO             Tdinfo;
} TDREPORT_STRUCT;

#pragma pack()

#endif
