//------------------------------------------------------------------------------
//
// CpuBreakpoint for RISC-V
//
// Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
//------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(RiscVCpuBreakpoint)
ASM_PFX(RiscVCpuBreakpoint):
  ebreak
  ret
