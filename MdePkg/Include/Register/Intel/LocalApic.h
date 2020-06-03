/** @file
  IA32 Local APIC Definitions.

  Copyright (c) 2010 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __INTEL_LOCAL_APIC_H__
#define __INTEL_LOCAL_APIC_H__

//
// Definition for Local APIC registers and related values
//
#define XAPIC_ID_OFFSET                         0x20
#define XAPIC_VERSION_OFFSET                    0x30
#define XAPIC_EOI_OFFSET                        0x0b0
#define XAPIC_ICR_DFR_OFFSET                    0x0e0
#define XAPIC_SPURIOUS_VECTOR_OFFSET            0x0f0
#define XAPIC_ICR_LOW_OFFSET                    0x300
#define XAPIC_ICR_HIGH_OFFSET                   0x310
#define XAPIC_LVT_TIMER_OFFSET                  0x320
#define XAPIC_LVT_LINT0_OFFSET                  0x350
#define XAPIC_LVT_LINT1_OFFSET                  0x360
#define XAPIC_TIMER_INIT_COUNT_OFFSET           0x380
#define XAPIC_TIMER_CURRENT_COUNT_OFFSET        0x390
#define XAPIC_TIMER_DIVIDE_CONFIGURATION_OFFSET 0x3E0

#define X2APIC_MSR_BASE_ADDRESS                 0x800
#define X2APIC_MSR_ICR_ADDRESS                  0x830

#define LOCAL_APIC_DELIVERY_MODE_FIXED           0
#define LOCAL_APIC_DELIVERY_MODE_LOWEST_PRIORITY 1
#define LOCAL_APIC_DELIVERY_MODE_SMI             2
#define LOCAL_APIC_DELIVERY_MODE_NMI             4
#define LOCAL_APIC_DELIVERY_MODE_INIT            5
#define LOCAL_APIC_DELIVERY_MODE_STARTUP         6
#define LOCAL_APIC_DELIVERY_MODE_EXTINT          7

#define LOCAL_APIC_DESTINATION_SHORTHAND_NO_SHORTHAND       0
#define LOCAL_APIC_DESTINATION_SHORTHAND_SELF               1
#define LOCAL_APIC_DESTINATION_SHORTHAND_ALL_INCLUDING_SELF 2
#define LOCAL_APIC_DESTINATION_SHORTHAND_ALL_EXCLUDING_SELF 3

//
// Local APIC Version Register.
//
typedef union {
  struct {
    UINT32  Version:8;                  ///< The version numbers of the local APIC.
    UINT32  Reserved0:8;                ///< Reserved.
    UINT32  MaxLvtEntry:8;              ///< Number of LVT entries minus 1.
    UINT32  EoiBroadcastSuppression:1;  ///< 1 if EOI-broadcast suppression supported.
    UINT32  Reserved1:7;                ///< Reserved.
  } Bits;
  UINT32    Uint32;
} LOCAL_APIC_VERSION;

//
// Low half of Interrupt Command Register (ICR).
//
typedef union {
  struct {
    UINT32  Vector:8;                ///< The vector number of the interrupt being sent.
    UINT32  DeliveryMode:3;          ///< Specifies the type of IPI to be sent.
    UINT32  DestinationMode:1;       ///< 0: physical destination mode, 1: logical destination mode.
    UINT32  DeliveryStatus:1;        ///< Indicates the IPI delivery status. This field is reserved in x2APIC mode.
    UINT32  Reserved0:1;             ///< Reserved.
    UINT32  Level:1;                 ///< 0 for the INIT level de-assert delivery mode. Otherwise 1.
    UINT32  TriggerMode:1;           ///< 0: edge, 1: level when using the INIT level de-assert delivery mode.
    UINT32  Reserved1:2;             ///< Reserved.
    UINT32  DestinationShorthand:2;  ///< A shorthand notation to specify the destination of the interrupt.
    UINT32  Reserved2:12;            ///< Reserved.
  } Bits;
  UINT32    Uint32;
} LOCAL_APIC_ICR_LOW;

//
// High half of Interrupt Command Register (ICR)
//
typedef union {
  struct {
    UINT32  Reserved0:24;   ///< Reserved.
    UINT32  Destination:8;  ///< Specifies the target processor or processors in xAPIC mode.
  } Bits;
  UINT32    Uint32;         ///< Destination field expanded to 32-bit in x2APIC mode.
} LOCAL_APIC_ICR_HIGH;

//
// Spurious-Interrupt Vector Register (SVR)
//
typedef union {
  struct {
    UINT32  SpuriousVector:8;           ///< Spurious Vector.
    UINT32  SoftwareEnable:1;           ///< APIC Software Enable/Disable.
    UINT32  FocusProcessorChecking:1;   ///< Focus Processor Checking.
    UINT32  Reserved0:2;                ///< Reserved.
    UINT32  EoiBroadcastSuppression:1;  ///< EOI-Broadcast Suppression.
    UINT32  Reserved1:19;               ///< Reserved.
  } Bits;
  UINT32    Uint32;
} LOCAL_APIC_SVR;

//
// Divide Configuration Register (DCR)
//
typedef union {
  struct {
    UINT32  DivideValue1:2;  ///< Low 2 bits of the divide value.
    UINT32  Reserved0:1;     ///< Always 0.
    UINT32  DivideValue2:1;  ///< Highest 1 bit of the divide value.
    UINT32  Reserved1:28;    ///< Reserved.
  } Bits;
  UINT32    Uint32;
} LOCAL_APIC_DCR;

//
// LVT Timer Register
//
typedef union {
  struct {
    UINT32  Vector:8;          ///< The vector number of the interrupt being sent.
    UINT32  Reserved0:4;       ///< Reserved.
    UINT32  DeliveryStatus:1;  ///< 0: Idle, 1: send pending.
    UINT32  Reserved1:3;       ///< Reserved.
    UINT32  Mask:1;            ///< 0: Not masked, 1: Masked.
    UINT32  TimerMode:1;       ///< 0: One-shot, 1: Periodic.
    UINT32  Reserved2:14;      ///< Reserved.
  } Bits;
  UINT32    Uint32;
} LOCAL_APIC_LVT_TIMER;

//
// LVT LINT0/LINT1 Register
//
typedef union {
  struct {
    UINT32  Vector:8;            ///< The vector number of the interrupt being sent.
    UINT32  DeliveryMode:3;      ///< Specifies the type of interrupt to be sent.
    UINT32  Reserved0:1;         ///< Reserved.
    UINT32  DeliveryStatus:1;    ///< 0: Idle, 1: send pending.
    UINT32  InputPinPolarity:1;  ///< Interrupt Input Pin Polarity.
    UINT32  RemoteIrr:1;         ///< RO. Set when the local APIC accepts the interrupt and reset when an EOI is received.
    UINT32  TriggerMode:1;       ///< 0:edge, 1:level.
    UINT32  Mask:1;              ///< 0: Not masked, 1: Masked.
    UINT32  Reserved1:15;        ///< Reserved.
  } Bits;
  UINT32    Uint32;
} LOCAL_APIC_LVT_LINT;

//
// MSI Address Register
//
typedef union {
  struct {
    UINT32  Reserved0:2;         ///< Reserved
    UINT32  DestinationMode:1;   ///< Specifies the Destination Mode.
    UINT32  RedirectionHint:1;   ///< Specifies the Redirection Hint.
    UINT32  Reserved1:8;         ///< Reserved.
    UINT32  DestinationId:8;     ///< Specifies the Destination ID.
    UINT32  BaseAddress:12;      ///< Must be 0FEEH
  } Bits;
  UINT32    Uint32;
} LOCAL_APIC_MSI_ADDRESS;

//
// MSI Address Register
//
typedef union {
  struct {
    UINT32  Vector:8;            ///< Interrupt vector in range 010h..0FEH
    UINT32  DeliveryMode:3;      ///< Specifies the type of interrupt to be sent.
    UINT32  Reserved0:3;         ///< Reserved.
    UINT32  Level:1;             ///< 0:Deassert, 1:Assert.  Ignored for Edge triggered interrupts.
    UINT32  TriggerMode:1;       ///< 0:Edge,     1:Level.
    UINT32  Reserved1:16;        ///< Reserved.
    UINT32  Reserved2:32;        ///< Reserved.
  } Bits;
  UINT64    Uint64;
} LOCAL_APIC_MSI_DATA;

#endif

