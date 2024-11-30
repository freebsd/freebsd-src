/** @file
  RISC-V CSR encodings

  Copyright (c) 2019, Western Digital Corporation or its affiliates. All rights reserved.<BR>
  Copyright (c) 2022, Ventana Micro Systems Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef RISCV_ENCODING_H_
#define RISCV_ENCODING_H_

#define MSTATUS_SIE         0x00000002UL
#define MSTATUS_MIE         0x00000008UL
#define MSTATUS_SPIE_SHIFT  5
#define MSTATUS_SPIE        (1UL << MSTATUS_SPIE_SHIFT)
#define MSTATUS_UBE         0x00000040UL
#define MSTATUS_MPIE        0x00000080UL
#define MSTATUS_SPP_SHIFT   8
#define MSTATUS_SPP         (1UL << MSTATUS_SPP_SHIFT)
#define MSTATUS_MPP_SHIFT   11
#define MSTATUS_MPP         (3UL << MSTATUS_MPP_SHIFT)
#define MSTATUS_FS          0x00006000UL

#define SSTATUS_SIE         MSTATUS_SIE
#define SSTATUS_SPIE_SHIFT  MSTATUS_SPIE_SHIFT
#define SSTATUS_SPIE        MSTATUS_SPIE
#define SSTATUS_SPP_SHIFT   MSTATUS_SPP_SHIFT
#define SSTATUS_SPP         MSTATUS_SPP

#define IRQ_S_SOFT    1
#define IRQ_VS_SOFT   2
#define IRQ_M_SOFT    3
#define IRQ_S_TIMER   5
#define IRQ_VS_TIMER  6
#define IRQ_M_TIMER   7
#define IRQ_S_EXT     9
#define IRQ_VS_EXT    10
#define IRQ_M_EXT     11
#define IRQ_S_GEXT    12
#define IRQ_PMU_OVF   13

#define MIP_SSIP    (1UL << IRQ_S_SOFT)
#define MIP_VSSIP   (1UL << IRQ_VS_SOFT)
#define MIP_MSIP    (1UL << IRQ_M_SOFT)
#define MIP_STIP    (1UL << IRQ_S_TIMER)
#define MIP_VSTIP   (1UL << IRQ_VS_TIMER)
#define MIP_MTIP    (1UL << IRQ_M_TIMER)
#define MIP_SEIP    (1UL << IRQ_S_EXT)
#define MIP_VSEIP   (1UL << IRQ_VS_EXT)
#define MIP_MEIP    (1UL << IRQ_M_EXT)
#define MIP_SGEIP   (1UL << IRQ_S_GEXT)
#define MIP_LCOFIP  (1UL << IRQ_PMU_OVF)

#define SIP_SSIP  MIP_SSIP
#define SIP_STIP  MIP_STIP

#define PRV_U  0UL
#define PRV_S  1UL
#define PRV_M  3UL

#define SATP64_MODE        0xF000000000000000ULL
#define SATP64_MODE_SHIFT  60
#define SATP64_ASID        0x0FFFF00000000000ULL
#define SATP64_PPN         0x00000FFFFFFFFFFFULL

#define SATP_MODE_OFF   0UL
#define SATP_MODE_SV32  1UL
#define SATP_MODE_SV39  8UL
#define SATP_MODE_SV48  9UL
#define SATP_MODE_SV57  10UL
#define SATP_MODE_SV64  11UL

#define SATP_MODE  SATP64_MODE

/* User Counters/Timers */
#define CSR_CYCLE  0xc00
#define CSR_TIME   0xc01

/* Floating-Point */
#define CSR_FCSR  0x003

/* Supervisor Trap Setup */
#define CSR_SSTATUS  0x100
#define CSR_SEDELEG  0x102
#define CSR_SIDELEG  0x103
#define CSR_SIE      0x104
#define CSR_STVEC    0x105

/* Supervisor Configuration */
#define CSR_SENVCFG  0x10a

/* Supervisor Trap Handling */
#define CSR_SSCRATCH  0x140
#define CSR_SEPC      0x141
#define CSR_SCAUSE    0x142
#define CSR_STVAL     0x143
#define CSR_SIP       0x144

/* Supervisor Protection and Translation */
#define CSR_SATP  0x180

/* Sstc extension */
#define CSR_STIMECMP  0x14D

/* Trap/Exception Causes */
#define CAUSE_MISALIGNED_FETCH          0x0
#define CAUSE_FETCH_ACCESS              0x1
#define CAUSE_ILLEGAL_INSTRUCTION       0x2
#define CAUSE_BREAKPOINT                0x3
#define CAUSE_MISALIGNED_LOAD           0x4
#define CAUSE_LOAD_ACCESS               0x5
#define CAUSE_MISALIGNED_STORE          0x6
#define CAUSE_STORE_ACCESS              0x7
#define CAUSE_USER_ECALL                0x8
#define CAUSE_SUPERVISOR_ECALL          0x9
#define CAUSE_VIRTUAL_SUPERVISOR_ECALL  0xa
#define CAUSE_MACHINE_ECALL             0xb
#define CAUSE_FETCH_PAGE_FAULT          0xc
#define CAUSE_LOAD_PAGE_FAULT           0xd
#define CAUSE_STORE_PAGE_FAULT          0xf
#define CAUSE_FETCH_GUEST_PAGE_FAULT    0x14
#define CAUSE_LOAD_GUEST_PAGE_FAULT     0x15
#define CAUSE_VIRTUAL_INST_FAULT        0x16
#define CAUSE_STORE_GUEST_PAGE_FAULT    0x17

/* Sstc extension */
#define CSR_SEED  0x15

#define SEED_OPST_MASK     0xc0000000
#define SEED_OPST_BIST     0x00000000
#define SEED_OPST_WAIT     0x40000000
#define SEED_OPST_ES16     0x80000000
#define SEED_OPST_DEAD     0xc0000000
#define SEED_ENTROPY_MASK  0xffff

#endif
