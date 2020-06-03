/** @file
  STM Resource Descriptor

  Copyright (c) 2015 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
  SMI Transfer Monitor (STM) User Guide Revision 1.00

**/

#ifndef _INTEL_STM_RESOURCE_DESCRIPTOR_H_
#define _INTEL_STM_RESOURCE_DESCRIPTOR_H_

#pragma pack (1)

/**
  STM Resource Descriptor Header
**/
typedef struct {
  UINT32  RscType;
  UINT16  Length;
  UINT16  ReturnStatus:1;
  UINT16  Reserved:14;
  UINT16  IgnoreResource:1;
} STM_RSC_DESC_HEADER;

/**
  Define values for the RscType field of #STM_RSC_DESC_HEADER
  @{
**/
#define END_OF_RESOURCES      0
#define MEM_RANGE             1
#define IO_RANGE              2
#define MMIO_RANGE            3
#define MACHINE_SPECIFIC_REG  4
#define PCI_CFG_RANGE         5
#define TRAPPED_IO_RANGE      6
#define ALL_RESOURCES         7
#define REGISTER_VIOLATION    8
#define MAX_DESC_TYPE         8
/// @}

/**
  STM Resource End Descriptor
**/
typedef struct {
  STM_RSC_DESC_HEADER  Hdr;
  UINT64               ResourceListContinuation;
} STM_RSC_END;

/**
  STM Resource Memory Descriptor
**/
typedef struct {
  STM_RSC_DESC_HEADER  Hdr;
  UINT64               Base;
  UINT64               Length;
  UINT32               RWXAttributes:3;
  UINT32               Reserved:29;
  UINT32               Reserved_2;
} STM_RSC_MEM_DESC;

/**
  Define values for the RWXAttributes field of #STM_RSC_MEM_DESC
  @{
**/
#define STM_RSC_MEM_R  0x1
#define STM_RSC_MEM_W  0x2
#define STM_RSC_MEM_X  0x4
/// @}

/**
  STM Resource I/O Descriptor
**/
typedef struct {
  STM_RSC_DESC_HEADER  Hdr;
  UINT16               Base;
  UINT16               Length;
  UINT32               Reserved;
} STM_RSC_IO_DESC;

/**
  STM Resource MMIO Descriptor
**/
typedef struct {
  STM_RSC_DESC_HEADER  Hdr;
  UINT64               Base;
  UINT64               Length;
  UINT32               RWXAttributes:3;
  UINT32               Reserved:29;
  UINT32               Reserved_2;
} STM_RSC_MMIO_DESC;

/**
  Define values for the RWXAttributes field of #STM_RSC_MMIO_DESC
  @{
**/
#define STM_RSC_MMIO_R  0x1
#define STM_RSC_MMIO_W  0x2
#define STM_RSC_MMIO_X  0x4
/// @}

/**
  STM Resource MSR Descriptor
**/
typedef struct {
  STM_RSC_DESC_HEADER  Hdr;
  UINT32               MsrIndex;
  UINT32               KernelModeProcessing:1;
  UINT32               Reserved:31;
  UINT64               ReadMask;
  UINT64               WriteMask;
} STM_RSC_MSR_DESC;

/**
  STM PCI Device Path node used for the PciDevicePath field of
  #STM_RSC_PCI_CFG_DESC
**/
typedef struct {
  ///
  /// Must be 1, indicating Hardware Device Path
  ///
  UINT8   Type;
  ///
  /// Must be 1, indicating PCI
  ///
  UINT8   Subtype;
  ///
  /// sizeof(STM_PCI_DEVICE_PATH_NODE) which is 6
  ///
  UINT16  Length;
  UINT8   PciFunction;
  UINT8   PciDevice;
} STM_PCI_DEVICE_PATH_NODE;

/**
  STM Resource PCI Configuration Descriptor
**/
typedef struct {
  STM_RSC_DESC_HEADER       Hdr;
  UINT16                    RWAttributes:2;
  UINT16                    Reserved:14;
  UINT16                    Base;
  UINT16                    Length;
  UINT8                     OriginatingBusNumber;
  UINT8                     LastNodeIndex;
  STM_PCI_DEVICE_PATH_NODE  PciDevicePath[1];
//STM_PCI_DEVICE_PATH_NODE  PciDevicePath[LastNodeIndex + 1];
} STM_RSC_PCI_CFG_DESC;

/**
  Define values for the RWAttributes field of #STM_RSC_PCI_CFG_DESC
  @{
**/
#define STM_RSC_PCI_CFG_R  0x1
#define STM_RSC_PCI_CFG_W  0x2
/// @}

/**
  STM Resource Trapped I/O Descriptor
**/
typedef struct {
  STM_RSC_DESC_HEADER  Hdr;
  UINT16               Base;
  UINT16               Length;
  UINT16               In:1;
  UINT16               Out:1;
  UINT16               Api:1;
  UINT16               Reserved1:13;
  UINT16               Reserved2;
} STM_RSC_TRAPPED_IO_DESC;

/**
  STM Resource All Descriptor
**/
typedef struct {
  STM_RSC_DESC_HEADER  Hdr;
} STM_RSC_ALL_RESOURCES_DESC;

/**
  STM Register Violation Descriptor
**/
typedef struct {
  STM_RSC_DESC_HEADER  Hdr;
  UINT32               RegisterType;
  UINT32               Reserved;
  UINT64               ReadMask;
  UINT64               WriteMask;
} STM_REGISTER_VIOLATION_DESC;

/**
  Enum values for the RWAttributes field of #STM_REGISTER_VIOLATION_DESC
**/
typedef enum {
  StmRegisterCr0,
  StmRegisterCr2,
  StmRegisterCr3,
  StmRegisterCr4,
  StmRegisterCr8,
  StmRegisterMax,
} STM_REGISTER_VIOLATION_TYPE;

/**
  Union of all STM resource types
**/
typedef union {
  STM_RSC_DESC_HEADER          Header;
  STM_RSC_END                  End;
  STM_RSC_MEM_DESC             Mem;
  STM_RSC_IO_DESC              Io;
  STM_RSC_MMIO_DESC            Mmio;
  STM_RSC_MSR_DESC             Msr;
  STM_RSC_PCI_CFG_DESC         PciCfg;
  STM_RSC_TRAPPED_IO_DESC      TrappedIo;
  STM_RSC_ALL_RESOURCES_DESC   All;
  STM_REGISTER_VIOLATION_DESC  RegisterViolation;
} STM_RSC;

#pragma pack ()

#endif
