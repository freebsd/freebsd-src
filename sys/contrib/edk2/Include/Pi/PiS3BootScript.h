/** @file
  This file contains the boot script defintions that are shared between the
  Boot Script Executor PPI and the Boot Script Save Protocol.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _PI_S3_BOOT_SCRIPT_H_
#define _PI_S3_BOOT_SCRIPT_H_

// *******************************************
// EFI Boot Script Opcode definitions
// *******************************************
#define EFI_BOOT_SCRIPT_IO_WRITE_OPCODE                0x00
#define EFI_BOOT_SCRIPT_IO_READ_WRITE_OPCODE           0x01
#define EFI_BOOT_SCRIPT_MEM_WRITE_OPCODE               0x02
#define EFI_BOOT_SCRIPT_MEM_READ_WRITE_OPCODE          0x03
#define EFI_BOOT_SCRIPT_PCI_CONFIG_WRITE_OPCODE        0x04
#define EFI_BOOT_SCRIPT_PCI_CONFIG_READ_WRITE_OPCODE   0x05
#define EFI_BOOT_SCRIPT_SMBUS_EXECUTE_OPCODE           0x06
#define EFI_BOOT_SCRIPT_STALL_OPCODE                   0x07
#define EFI_BOOT_SCRIPT_DISPATCH_OPCODE                0x08
#define EFI_BOOT_SCRIPT_DISPATCH_2_OPCODE              0x09
#define EFI_BOOT_SCRIPT_INFORMATION_OPCODE             0x0A
#define EFI_BOOT_SCRIPT_PCI_CONFIG2_WRITE_OPCODE       0x0B
#define EFI_BOOT_SCRIPT_PCI_CONFIG2_READ_WRITE_OPCODE  0x0C
#define EFI_BOOT_SCRIPT_IO_POLL_OPCODE                 0x0D
#define EFI_BOOT_SCRIPT_MEM_POLL_OPCODE                0x0E
#define EFI_BOOT_SCRIPT_PCI_CONFIG_POLL_OPCODE         0x0F
#define EFI_BOOT_SCRIPT_PCI_CONFIG2_POLL_OPCODE        0x10

// *******************************************
// EFI_BOOT_SCRIPT_WIDTH
// *******************************************
typedef enum {
  EfiBootScriptWidthUint8,
  EfiBootScriptWidthUint16,
  EfiBootScriptWidthUint32,
  EfiBootScriptWidthUint64,
  EfiBootScriptWidthFifoUint8,
  EfiBootScriptWidthFifoUint16,
  EfiBootScriptWidthFifoUint32,
  EfiBootScriptWidthFifoUint64,
  EfiBootScriptWidthFillUint8,
  EfiBootScriptWidthFillUint16,
  EfiBootScriptWidthFillUint32,
  EfiBootScriptWidthFillUint64,
  EfiBootScriptWidthMaximum
} EFI_BOOT_SCRIPT_WIDTH;

#endif
