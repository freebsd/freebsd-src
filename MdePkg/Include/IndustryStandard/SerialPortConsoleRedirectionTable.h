/** @file
  ACPI Serial Port Console Redirection Table as defined by Microsoft in
  http://www.microsoft.com/whdc/system/platform/server/spcr.mspx

  Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2015 Hewlett Packard Enterprise Development LP<BR>
  Copyright (c) 2014 - 2016, ARM Limited. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_H_
#define _SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_H_


#include <IndustryStandard/Acpi.h>

//
// Ensure proper structure formats
//
#pragma pack(1)

///
/// SPCR Revision (defined in spec)
///
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_REVISION 0x02

///
/// Serial Port Console Redirection Table Format
///
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER             Header;
  UINT8                                   InterfaceType;
  UINT8                                   Reserved1[3];
  EFI_ACPI_5_0_GENERIC_ADDRESS_STRUCTURE  BaseAddress;
  UINT8                                   InterruptType;
  UINT8                                   Irq;
  UINT32                                  GlobalSystemInterrupt;
  UINT8                                   BaudRate;
  UINT8                                   Parity;
  UINT8                                   StopBits;
  UINT8                                   FlowControl;
  UINT8                                   TerminalType;
  UINT8                                   Reserved2;
  UINT16                                  PciDeviceId;
  UINT16                                  PciVendorId;
  UINT8                                   PciBusNumber;
  UINT8                                   PciDeviceNumber;
  UINT8                                   PciFunctionNumber;
  UINT32                                  PciFlags;
  UINT8                                   PciSegment;
  UINT32                                  Reserved3;
} EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE;

#pragma pack()

//
// SPCR Definitions
//

//
// Interface Type
//

///
/// Full 16550 interface
///
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_INTERFACE_TYPE_16550                     0
///
/// Full 16450 interface
///
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_INTERFACE_TYPE_16450                     1


//
// The Serial Port Subtypes for ARM are documented in Table 3 of the DBG2 Specification
//

///
/// ARM PL011 UART
///
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_INTERFACE_TYPE_ARM_PL011_UART            0x03

///
/// ARM SBSA Generic UART (2.x) supporting 32-bit only accesses [deprecated]
///
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_INTERFACE_TYPE_ARM_SBSA_GENERIC_UART_2X  0x0d

///
/// ARM SBSA Generic UART
///
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_INTERFACE_TYPE_ARM_SBSA_GENERIC_UART     0x0e

///
/// ARM DCC
///
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_INTERFACE_TYPE_DCC                       0x0f

///
/// BCM2835 UART
///
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_INTERFACE_TYPE_BCM2835_UART              0x10

//
// Interrupt Type
//

///
/// PC-AT-compatible dual-8259 IRQ interrupt
///
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_INTERRUPT_TYPE_8259    0x1
///
/// I/O APIC interrupt (Global System Interrupt)
///
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_INTERRUPT_TYPE_APIC    0x2
///
/// I/O SAPIC interrupt (Global System Interrupt)
///
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_INTERRUPT_TYPE_SAPIC   0x4
///
/// ARMH GIC interrupt (Global System Interrupt)
///
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_INTERRUPT_TYPE_GIC     0x8

//
// Baud Rate
//
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_BAUD_RATE_9600         3
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_BAUD_RATE_19200        4
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_BAUD_RATE_57600        6
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_BAUD_RATE_115200       7

//
// Parity
//
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_PARITY_NO_PARITY       0

//
// Stop Bits
//
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_STOP_BITS_1            1

//
// Flow Control
//

///
/// DCD required for transmit
///
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_FLOW_CONTROL_DCD       0x1
///
/// RTS/CTS hardware flow control
///
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_FLOW_CONTROL_RTS_CTS   0x2
///
///  XON/XOFF software control
///
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_FLOW_CONTROL_XON_XOFF  0x4

//
// Terminal Type
//
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_TERMINAL_TYPE_VT100      0
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_TERMINAL_TYPE_VT100_PLUS 1
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_TERMINAL_TYPE_VT_UTF8    2
#define EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_TERMINAL_TYPE_ANSI       3

#endif
