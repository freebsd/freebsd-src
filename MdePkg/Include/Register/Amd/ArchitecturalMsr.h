/** @file ArchitecturalMsr.h
  AMD Architectural MSR Definitions.

  Provides defines for Machine Specific Registers(MSR) indexes.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
  AMD64 Architecture Programmer’s Manual, Volumes 2
  Rev. 3.37, Volume 2: System Programming

**/

#ifndef AMD_ARCHITECTURAL_MSR_H_
#define AMD_ARCHITECTURAL_MSR_H_

/*
  See Appendix A.8, "System Management Mode MSR Cross-Reference".

  SMBASE MSR that contains the SMRAM base address.
  Reset value: 0000_0000_0003_0000h

*/
#define AMD_64_SMM_BASE  0xC0010111

/*
  See Appendix A.8, "System Management Mode MSR Cross-Reference".

  SMM_ADDR Contains the base address of protected
  memory for the SMM Handler.

  Specific usage, see AMD64 Architecture Programmer’s Manual,
  Volumes 2 (Rev. 3.37), Section 10.2.5

  Reset value: 0000_0000_0000_0000h

*/
#define AMD_64_SMM_ADDR  0xC0010112

/*
  See Appendix A.8, "System Management Mode MSR Cross-Reference".

  SMM_MASK Contains a mask which determines the size of
  the protected area for the SMM handler.

  Specific usage, see AMD64 Architecture Programmer’s Manual,
  Volumes 2 (Rev. 3.37), Section 10.2.5

  Reset value: 0000_0000_0000_0000h

*/
#define AMD_64_SMM_MASK  0xC0010113

#endif // AMD_ARCHITECTURAL_MSR_H_
