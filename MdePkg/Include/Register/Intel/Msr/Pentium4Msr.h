/** @file
  MSR Definitions for Pentium(R) 4 Processors.

  Provides defines for Machine Specific Registers(MSR) indexes. Data structures
  are provided for MSRs that contain one or more bit fields.  If the MSR value
  returned is a single 32-bit or 64-bit value, then a data structure is not
  provided for that MSR.

  Copyright (c) 2016 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
  Intel(R) 64 and IA-32 Architectures Software Developer's Manual, Volume 4,
  May 2018, Volume 4: Model-Specific-Registers (MSR)

**/

#ifndef __PENTIUM_4_MSR_H__
#define __PENTIUM_4_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>

/**
  Is Pentium(R) 4 Processors?

  @param   DisplayFamily  Display Family ID
  @param   DisplayModel   Display Model ID

  @retval  TRUE   Yes, it is.
  @retval  FALSE  No, it isn't.
**/
#define IS_PENTIUM_4_PROCESSOR(DisplayFamily, DisplayModel) \
  (DisplayFamily == 0x0F \
   )

/**
  3, 4, 6. Shared. See Section 8.10.5, "Monitor/Mwait Address Range
  Determination.".

  @param  ECX  MSR_PENTIUM_4_IA32_MONITOR_FILTER_LINE_SIZE (0x00000006)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_IA32_MONITOR_FILTER_LINE_SIZE);
  AsmWriteMsr64 (MSR_PENTIUM_4_IA32_MONITOR_FILTER_LINE_SIZE, Msr);
  @endcode
  @note MSR_PENTIUM_4_IA32_MONITOR_FILTER_LINE_SIZE is defined as IA32_MONITOR_FILTER_LINE_SIZE in SDM.
**/
#define MSR_PENTIUM_4_IA32_MONITOR_FILTER_LINE_SIZE  0x00000006

/**
  0, 1, 2, 3, 4, 6. Shared. Processor Hard Power-On Configuration (R/W)
  Enables and disables processor features; (R) indicates current processor
  configuration.

  @param  ECX  MSR_PENTIUM_4_EBC_HARD_POWERON (0x0000002A)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_EBC_HARD_POWERON_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_EBC_HARD_POWERON_REGISTER.

  <b>Example usage</b>
  @code
  MSR_PENTIUM_4_EBC_HARD_POWERON_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_PENTIUM_4_EBC_HARD_POWERON);
  AsmWriteMsr64 (MSR_PENTIUM_4_EBC_HARD_POWERON, Msr.Uint64);
  @endcode
  @note MSR_PENTIUM_4_EBC_HARD_POWERON is defined as MSR_EBC_HARD_POWERON in SDM.
**/
#define MSR_PENTIUM_4_EBC_HARD_POWERON  0x0000002A

/**
  MSR information returned for MSR index #MSR_PENTIUM_4_EBC_HARD_POWERON
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Output Tri-state Enabled (R) Indicates whether tri-state
    /// output is enabled (1) or disabled (0) as set by the strapping of SMI#.
    /// The value in this bit is written on the deassertion of RESET#; the bit
    /// is set to 1 when the address bus signal is asserted.
    ///
    UINT32    OutputTriStateEnabled     : 1;
    ///
    /// [Bit 1] Execute BIST (R)  Indicates whether the execution of the BIST
    /// is enabled (1) or disabled (0) as set by the strapping of INIT#. The
    /// value in this bit is written on the deassertion of RESET#; the bit is
    /// set to 1 when the address bus signal is asserted.
    ///
    UINT32    ExecuteBIST               : 1;
    ///
    /// [Bit 2] In Order Queue Depth (R) Indicates whether the in order queue
    /// depth for the system bus is 1 (1) or up to 12 (0) as set by the
    /// strapping of A7#. The value in this bit is written on the deassertion
    /// of RESET#; the bit is set to 1 when the address bus signal is asserted.
    ///
    UINT32    InOrderQueueDepth         : 1;
    ///
    /// [Bit 3] MCERR# Observation Disabled (R) Indicates whether MCERR#
    /// observation is enabled (0) or disabled (1) as determined by the
    /// strapping of A9#. The value in this bit is written on the deassertion
    /// of RESET#; the bit is set to 1 when the address bus signal is asserted.
    ///
    UINT32    MCERR_ObservationDisabled : 1;
    ///
    /// [Bit 4] BINIT# Observation Enabled (R) Indicates whether BINIT#
    /// observation is enabled (0) or disabled (1) as determined by the
    /// strapping of A10#. The value in this bit is written on the deassertion
    /// of RESET#; the bit is set to 1 when the address bus signal is asserted.
    ///
    UINT32    BINIT_ObservationEnabled  : 1;
    ///
    /// [Bits 6:5] APIC Cluster ID (R)  Contains the logical APIC cluster ID
    /// value as set by the strapping of A12# and A11#. The logical cluster ID
    /// value is written into the field on the deassertion of RESET#; the
    /// field is set to 1 when the address bus signal is asserted.
    ///
    UINT32    APICClusterID             : 2;
    ///
    /// [Bit 7] Bus Park Disable (R)  Indicates whether bus park is enabled
    /// (0) or disabled (1) as set by the strapping of A15#. The value in this
    /// bit is written on the deassertion of RESET#; the bit is set to 1 when
    /// the address bus signal is asserted.
    ///
    UINT32    BusParkDisable            : 1;
    UINT32    Reserved1                 : 4;
    ///
    /// [Bits 13:12] Agent ID (R)  Contains the logical agent ID value as set
    /// by the strapping of BR[3:0]. The logical ID value is written into the
    /// field on the deassertion of RESET#; the field is set to 1 when the
    /// address bus signal is asserted.
    ///
    UINT32    AgentID                   : 2;
    UINT32    Reserved2                 : 18;
    UINT32    Reserved3                 : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_PENTIUM_4_EBC_HARD_POWERON_REGISTER;

/**
  0, 1, 2, 3, 4, 6. Shared. Processor Soft Power-On Configuration (R/W)
  Enables and disables processor features.

  @param  ECX  MSR_PENTIUM_4_EBC_SOFT_POWERON (0x0000002B)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_EBC_SOFT_POWERON_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_EBC_SOFT_POWERON_REGISTER.

  <b>Example usage</b>
  @code
  MSR_PENTIUM_4_EBC_SOFT_POWERON_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_PENTIUM_4_EBC_SOFT_POWERON);
  AsmWriteMsr64 (MSR_PENTIUM_4_EBC_SOFT_POWERON, Msr.Uint64);
  @endcode
  @note MSR_PENTIUM_4_EBC_SOFT_POWERON is defined as MSR_EBC_SOFT_POWERON in SDM.
**/
#define MSR_PENTIUM_4_EBC_SOFT_POWERON  0x0000002B

/**
  MSR information returned for MSR index #MSR_PENTIUM_4_EBC_SOFT_POWERON
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] RCNT/SCNT On Request Encoding Enable (R/W)  Controls the
    /// driving of RCNT/SCNT on the request encoding. Set to enable (1); clear
    /// to disabled (0, default).
    ///
    UINT32    RCNT_SCNT                          : 1;
    ///
    /// [Bit 1] Data Error Checking Disable (R/W)  Set to disable system data
    /// bus parity checking; clear to enable parity checking.
    ///
    UINT32    DataErrorCheckingDisable           : 1;
    ///
    /// [Bit 2] Response Error Checking Disable (R/W) Set to disable
    /// (default); clear to enable.
    ///
    UINT32    ResponseErrorCheckingDisable       : 1;
    ///
    /// [Bit 3] Address/Request Error Checking Disable (R/W) Set to disable
    /// (default); clear to enable.
    ///
    UINT32    AddressRequestErrorCheckingDisable : 1;
    ///
    /// [Bit 4] Initiator MCERR# Disable (R/W) Set to disable MCERR# driving
    /// for initiator bus requests (default); clear to enable.
    ///
    UINT32    InitiatorMCERR_Disable             : 1;
    ///
    /// [Bit 5] Internal MCERR# Disable (R/W) Set to disable MCERR# driving
    /// for initiator internal errors (default); clear to enable.
    ///
    UINT32    InternalMCERR_Disable              : 1;
    ///
    /// [Bit 6] BINIT# Driver Disable (R/W)  Set to disable BINIT# driver
    /// (default); clear to enable driver.
    ///
    UINT32    BINIT_DriverDisable                : 1;
    UINT32    Reserved1                          : 25;
    UINT32    Reserved2                          : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_PENTIUM_4_EBC_SOFT_POWERON_REGISTER;

/**
  2,3, 4, 6. Shared. Processor Frequency Configuration The bit field layout of
  this MSR varies according to the MODEL value in the CPUID version
  information. The following bit field layout applies to Pentium 4 and Xeon
  Processors with MODEL encoding equal or greater than 2. (R) The field
  Indicates the current processor frequency configuration.

  @param  ECX  MSR_PENTIUM_4_EBC_FREQUENCY_ID (0x0000002C)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_EBC_FREQUENCY_ID_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_EBC_FREQUENCY_ID_REGISTER.

  <b>Example usage</b>
  @code
  MSR_PENTIUM_4_EBC_FREQUENCY_ID_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_PENTIUM_4_EBC_FREQUENCY_ID);
  @endcode
  @note MSR_PENTIUM_4_EBC_FREQUENCY_ID is defined as MSR_EBC_FREQUENCY_ID in SDM.
**/
#define MSR_PENTIUM_4_EBC_FREQUENCY_ID  0x0000002C

/**
  MSR information returned for MSR index #MSR_PENTIUM_4_EBC_FREQUENCY_ID
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1 : 16;
    ///
    /// [Bits 18:16] Scalable Bus Speed (R/W) Indicates the intended scalable
    /// bus speed: *EncodingScalable Bus Speed*
    ///
    ///   000B 100 MHz (Model 2).
    ///   000B 266 MHz (Model 3 or 4)
    ///   001B 133 MHz
    ///   010B 200 MHz
    ///   011B 166 MHz
    ///   100B 333 MHz (Model 6)
    ///
    ///   133.33 MHz should be utilized if performing calculation with System
    ///   Bus Speed when encoding is 001B. 166.67 MHz should be utilized if
    ///   performing calculation with System Bus Speed when encoding is 011B.
    ///   266.67 MHz should be utilized if performing calculation with System
    ///   Bus Speed when encoding is 000B and model encoding = 3 or 4. 333.33
    ///   MHz should be utilized if performing calculation with System Bus
    ///   Speed when encoding is 100B and model encoding = 6. All other values
    ///   are reserved.
    ///
    UINT32    ScalableBusSpeed : 3;
    UINT32    Reserved2        : 5;
    ///
    /// [Bits 31:24] Core Clock Frequency to System Bus  Frequency Ratio (R)
    /// The processor core clock frequency to system bus frequency ratio
    /// observed at the de-assertion of the reset pin.
    ///
    UINT32    ClockRatio       : 8;
    UINT32    Reserved3        : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_PENTIUM_4_EBC_FREQUENCY_ID_REGISTER;

/**
  0, 1. Shared. Processor Frequency Configuration (R)  The bit field layout of
  this MSR varies according to the MODEL value of the CPUID version
  information. This bit field layout applies to Pentium 4 and Xeon Processors
  with MODEL encoding less than 2. Indicates current processor frequency
  configuration.

  @param  ECX  MSR_PENTIUM_4_EBC_FREQUENCY_ID_1 (0x0000002C)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_EBC_FREQUENCY_ID_1_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_EBC_FREQUENCY_ID_1_REGISTER.

  <b>Example usage</b>
  @code
  MSR_PENTIUM_4_EBC_FREQUENCY_ID_1_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_PENTIUM_4_EBC_FREQUENCY_ID_1);
  @endcode
  @note MSR_PENTIUM_4_EBC_FREQUENCY_ID_1 is defined as MSR_EBC_FREQUENCY_ID_1 in SDM.
**/
#define MSR_PENTIUM_4_EBC_FREQUENCY_ID_1  0x0000002C

/**
  MSR information returned for MSR index #MSR_PENTIUM_4_EBC_FREQUENCY_ID_1
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1        : 21;
    ///
    /// [Bits 23:21] Scalable Bus Speed (R/W) Indicates the intended scalable
    /// bus speed: *Encoding* *Scalable Bus Speed*
    ///
    ///   000B 100 MHz All others values reserved.
    ///
    UINT32    ScalableBusSpeed : 3;
    UINT32    Reserved2        : 8;
    UINT32    Reserved3        : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_PENTIUM_4_EBC_FREQUENCY_ID_1_REGISTER;

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check EAX/RAX Save State See Section
  15.3.2.6, "IA32_MCG Extended Machine Check State MSRs.". Contains register
  state at time of machine check error. When in non-64-bit modes at the time
  of the error, bits 63-32 do not contain valid data.

  @param  ECX  MSR_PENTIUM_4_MCG_RAX (0x00000180)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_RAX);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_RAX, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_RAX is defined as MSR_MCG_RAX in SDM.
**/
#define MSR_PENTIUM_4_MCG_RAX  0x00000180

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check EBX/RBX Save State See Section
  15.3.2.6, "IA32_MCG Extended Machine Check State MSRs.". Contains register
  state at time of machine check error. When in non-64-bit modes at the time
  of the error, bits 63-32 do not contain valid data.

  @param  ECX  MSR_PENTIUM_4_MCG_RBX (0x00000181)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_RBX);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_RBX, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_RBX is defined as MSR_MCG_RBX in SDM.
**/
#define MSR_PENTIUM_4_MCG_RBX  0x00000181

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check ECX/RCX Save State See Section
  15.3.2.6, "IA32_MCG Extended Machine Check State MSRs.". Contains register
  state at time of machine check error. When in non-64-bit modes at the time
  of the error, bits 63-32 do not contain valid data.

  @param  ECX  MSR_PENTIUM_4_MCG_RCX (0x00000182)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_RCX);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_RCX, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_RCX is defined as MSR_MCG_RCX in SDM.
**/
#define MSR_PENTIUM_4_MCG_RCX  0x00000182

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check EDX/RDX Save State See Section
  15.3.2.6, "IA32_MCG Extended Machine Check State MSRs.". Contains register
  state at time of machine check error. When in non-64-bit modes at the time
  of the error, bits 63-32 do not contain valid data.

  @param  ECX  MSR_PENTIUM_4_MCG_RDX (0x00000183)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_RDX);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_RDX, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_RDX is defined as MSR_MCG_RDX in SDM.
**/
#define MSR_PENTIUM_4_MCG_RDX  0x00000183

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check ESI/RSI Save State See Section
  15.3.2.6, "IA32_MCG Extended Machine Check State MSRs.". Contains register
  state at time of machine check error. When in non-64-bit modes at the time
  of the error, bits 63-32 do not contain valid data.

  @param  ECX  MSR_PENTIUM_4_MCG_RSI (0x00000184)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_RSI);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_RSI, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_RSI is defined as MSR_MCG_RSI in SDM.
**/
#define MSR_PENTIUM_4_MCG_RSI  0x00000184

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check EDI/RDI Save State See Section
  15.3.2.6, "IA32_MCG Extended Machine Check State MSRs.". Contains register
  state at time of machine check error. When in non-64-bit modes at the time
  of the error, bits 63-32 do not contain valid data.

  @param  ECX  MSR_PENTIUM_4_MCG_RDI (0x00000185)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_RDI);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_RDI, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_RDI is defined as MSR_MCG_RDI in SDM.
**/
#define MSR_PENTIUM_4_MCG_RDI  0x00000185

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check EBP/RBP Save State See Section
  15.3.2.6, "IA32_MCG Extended Machine Check State MSRs.". Contains register
  state at time of machine check error. When in non-64-bit modes at the time
  of the error, bits 63-32 do not contain valid data.

  @param  ECX  MSR_PENTIUM_4_MCG_RBP (0x00000186)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_RBP);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_RBP, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_RBP is defined as MSR_MCG_RBP in SDM.
**/
#define MSR_PENTIUM_4_MCG_RBP  0x00000186

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check ESP/RSP Save State See Section
  15.3.2.6, "IA32_MCG Extended Machine Check State MSRs.". Contains register
  state at time of machine check error. When in non-64-bit modes at the time
  of the error, bits 63-32 do not contain valid data.

  @param  ECX  MSR_PENTIUM_4_MCG_RSP (0x00000187)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_RSP);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_RSP, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_RSP is defined as MSR_MCG_RSP in SDM.
**/
#define MSR_PENTIUM_4_MCG_RSP  0x00000187

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check EFLAGS/RFLAG Save State See Section
  15.3.2.6, "IA32_MCG Extended Machine Check State MSRs.". Contains register
  state at time of machine check error. When in non-64-bit modes at the time
  of the error, bits 63-32 do not contain valid data.

  @param  ECX  MSR_PENTIUM_4_MCG_RFLAGS (0x00000188)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_RFLAGS);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_RFLAGS, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_RFLAGS is defined as MSR_MCG_RFLAGS in SDM.
**/
#define MSR_PENTIUM_4_MCG_RFLAGS  0x00000188

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check EIP/RIP Save State See Section
  15.3.2.6, "IA32_MCG Extended Machine Check State MSRs.". Contains register
  state at time of machine check error. When in non-64-bit modes at the time
  of the error, bits 63-32 do not contain valid data.

  @param  ECX  MSR_PENTIUM_4_MCG_RIP (0x00000189)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_RIP);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_RIP, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_RIP is defined as MSR_MCG_RIP in SDM.
**/
#define MSR_PENTIUM_4_MCG_RIP  0x00000189

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check Miscellaneous See Section 15.3.2.6,
  "IA32_MCG Extended Machine Check State MSRs.".

  @param  ECX  MSR_PENTIUM_4_MCG_MISC (0x0000018A)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_MCG_MISC_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_MCG_MISC_REGISTER.

  <b>Example usage</b>
  @code
  MSR_PENTIUM_4_MCG_MISC_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_PENTIUM_4_MCG_MISC);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_MISC, Msr.Uint64);
  @endcode
  @note MSR_PENTIUM_4_MCG_MISC is defined as MSR_MCG_MISC in SDM.
**/
#define MSR_PENTIUM_4_MCG_MISC  0x0000018A

/**
  MSR information returned for MSR index #MSR_PENTIUM_4_MCG_MISC
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] DS When set, the bit indicates that a page assist or page
    /// fault occurred during DS normal operation. The processors response is
    /// to shut down. The bit is used as an aid for debugging DS handling
    /// code. It is the responsibility of the user (BIOS or operating system)
    /// to clear this bit for normal operation.
    ///
    UINT32    DS        : 1;
    UINT32    Reserved1 : 31;
    UINT32    Reserved2 : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_PENTIUM_4_MCG_MISC_REGISTER;

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check R8 See Section 15.3.2.6, "IA32_MCG
  Extended Machine Check State MSRs.". Registers R8-15 (and the associated
  state-save MSRs) exist only in Intel 64 processors. These registers contain
  valid information only when the processor is operating in 64-bit mode at the
  time of the error.

  @param  ECX  MSR_PENTIUM_4_MCG_R8 (0x00000190)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_R8);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_R8, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_R8 is defined as MSR_MCG_R8 in SDM.
**/
#define MSR_PENTIUM_4_MCG_R8  0x00000190

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check R9D/R9 See Section 15.3.2.6,
  "IA32_MCG Extended Machine Check State MSRs.". Registers R8-15 (and the
  associated state-save MSRs) exist only in Intel 64 processors. These
  registers contain valid information only when the processor is operating in
  64-bit mode at the time of the error.

  @param  ECX  MSR_PENTIUM_4_MCG_R9 (0x00000191)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_R9);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_R9, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_R9 is defined as MSR_MCG_R9 in SDM.
**/
#define MSR_PENTIUM_4_MCG_R9  0x00000191

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check R10 See Section 15.3.2.6, "IA32_MCG
  Extended Machine Check State MSRs.". Registers R8-15 (and the associated
  state-save MSRs) exist only in Intel 64 processors. These registers contain
  valid information only when the processor is operating in 64-bit mode at the
  time of the error.

  @param  ECX  MSR_PENTIUM_4_MCG_R10 (0x00000192)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_R10);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_R10, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_R10 is defined as MSR_MCG_R10 in SDM.
**/
#define MSR_PENTIUM_4_MCG_R10  0x00000192

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check R11 See Section 15.3.2.6, "IA32_MCG
  Extended Machine Check State MSRs.". Registers R8-15 (and the associated
  state-save MSRs) exist only in Intel 64 processors. These registers contain
  valid information only when the processor is operating in 64-bit mode at the
  time of the error.

  @param  ECX  MSR_PENTIUM_4_MCG_R11 (0x00000193)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_R11);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_R11, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_R11 is defined as MSR_MCG_R11 in SDM.
**/
#define MSR_PENTIUM_4_MCG_R11  0x00000193

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check R12 See Section 15.3.2.6, "IA32_MCG
  Extended Machine Check State MSRs.". Registers R8-15 (and the associated
  state-save MSRs) exist only in Intel 64 processors. These registers contain
  valid information only when the processor is operating in 64-bit mode at the
  time of the error.

  @param  ECX  MSR_PENTIUM_4_MCG_R12 (0x00000194)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_R12);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_R12, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_R12 is defined as MSR_MCG_R12 in SDM.
**/
#define MSR_PENTIUM_4_MCG_R12  0x00000194

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check R13 See Section 15.3.2.6, "IA32_MCG
  Extended Machine Check State MSRs.". Registers R8-15 (and the associated
  state-save MSRs) exist only in Intel 64 processors. These registers contain
  valid information only when the processor is operating in 64-bit mode at the
  time of the error.

  @param  ECX  MSR_PENTIUM_4_MCG_R13 (0x00000195)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_R13);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_R13, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_R13 is defined as MSR_MCG_R13 in SDM.
**/
#define MSR_PENTIUM_4_MCG_R13  0x00000195

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check R14 See Section 15.3.2.6, "IA32_MCG
  Extended Machine Check State MSRs.". Registers R8-15 (and the associated
  state-save MSRs) exist only in Intel 64 processors. These registers contain
  valid information only when the processor is operating in 64-bit mode at the
  time of the error.

  @param  ECX  MSR_PENTIUM_4_MCG_R14 (0x00000196)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_R14);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_R14, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_R14 is defined as MSR_MCG_R14 in SDM.
**/
#define MSR_PENTIUM_4_MCG_R14  0x00000196

/**
  0, 1, 2, 3, 4, 6. Unique. Machine Check R15 See Section 15.3.2.6, "IA32_MCG
  Extended Machine Check State MSRs.". Registers R8-15 (and the associated
  state-save MSRs) exist only in Intel 64 processors. These registers contain
  valid information only when the processor is operating in 64-bit mode at the
  time of the error.

  @param  ECX  MSR_PENTIUM_4_MCG_R15 (0x00000197)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MCG_R15);
  AsmWriteMsr64 (MSR_PENTIUM_4_MCG_R15, Msr);
  @endcode
  @note MSR_PENTIUM_4_MCG_R15 is defined as MSR_MCG_R15 in SDM.
**/
#define MSR_PENTIUM_4_MCG_R15  0x00000197

/**
  Thermal Monitor 2 Control. 3,. Shared. For Family F, Model 3 processors:
  When read, specifies the value of the target TM2 transition last written.
  When set, it sets the next target value for TM2 transition. 4, 6. Shared.
  For Family F, Model 4 and Model 6 processors: When read, specifies the value
  of the target TM2 transition last written. Writes may cause #GP exceptions.

  @param  ECX  MSR_PENTIUM_4_THERM2_CTL (0x0000019D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_THERM2_CTL);
  AsmWriteMsr64 (MSR_PENTIUM_4_THERM2_CTL, Msr);
  @endcode
  @note MSR_PENTIUM_4_THERM2_CTL is defined as MSR_THERM2_CTL in SDM.
**/
#define MSR_PENTIUM_4_THERM2_CTL  0x0000019D

/**
  0, 1, 2, 3, 4, 6. Shared. Enable Miscellaneous Processor Features (R/W).

  @param  ECX  MSR_PENTIUM_4_IA32_MISC_ENABLE (0x000001A0)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_IA32_MISC_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_IA32_MISC_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_PENTIUM_4_IA32_MISC_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_PENTIUM_4_IA32_MISC_ENABLE);
  AsmWriteMsr64 (MSR_PENTIUM_4_IA32_MISC_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_PENTIUM_4_IA32_MISC_ENABLE is defined as IA32_MISC_ENABLE in SDM.
**/
#define MSR_PENTIUM_4_IA32_MISC_ENABLE  0x000001A0

/**
  MSR information returned for MSR index #MSR_PENTIUM_4_IA32_MISC_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Fast-Strings Enable. See Table 2-2.
    ///
    UINT32    FastStrings : 1;
    UINT32    Reserved1   : 1;
    ///
    /// [Bit 2] x87 FPU Fopcode Compatibility Mode Enable.
    ///
    UINT32    FPU         : 1;
    ///
    /// [Bit 3] Thermal Monitor 1 Enable See Section 14.7.2, "Thermal
    /// Monitor," and see Table 2-2.
    ///
    UINT32    TM1         : 1;
    ///
    /// [Bit 4] Split-Lock Disable When set, the bit causes an #AC exception
    /// to be issued instead of a split-lock cycle. Operating systems that set
    /// this bit must align system structures to avoid split-lock scenarios.
    /// When the bit is clear (default), normal split-locks are issued to the
    /// bus.
    ///   This debug feature is specific to the Pentium 4 processor.
    ///
    UINT32    SplitLockDisable : 1;
    UINT32    Reserved2        : 1;
    ///
    /// [Bit 6] Third-Level Cache Disable (R/W) When set, the third-level
    /// cache is disabled; when clear (default) the third-level cache is
    /// enabled. This flag is reserved for processors that do not have a
    /// third-level cache. Note that the bit controls only the third-level
    /// cache; and only if overall caching is enabled through the CD flag of
    /// control register CR0, the page-level cache controls, and/or the MTRRs.
    /// See Section 11.5.4, "Disabling and Enabling the L3 Cache.".
    ///
    UINT32    ThirdLevelCacheDisable : 1;
    ///
    /// [Bit 7] Performance Monitoring Available (R) See Table 2-2.
    ///
    UINT32    PerformanceMonitoring  : 1;
    ///
    /// [Bit 8] Suppress Lock Enable When set, assertion of LOCK on the bus is
    /// suppressed during a Split Lock access. When clear (default), LOCK is
    /// not suppressed.
    ///
    UINT32    SuppressLockEnable     : 1;
    ///
    /// [Bit 9] Prefetch Queue Disable When set, disables the prefetch queue.
    /// When clear (default), enables the prefetch queue.
    ///
    UINT32    PrefetchQueueDisable   : 1;
    ///
    /// [Bit 10] FERR# Interrupt Reporting Enable (R/W)  When set, interrupt
    /// reporting through the FERR# pin is enabled; when clear, this interrupt
    /// reporting function is disabled.
    ///   When this flag is set and the processor is in the stop-clock state
    ///   (STPCLK# is asserted), asserting the FERR# pin signals to the
    ///   processor that an interrupt (such as, INIT#, BINIT#, INTR, NMI,
    ///   SMI#, or RESET#) is pending and that the processor should return to
    ///   normal operation to handle the interrupt. This flag does not affect
    ///   the normal operation of the FERR# pin (to indicate an unmasked
    ///   floatingpoint error) when the STPCLK# pin is not asserted.
    ///
    UINT32    FERR : 1;
    ///
    /// [Bit 11] Branch Trace Storage Unavailable (BTS_UNAVILABLE) (R) See
    /// Table 2-2. When set, the processor does not support branch trace
    /// storage (BTS); when clear, BTS is supported.
    ///
    UINT32    BTS  : 1;
    ///
    /// [Bit 12] PEBS_UNAVILABLE: Processor Event Based Sampling Unavailable
    /// (R) See Table 2-2. When set, the processor does not support processor
    /// event-based sampling (PEBS); when clear, PEBS is supported.
    ///
    UINT32    PEBS : 1;
    ///
    /// [Bit 13] 3. TM2 Enable (R/W) When this bit is set (1) and the thermal
    /// sensor indicates that the die temperature is at the predetermined
    /// threshold, the Thermal Monitor 2 mechanism is engaged. TM2 will reduce
    /// the bus to core ratio and voltage according to the value last written
    /// to MSR_THERM2_CTL bits 15:0. When this bit is clear (0, default), the
    /// processor does not change the VID signals or the bus to core ratio
    /// when the processor enters a thermal managed state. If the TM2 feature
    /// flag (ECX[8]) is not set to 1 after executing CPUID with EAX = 1, then
    /// this feature is not supported and BIOS must not alter the contents of
    /// this bit location. The processor is operating out of spec if both this
    /// bit and the TM1 bit are set to disabled states.
    ///
    UINT32    TM2       : 1;
    UINT32    Reserved3 : 4;
    ///
    /// [Bit 18] 3, 4, 6. ENABLE MONITOR FSM (R/W) See Table 2-2.
    ///
    UINT32    MONITOR   : 1;
    ///
    /// [Bit 19] Adjacent Cache Line Prefetch Disable (R/W)  When set to 1,
    /// the processor fetches the cache line of the 128-byte sector containing
    /// currently required data. When set to 0, the processor fetches both
    /// cache lines in the sector.
    ///   Single processor platforms should not set this bit. Server platforms
    ///   should set or clear this bit based on platform performance observed
    ///   in validation and testing. BIOS may contain a setup option that
    ///   controls the setting of this bit.
    ///
    UINT32    AdjacentCacheLinePrefetchDisable : 1;
    UINT32    Reserved4                        : 2;
    ///
    /// [Bit 22] 3, 4, 6. Limit CPUID MAXVAL (R/W) See Table 2-2. Setting this
    /// can cause unexpected behavior to software that depends on the
    /// availability of CPUID leaves greater than 3.
    ///
    UINT32    LimitCpuidMaxval                 : 1;
    ///
    /// [Bit 23] Shared. xTPR Message Disable (R/W) See Table 2-2.
    ///
    UINT32    xTPR_Message_Disable             : 1;
    ///
    /// [Bit 24] L1 Data Cache Context Mode (R/W)  When set, the L1 data cache
    /// is placed in shared mode; when clear (default), the cache is placed in
    /// adaptive mode. This bit is only enabled for IA-32 processors that
    /// support Intel Hyper-Threading Technology. See Section 11.5.6, "L1 Data
    /// Cache Context Mode." When L1 is running in adaptive mode and CR3s are
    /// identical, data in L1 is shared across logical processors. Otherwise,
    /// L1 is not shared and cache use is competitive. If the Context ID
    /// feature flag (ECX[10]) is set to 0 after executing CPUID with EAX = 1,
    /// the ability to switch modes is not supported. BIOS must not alter the
    /// contents of IA32_MISC_ENABLE[24].
    ///
    UINT32    L1DataCacheContextMode : 1;
    UINT32    Reserved5              : 7;
    UINT32    Reserved6              : 2;
    ///
    /// [Bit 34] Unique. XD Bit Disable (R/W) See Table 2-2.
    ///
    UINT32    XD                     : 1;
    UINT32    Reserved7              : 29;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_PENTIUM_4_IA32_MISC_ENABLE_REGISTER;

/**
  3, 4, 6. Shared. Platform Feature Requirements (R).

  @param  ECX  MSR_PENTIUM_4_PLATFORM_BRV (0x000001A1)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_PLATFORM_BRV_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_PLATFORM_BRV_REGISTER.

  <b>Example usage</b>
  @code
  MSR_PENTIUM_4_PLATFORM_BRV_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_PENTIUM_4_PLATFORM_BRV);
  @endcode
  @note MSR_PENTIUM_4_PLATFORM_BRV is defined as MSR_PLATFORM_BRV in SDM.
**/
#define MSR_PENTIUM_4_PLATFORM_BRV  0x000001A1

/**
  MSR information returned for MSR index #MSR_PENTIUM_4_PLATFORM_BRV
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1 : 18;
    ///
    /// [Bit 18] PLATFORM Requirements When set to 1, indicates the processor
    /// has specific platform requirements. The details of the platform
    /// requirements are listed in the respective data sheets of the processor.
    ///
    UINT32    PLATFORM  : 1;
    UINT32    Reserved2 : 13;
    UINT32    Reserved3 : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_PENTIUM_4_PLATFORM_BRV_REGISTER;

/**
  0, 1, 2, 3, 4, 6. Unique. Last Exception Record From Linear IP (R)  Contains
  a pointer to the last branch instruction that the processor executed prior
  to the last exception that was generated or the last interrupt that was
  handled. See Section 17.13.3, "Last Exception Records.". Unique. From Linear
  IP Linear address of the last branch instruction (If IA-32e mode is active).
  From Linear IP Linear address of the last branch instruction. Reserved.

  @param  ECX  MSR_PENTIUM_4_LER_FROM_LIP (0x000001D7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_LER_FROM_LIP);
  @endcode
  @note MSR_PENTIUM_4_LER_FROM_LIP is defined as MSR_LER_FROM_LIP in SDM.
**/
#define MSR_PENTIUM_4_LER_FROM_LIP  0x000001D7

/**
  0, 1, 2, 3, 4, 6. Unique. Last Exception Record To Linear IP (R)  This area
  contains a pointer to the target of the last branch instruction that the
  processor executed prior to the last exception that was generated or the
  last interrupt that was handled. See Section 17.13.3, "Last Exception
  Records.". Unique. From Linear IP Linear address of the target of the last
  branch instruction (If IA-32e mode is active). From Linear IP Linear address
  of the target of the last branch instruction. Reserved.

  @param  ECX  MSR_PENTIUM_4_LER_TO_LIP (0x000001D8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_LER_TO_LIP);
  @endcode
  @note MSR_PENTIUM_4_LER_TO_LIP is defined as MSR_LER_TO_LIP in SDM.
**/
#define MSR_PENTIUM_4_LER_TO_LIP  0x000001D8

/**
  0, 1, 2, 3, 4, 6. Unique. Debug Control (R/W)  Controls how several debug
  features are used. Bit definitions are discussed in the referenced section.
  See Section 17.13.1, "MSR_DEBUGCTLA MSR.".

  @param  ECX  MSR_PENTIUM_4_DEBUGCTLA (0x000001D9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_DEBUGCTLA);
  AsmWriteMsr64 (MSR_PENTIUM_4_DEBUGCTLA, Msr);
  @endcode
  @note MSR_PENTIUM_4_DEBUGCTLA is defined as MSR_DEBUGCTLA in SDM.
**/
#define MSR_PENTIUM_4_DEBUGCTLA  0x000001D9

/**
  0, 1, 2, 3, 4, 6. Unique. Last Branch Record Stack TOS (R/W)  Contains an
  index (0-3 or 0-15) that points to the top of the last branch record stack
  (that is, that points the index of the MSR containing the most recent branch
  record). See Section 17.13.2, "LBR Stack for Processors Based on Intel
  NetBurst(R) Microarchitecture"; and addresses 1DBH-1DEH and 680H-68FH.

  @param  ECX  MSR_PENTIUM_4_LASTBRANCH_TOS (0x000001DA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_LASTBRANCH_TOS);
  AsmWriteMsr64 (MSR_PENTIUM_4_LASTBRANCH_TOS, Msr);
  @endcode
  @note MSR_PENTIUM_4_LASTBRANCH_TOS is defined as MSR_LASTBRANCH_TOS in SDM.
**/
#define MSR_PENTIUM_4_LASTBRANCH_TOS  0x000001DA

/**
  0, 1, 2. Unique. Last Branch Record n (R/W)  One of four last branch record
  registers on the last branch record stack. It contains pointers to the
  source and destination instruction for one of the last four branches,
  exceptions, or interrupts that the processor took. MSR_LASTBRANCH_0 through
  MSR_LASTBRANCH_3 at 1DBH-1DEH are available only on family 0FH, models
  0H-02H. They have been replaced by the MSRs at 680H68FH and 6C0H-6CFH. See
  Section 17.12, "Last Branch, Call Stack, Interrupt, and Exception Recording
  for Processors based on Skylake Microarchitecture.".

  @param  ECX  MSR_PENTIUM_4_LASTBRANCH_n
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_LASTBRANCH_0);
  AsmWriteMsr64 (MSR_PENTIUM_4_LASTBRANCH_0, Msr);
  @endcode
  @note MSR_PENTIUM_4_LASTBRANCH_0 is defined as MSR_LASTBRANCH_0 in SDM.
        MSR_PENTIUM_4_LASTBRANCH_1 is defined as MSR_LASTBRANCH_1 in SDM.
        MSR_PENTIUM_4_LASTBRANCH_2 is defined as MSR_LASTBRANCH_2 in SDM.
        MSR_PENTIUM_4_LASTBRANCH_3 is defined as MSR_LASTBRANCH_3 in SDM.
  @{
**/
#define MSR_PENTIUM_4_LASTBRANCH_0  0x000001DB
#define MSR_PENTIUM_4_LASTBRANCH_1  0x000001DC
#define MSR_PENTIUM_4_LASTBRANCH_2  0x000001DD
#define MSR_PENTIUM_4_LASTBRANCH_3  0x000001DE
/// @}

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.2, "Performance Counters.".

  @param  ECX  MSR_PENTIUM_4_BPU_COUNTERn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_BPU_COUNTER0);
  AsmWriteMsr64 (MSR_PENTIUM_4_BPU_COUNTER0, Msr);
  @endcode
  @note MSR_PENTIUM_4_BPU_COUNTER0 is defined as MSR_BPU_COUNTER0 in SDM.
        MSR_PENTIUM_4_BPU_COUNTER1 is defined as MSR_BPU_COUNTER1 in SDM.
        MSR_PENTIUM_4_BPU_COUNTER2 is defined as MSR_BPU_COUNTER2 in SDM.
        MSR_PENTIUM_4_BPU_COUNTER3 is defined as MSR_BPU_COUNTER3 in SDM.
  @{
**/
#define MSR_PENTIUM_4_BPU_COUNTER0  0x00000300
#define MSR_PENTIUM_4_BPU_COUNTER1  0x00000301
#define MSR_PENTIUM_4_BPU_COUNTER2  0x00000302
#define MSR_PENTIUM_4_BPU_COUNTER3  0x00000303
/// @}

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.2, "Performance Counters.".

  @param  ECX  MSR_PENTIUM_4_MS_COUNTERn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MS_COUNTER0);
  AsmWriteMsr64 (MSR_PENTIUM_4_MS_COUNTER0, Msr);
  @endcode
  @note MSR_PENTIUM_4_MS_COUNTER0 is defined as MSR_MS_COUNTER0 in SDM.
        MSR_PENTIUM_4_MS_COUNTER1 is defined as MSR_MS_COUNTER1 in SDM.
        MSR_PENTIUM_4_MS_COUNTER2 is defined as MSR_MS_COUNTER2 in SDM.
        MSR_PENTIUM_4_MS_COUNTER3 is defined as MSR_MS_COUNTER3 in SDM.
  @{
**/
#define MSR_PENTIUM_4_MS_COUNTER0  0x00000304
#define MSR_PENTIUM_4_MS_COUNTER1  0x00000305
#define MSR_PENTIUM_4_MS_COUNTER2  0x00000306
#define MSR_PENTIUM_4_MS_COUNTER3  0x00000307
/// @}

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.2, "Performance Counters.".

  @param  ECX  MSR_PENTIUM_4_FLAME_COUNTERn (0x00000308)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_FLAME_COUNTER0);
  AsmWriteMsr64 (MSR_PENTIUM_4_FLAME_COUNTER0, Msr);
  @endcode
  @note MSR_PENTIUM_4_FLAME_COUNTER0 is defined as MSR_FLAME_COUNTER0 in SDM.
        MSR_PENTIUM_4_FLAME_COUNTER1 is defined as MSR_FLAME_COUNTER1 in SDM.
        MSR_PENTIUM_4_FLAME_COUNTER2 is defined as MSR_FLAME_COUNTER2 in SDM.
        MSR_PENTIUM_4_FLAME_COUNTER3 is defined as MSR_FLAME_COUNTER3 in SDM.
  @{
**/
#define MSR_PENTIUM_4_FLAME_COUNTER0  0x00000308
#define MSR_PENTIUM_4_FLAME_COUNTER1  0x00000309
#define MSR_PENTIUM_4_FLAME_COUNTER2  0x0000030A
#define MSR_PENTIUM_4_FLAME_COUNTER3  0x0000030B
/// @}

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.2, "Performance Counters.".

  @param  ECX  MSR_PENTIUM_4_IQ_COUNTERn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_IQ_COUNTER0);
  AsmWriteMsr64 (MSR_PENTIUM_4_IQ_COUNTER0, Msr);
  @endcode
  @note MSR_PENTIUM_4_IQ_COUNTER0 is defined as MSR_IQ_COUNTER0 in SDM.
        MSR_PENTIUM_4_IQ_COUNTER1 is defined as MSR_IQ_COUNTER1 in SDM.
        MSR_PENTIUM_4_IQ_COUNTER2 is defined as MSR_IQ_COUNTER2 in SDM.
        MSR_PENTIUM_4_IQ_COUNTER3 is defined as MSR_IQ_COUNTER3 in SDM.
        MSR_PENTIUM_4_IQ_COUNTER4 is defined as MSR_IQ_COUNTER4 in SDM.
        MSR_PENTIUM_4_IQ_COUNTER5 is defined as MSR_IQ_COUNTER5 in SDM.
  @{
**/
#define MSR_PENTIUM_4_IQ_COUNTER0  0x0000030C
#define MSR_PENTIUM_4_IQ_COUNTER1  0x0000030D
#define MSR_PENTIUM_4_IQ_COUNTER2  0x0000030E
#define MSR_PENTIUM_4_IQ_COUNTER3  0x0000030F
#define MSR_PENTIUM_4_IQ_COUNTER4  0x00000310
#define MSR_PENTIUM_4_IQ_COUNTER5  0x00000311
/// @}

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.3, "CCCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_BPU_CCCRn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_BPU_CCCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_BPU_CCCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_BPU_CCCR0 is defined as MSR_BPU_CCCR0 in SDM.
        MSR_PENTIUM_4_BPU_CCCR1 is defined as MSR_BPU_CCCR1 in SDM.
        MSR_PENTIUM_4_BPU_CCCR2 is defined as MSR_BPU_CCCR2 in SDM.
        MSR_PENTIUM_4_BPU_CCCR3 is defined as MSR_BPU_CCCR3 in SDM.
  @{
**/
#define MSR_PENTIUM_4_BPU_CCCR0  0x00000360
#define MSR_PENTIUM_4_BPU_CCCR1  0x00000361
#define MSR_PENTIUM_4_BPU_CCCR2  0x00000362
#define MSR_PENTIUM_4_BPU_CCCR3  0x00000363
/// @}

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.3, "CCCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_MS_CCCRn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MS_CCCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_MS_CCCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_MS_CCCR0 is defined as MSR_MS_CCCR0 in SDM.
        MSR_PENTIUM_4_MS_CCCR1 is defined as MSR_MS_CCCR1 in SDM.
        MSR_PENTIUM_4_MS_CCCR2 is defined as MSR_MS_CCCR2 in SDM.
        MSR_PENTIUM_4_MS_CCCR3 is defined as MSR_MS_CCCR3 in SDM.
  @{
**/
#define MSR_PENTIUM_4_MS_CCCR0  0x00000364
#define MSR_PENTIUM_4_MS_CCCR1  0x00000365
#define MSR_PENTIUM_4_MS_CCCR2  0x00000366
#define MSR_PENTIUM_4_MS_CCCR3  0x00000367
/// @}

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.3, "CCCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_FLAME_CCCRn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_FLAME_CCCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_FLAME_CCCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_FLAME_CCCR0 is defined as MSR_FLAME_CCCR0 in SDM.
        MSR_PENTIUM_4_FLAME_CCCR1 is defined as MSR_FLAME_CCCR1 in SDM.
        MSR_PENTIUM_4_FLAME_CCCR2 is defined as MSR_FLAME_CCCR2 in SDM.
        MSR_PENTIUM_4_FLAME_CCCR3 is defined as MSR_FLAME_CCCR3 in SDM.
  @{
**/
#define MSR_PENTIUM_4_FLAME_CCCR0  0x00000368
#define MSR_PENTIUM_4_FLAME_CCCR1  0x00000369
#define MSR_PENTIUM_4_FLAME_CCCR2  0x0000036A
#define MSR_PENTIUM_4_FLAME_CCCR3  0x0000036B
/// @}

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.3, "CCCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_IQ_CCCRn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_IQ_CCCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_IQ_CCCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_IQ_CCCR0 is defined as MSR_IQ_CCCR0 in SDM.
        MSR_PENTIUM_4_IQ_CCCR1 is defined as MSR_IQ_CCCR1 in SDM.
        MSR_PENTIUM_4_IQ_CCCR2 is defined as MSR_IQ_CCCR2 in SDM.
        MSR_PENTIUM_4_IQ_CCCR3 is defined as MSR_IQ_CCCR3 in SDM.
        MSR_PENTIUM_4_IQ_CCCR4 is defined as MSR_IQ_CCCR4 in SDM.
        MSR_PENTIUM_4_IQ_CCCR5 is defined as MSR_IQ_CCCR5 in SDM.
  @{
**/
#define MSR_PENTIUM_4_IQ_CCCR0  0x0000036C
#define MSR_PENTIUM_4_IQ_CCCR1  0x0000036D
#define MSR_PENTIUM_4_IQ_CCCR2  0x0000036E
#define MSR_PENTIUM_4_IQ_CCCR3  0x0000036F
#define MSR_PENTIUM_4_IQ_CCCR4  0x00000370
#define MSR_PENTIUM_4_IQ_CCCR5  0x00000371
/// @}

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_BSU_ESCR0 (0x000003A0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_BSU_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_BSU_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_BSU_ESCR0 is defined as MSR_BSU_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_BSU_ESCR0  0x000003A0

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_BSU_ESCR1 (0x000003A1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_BSU_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_BSU_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_BSU_ESCR1 is defined as MSR_BSU_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_BSU_ESCR1  0x000003A1

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_FSB_ESCR0 (0x000003A2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_FSB_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_FSB_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_FSB_ESCR0 is defined as MSR_FSB_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_FSB_ESCR0  0x000003A2

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_FSB_ESCR1 (0x000003A3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_FSB_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_FSB_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_FSB_ESCR1 is defined as MSR_FSB_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_FSB_ESCR1  0x000003A3

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_FIRM_ESCR0 (0x000003A4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_FIRM_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_FIRM_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_FIRM_ESCR0 is defined as MSR_FIRM_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_FIRM_ESCR0  0x000003A4

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_FIRM_ESCR1 (0x000003A5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_FIRM_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_FIRM_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_FIRM_ESCR1 is defined as MSR_FIRM_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_FIRM_ESCR1  0x000003A5

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_FLAME_ESCR0 (0x000003A6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_FLAME_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_FLAME_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_FLAME_ESCR0 is defined as MSR_FLAME_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_FLAME_ESCR0  0x000003A6

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_FLAME_ESCR1 (0x000003A7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_FLAME_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_FLAME_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_FLAME_ESCR1 is defined as MSR_FLAME_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_FLAME_ESCR1  0x000003A7

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_DAC_ESCR0 (0x000003A8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_DAC_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_DAC_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_DAC_ESCR0 is defined as MSR_DAC_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_DAC_ESCR0  0x000003A8

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_DAC_ESCR1 (0x000003A9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_DAC_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_DAC_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_DAC_ESCR1 is defined as MSR_DAC_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_DAC_ESCR1  0x000003A9

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_MOB_ESCR0 (0x000003AA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MOB_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_MOB_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_MOB_ESCR0 is defined as MSR_MOB_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_MOB_ESCR0  0x000003AA

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_MOB_ESCR1 (0x000003AB)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MOB_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_MOB_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_MOB_ESCR1 is defined as MSR_MOB_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_MOB_ESCR1  0x000003AB

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_PMH_ESCR0 (0x000003AC)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_PMH_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_PMH_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_PMH_ESCR0 is defined as MSR_PMH_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_PMH_ESCR0  0x000003AC

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_PMH_ESCR1 (0x000003AD)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_PMH_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_PMH_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_PMH_ESCR1 is defined as MSR_PMH_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_PMH_ESCR1  0x000003AD

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_SAAT_ESCR0 (0x000003AE)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_SAAT_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_SAAT_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_SAAT_ESCR0 is defined as MSR_SAAT_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_SAAT_ESCR0  0x000003AE

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_SAAT_ESCR1 (0x000003AF)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_SAAT_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_SAAT_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_SAAT_ESCR1 is defined as MSR_SAAT_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_SAAT_ESCR1  0x000003AF

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_U2L_ESCR0 (0x000003B0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_U2L_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_U2L_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_U2L_ESCR0 is defined as MSR_U2L_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_U2L_ESCR0  0x000003B0

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_U2L_ESCR1 (0x000003B1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_U2L_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_U2L_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_U2L_ESCR1 is defined as MSR_U2L_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_U2L_ESCR1  0x000003B1

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_BPU_ESCR0 (0x000003B2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_BPU_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_BPU_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_BPU_ESCR0 is defined as MSR_BPU_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_BPU_ESCR0  0x000003B2

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_BPU_ESCR1 (0x000003B3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_BPU_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_BPU_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_BPU_ESCR1 is defined as MSR_BPU_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_BPU_ESCR1  0x000003B3

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_IS_ESCR0 (0x000003B4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_IS_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_IS_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_IS_ESCR0 is defined as MSR_IS_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_IS_ESCR0  0x000003B4

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_IS_ESCR1 (0x000003B5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_IS_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_IS_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_IS_ESCR1 is defined as MSR_IS_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_IS_ESCR1  0x000003B5

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_ITLB_ESCR0 (0x000003B6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_ITLB_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_ITLB_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_ITLB_ESCR0 is defined as MSR_ITLB_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_ITLB_ESCR0  0x000003B6

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_ITLB_ESCR1 (0x000003B7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_ITLB_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_ITLB_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_ITLB_ESCR1 is defined as MSR_ITLB_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_ITLB_ESCR1  0x000003B7

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_CRU_ESCR0 (0x000003B8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_CRU_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_CRU_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_CRU_ESCR0 is defined as MSR_CRU_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_CRU_ESCR0  0x000003B8

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_CRU_ESCR1 (0x000003B9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_CRU_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_CRU_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_CRU_ESCR1 is defined as MSR_CRU_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_CRU_ESCR1  0x000003B9

/**
  0, 1, 2. Shared. See Section 18.6.3.1, "ESCR MSRs." This MSR is not
  available on later processors. It is only available on processor family 0FH,
  models 01H-02H.

  @param  ECX  MSR_PENTIUM_4_IQ_ESCR0 (0x000003BA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_IQ_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_IQ_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_IQ_ESCR0 is defined as MSR_IQ_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_IQ_ESCR0  0x000003BA

/**
  0, 1, 2. Shared. See Section 18.6.3.1, "ESCR MSRs." This MSR is not
  available on later processors. It is only available on processor family 0FH,
  models 01H-02H.

  @param  ECX  MSR_PENTIUM_4_IQ_ESCR1 (0x000003BB)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_IQ_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_IQ_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_IQ_ESCR1 is defined as MSR_IQ_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_IQ_ESCR1  0x000003BB

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_RAT_ESCR0 (0x000003BC)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_RAT_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_RAT_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_RAT_ESCR0 is defined as MSR_RAT_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_RAT_ESCR0  0x000003BC

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_RAT_ESCR1 (0x000003BD)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_RAT_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_RAT_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_RAT_ESCR1 is defined as MSR_RAT_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_RAT_ESCR1  0x000003BD

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_SSU_ESCR0 (0x000003BE)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_SSU_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_SSU_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_SSU_ESCR0 is defined as MSR_SSU_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_SSU_ESCR0  0x000003BE

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_MS_ESCR0 (0x000003C0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MS_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_MS_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_MS_ESCR0 is defined as MSR_MS_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_MS_ESCR0  0x000003C0

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_MS_ESCR1 (0x000003C1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_MS_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_MS_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_MS_ESCR1 is defined as MSR_MS_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_MS_ESCR1  0x000003C1

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_TBPU_ESCR0 (0x000003C2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_TBPU_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_TBPU_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_TBPU_ESCR0 is defined as MSR_TBPU_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_TBPU_ESCR0  0x000003C2

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_TBPU_ESCR1 (0x000003C3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_TBPU_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_TBPU_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_TBPU_ESCR1 is defined as MSR_TBPU_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_TBPU_ESCR1  0x000003C3

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_TC_ESCR0 (0x000003C4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_TC_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_TC_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_TC_ESCR0 is defined as MSR_TC_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_TC_ESCR0  0x000003C4

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_TC_ESCR1 (0x000003C5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_TC_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_TC_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_TC_ESCR1 is defined as MSR_TC_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_TC_ESCR1  0x000003C5

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_IX_ESCR0 (0x000003C8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_IX_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_IX_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_IX_ESCR0 is defined as MSR_IX_ESCR0 in SDM.
**/
#define MSR_PENTIUM_4_IX_ESCR0  0x000003C8

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_IX_ESCR1 (0x000003C9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_IX_ESCR1);
  AsmWriteMsr64 (MSR_PENTIUM_4_IX_ESCR1, Msr);
  @endcode
  @note MSR_PENTIUM_4_IX_ESCR1 is defined as MSR_IX_ESCR1 in SDM.
**/
#define MSR_PENTIUM_4_IX_ESCR1  0x000003C9

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_ALF_ESCRn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_ALF_ESCR0);
  AsmWriteMsr64 (MSR_PENTIUM_4_ALF_ESCR0, Msr);
  @endcode
  @note MSR_PENTIUM_4_ALF_ESCR0 is defined as MSR_ALF_ESCR0 in SDM.
        MSR_PENTIUM_4_ALF_ESCR1 is defined as MSR_ALF_ESCR1 in SDM.
        MSR_PENTIUM_4_CRU_ESCR2 is defined as MSR_CRU_ESCR2 in SDM.
        MSR_PENTIUM_4_CRU_ESCR3 is defined as MSR_CRU_ESCR3 in SDM.
        MSR_PENTIUM_4_CRU_ESCR4 is defined as MSR_CRU_ESCR4 in SDM.
        MSR_PENTIUM_4_CRU_ESCR5 is defined as MSR_CRU_ESCR5 in SDM.
  @{
**/
#define MSR_PENTIUM_4_ALF_ESCR0  0x000003CA
#define MSR_PENTIUM_4_ALF_ESCR1  0x000003CB
#define MSR_PENTIUM_4_CRU_ESCR2  0x000003CC
#define MSR_PENTIUM_4_CRU_ESCR3  0x000003CD
#define MSR_PENTIUM_4_CRU_ESCR4  0x000003E0
#define MSR_PENTIUM_4_CRU_ESCR5  0x000003E1
/// @}

/**
  0, 1, 2, 3, 4, 6. Shared. See Section 18.6.3.1, "ESCR MSRs.".

  @param  ECX  MSR_PENTIUM_4_TC_PRECISE_EVENT (0x000003F0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_TC_PRECISE_EVENT);
  AsmWriteMsr64 (MSR_PENTIUM_4_TC_PRECISE_EVENT, Msr);
  @endcode
  @note MSR_PENTIUM_4_TC_PRECISE_EVENT is defined as MSR_TC_PRECISE_EVENT in SDM.
**/
#define MSR_PENTIUM_4_TC_PRECISE_EVENT  0x000003F0

/**
  0, 1, 2, 3, 4, 6. Shared. Processor Event Based Sampling (PEBS) (R/W)
  Controls the enabling of processor event sampling and replay tagging.

  @param  ECX  MSR_PENTIUM_4_PEBS_ENABLE (0x000003F1)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_PEBS_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_PENTIUM_4_PEBS_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_PENTIUM_4_PEBS_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_PENTIUM_4_PEBS_ENABLE);
  AsmWriteMsr64 (MSR_PENTIUM_4_PEBS_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_PENTIUM_4_PEBS_ENABLE is defined as MSR_PEBS_ENABLE in SDM.
**/
#define MSR_PENTIUM_4_PEBS_ENABLE  0x000003F1

/**
  MSR information returned for MSR index #MSR_PENTIUM_4_PEBS_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 12:0] See Table 19-36.
    ///
    UINT32    EventNum            : 13;
    UINT32    Reserved1           : 11;
    ///
    /// [Bit 24] UOP Tag  Enables replay tagging when set.
    ///
    UINT32    UOP                 : 1;
    ///
    /// [Bit 25] ENABLE_PEBS_MY_THR (R/W) Enables PEBS for the target logical
    /// processor when set; disables PEBS when clear (default). See Section
    /// 18.6.4.3, "IA32_PEBS_ENABLE MSR," for an explanation of the target
    /// logical processor. This bit is called ENABLE_PEBS in IA-32 processors
    /// that do not support Intel HyperThreading Technology.
    ///
    UINT32    ENABLE_PEBS_MY_THR  : 1;
    ///
    /// [Bit 26] ENABLE_PEBS_OTH_THR (R/W) Enables PEBS for the target logical
    /// processor when set; disables PEBS when clear (default). See Section
    /// 18.6.4.3, "IA32_PEBS_ENABLE MSR," for an explanation of the target
    /// logical processor. This bit is reserved for IA-32 processors that do
    /// not support Intel Hyper-Threading Technology.
    ///
    UINT32    ENABLE_PEBS_OTH_THR : 1;
    UINT32    Reserved2           : 5;
    UINT32    Reserved3           : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_PENTIUM_4_PEBS_ENABLE_REGISTER;

/**
  0, 1, 2, 3, 4, 6. Shared. See Table 19-36.

  @param  ECX  MSR_PENTIUM_4_PEBS_MATRIX_VERT (0x000003F2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_PEBS_MATRIX_VERT);
  AsmWriteMsr64 (MSR_PENTIUM_4_PEBS_MATRIX_VERT, Msr);
  @endcode
  @note MSR_PENTIUM_4_PEBS_MATRIX_VERT is defined as MSR_PEBS_MATRIX_VERT in SDM.
**/
#define MSR_PENTIUM_4_PEBS_MATRIX_VERT  0x000003F2

/**
  3, 4, 6. Unique. Last Branch Record n (R/W)  One of 16 pairs of last branch
  record registers on the last branch record stack (680H-68FH). This part of
  the stack contains pointers to the source instruction for one of the last 16
  branches, exceptions, or interrupts taken by the processor. The MSRs at
  680H-68FH, 6C0H-6CfH are not available in processor releases before family
  0FH, model 03H. These MSRs replace MSRs previously located at
  1DBH-1DEH.which performed the same function for early releases. See Section
  17.12, "Last Branch, Call Stack, Interrupt, and Exception Recording for
  Processors based on Skylake Microarchitecture.".

  @param  ECX  MSR_PENTIUM_4_LASTBRANCH_n_FROM_IP
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_LASTBRANCH_0_FROM_IP);
  AsmWriteMsr64 (MSR_PENTIUM_4_LASTBRANCH_0_FROM_IP, Msr);
  @endcode
  @note MSR_PENTIUM_4_LASTBRANCH_0_FROM_IP  is defined as MSR_LASTBRANCH_0_FROM_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_1_FROM_IP  is defined as MSR_LASTBRANCH_1_FROM_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_2_FROM_IP  is defined as MSR_LASTBRANCH_2_FROM_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_3_FROM_IP  is defined as MSR_LASTBRANCH_3_FROM_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_4_FROM_IP  is defined as MSR_LASTBRANCH_4_FROM_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_5_FROM_IP  is defined as MSR_LASTBRANCH_5_FROM_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_6_FROM_IP  is defined as MSR_LASTBRANCH_6_FROM_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_7_FROM_IP  is defined as MSR_LASTBRANCH_7_FROM_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_8_FROM_IP  is defined as MSR_LASTBRANCH_8_FROM_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_9_FROM_IP  is defined as MSR_LASTBRANCH_9_FROM_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_10_FROM_IP is defined as MSR_LASTBRANCH_10_FROM_IP in SDM.
        MSR_PENTIUM_4_LASTBRANCH_11_FROM_IP is defined as MSR_LASTBRANCH_11_FROM_IP in SDM.
        MSR_PENTIUM_4_LASTBRANCH_12_FROM_IP is defined as MSR_LASTBRANCH_12_FROM_IP in SDM.
        MSR_PENTIUM_4_LASTBRANCH_13_FROM_IP is defined as MSR_LASTBRANCH_13_FROM_IP in SDM.
        MSR_PENTIUM_4_LASTBRANCH_14_FROM_IP is defined as MSR_LASTBRANCH_14_FROM_IP in SDM.
        MSR_PENTIUM_4_LASTBRANCH_15_FROM_IP is defined as MSR_LASTBRANCH_15_FROM_IP in SDM.
  @{
**/
#define MSR_PENTIUM_4_LASTBRANCH_0_FROM_IP   0x00000680
#define MSR_PENTIUM_4_LASTBRANCH_1_FROM_IP   0x00000681
#define MSR_PENTIUM_4_LASTBRANCH_2_FROM_IP   0x00000682
#define MSR_PENTIUM_4_LASTBRANCH_3_FROM_IP   0x00000683
#define MSR_PENTIUM_4_LASTBRANCH_4_FROM_IP   0x00000684
#define MSR_PENTIUM_4_LASTBRANCH_5_FROM_IP   0x00000685
#define MSR_PENTIUM_4_LASTBRANCH_6_FROM_IP   0x00000686
#define MSR_PENTIUM_4_LASTBRANCH_7_FROM_IP   0x00000687
#define MSR_PENTIUM_4_LASTBRANCH_8_FROM_IP   0x00000688
#define MSR_PENTIUM_4_LASTBRANCH_9_FROM_IP   0x00000689
#define MSR_PENTIUM_4_LASTBRANCH_10_FROM_IP  0x0000068A
#define MSR_PENTIUM_4_LASTBRANCH_11_FROM_IP  0x0000068B
#define MSR_PENTIUM_4_LASTBRANCH_12_FROM_IP  0x0000068C
#define MSR_PENTIUM_4_LASTBRANCH_13_FROM_IP  0x0000068D
#define MSR_PENTIUM_4_LASTBRANCH_14_FROM_IP  0x0000068E
#define MSR_PENTIUM_4_LASTBRANCH_15_FROM_IP  0x0000068F
/// @}

/**
  3, 4, 6. Unique. Last Branch Record n (R/W)  One of 16 pairs of last branch
  record registers on the last branch record stack (6C0H-6CFH). This part of
  the stack contains pointers to the destination instruction for one of the
  last 16 branches, exceptions, or interrupts that the processor took. See
  Section 17.12, "Last Branch, Call Stack, Interrupt, and Exception Recording
  for Processors based on Skylake Microarchitecture.".

  @param  ECX  MSR_PENTIUM_4_LASTBRANCH_n_TO_IP
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_LASTBRANCH_0_TO_IP);
  AsmWriteMsr64 (MSR_PENTIUM_4_LASTBRANCH_0_TO_IP, Msr);
  @endcode
  @note MSR_PENTIUM_4_LASTBRANCH_0_TO_IP  is defined as MSR_LASTBRANCH_0_TO_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_1_TO_IP  is defined as MSR_LASTBRANCH_1_TO_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_2_TO_IP  is defined as MSR_LASTBRANCH_2_TO_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_3_TO_IP  is defined as MSR_LASTBRANCH_3_TO_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_4_TO_IP  is defined as MSR_LASTBRANCH_4_TO_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_5_TO_IP  is defined as MSR_LASTBRANCH_5_TO_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_6_TO_IP  is defined as MSR_LASTBRANCH_6_TO_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_7_TO_IP  is defined as MSR_LASTBRANCH_7_TO_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_8_TO_IP  is defined as MSR_LASTBRANCH_8_TO_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_9_TO_IP  is defined as MSR_LASTBRANCH_9_TO_IP  in SDM.
        MSR_PENTIUM_4_LASTBRANCH_10_TO_IP is defined as MSR_LASTBRANCH_10_TO_IP in SDM.
        MSR_PENTIUM_4_LASTBRANCH_11_TO_IP is defined as MSR_LASTBRANCH_11_TO_IP in SDM.
        MSR_PENTIUM_4_LASTBRANCH_12_TO_IP is defined as MSR_LASTBRANCH_12_TO_IP in SDM.
        MSR_PENTIUM_4_LASTBRANCH_13_TO_IP is defined as MSR_LASTBRANCH_13_TO_IP in SDM.
        MSR_PENTIUM_4_LASTBRANCH_14_TO_IP is defined as MSR_LASTBRANCH_14_TO_IP in SDM.
        MSR_PENTIUM_4_LASTBRANCH_15_TO_IP is defined as MSR_LASTBRANCH_15_TO_IP in SDM.
  @{
**/
#define MSR_PENTIUM_4_LASTBRANCH_0_TO_IP   0x000006C0
#define MSR_PENTIUM_4_LASTBRANCH_1_TO_IP   0x000006C1
#define MSR_PENTIUM_4_LASTBRANCH_2_TO_IP   0x000006C2
#define MSR_PENTIUM_4_LASTBRANCH_3_TO_IP   0x000006C3
#define MSR_PENTIUM_4_LASTBRANCH_4_TO_IP   0x000006C4
#define MSR_PENTIUM_4_LASTBRANCH_5_TO_IP   0x000006C5
#define MSR_PENTIUM_4_LASTBRANCH_6_TO_IP   0x000006C6
#define MSR_PENTIUM_4_LASTBRANCH_7_TO_IP   0x000006C7
#define MSR_PENTIUM_4_LASTBRANCH_8_TO_IP   0x000006C8
#define MSR_PENTIUM_4_LASTBRANCH_9_TO_IP   0x000006C9
#define MSR_PENTIUM_4_LASTBRANCH_10_TO_IP  0x000006CA
#define MSR_PENTIUM_4_LASTBRANCH_11_TO_IP  0x000006CB
#define MSR_PENTIUM_4_LASTBRANCH_12_TO_IP  0x000006CC
#define MSR_PENTIUM_4_LASTBRANCH_13_TO_IP  0x000006CD
#define MSR_PENTIUM_4_LASTBRANCH_14_TO_IP  0x000006CE
#define MSR_PENTIUM_4_LASTBRANCH_15_TO_IP  0x000006CF
/// @}

/**
  3, 4. Shared. IFSB BUSQ Event Control and Counter Register (R/W) See Section
  18.6.6, "Performance Monitoring on 64bit Intel Xeon Processor MP with Up to
  8-MByte L3 Cache.".

  @param  ECX  MSR_PENTIUM_4_IFSB_BUSQ0 (0x000107CC)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_IFSB_BUSQ0);
  AsmWriteMsr64 (MSR_PENTIUM_4_IFSB_BUSQ0, Msr);
  @endcode
  @note MSR_PENTIUM_4_IFSB_BUSQ0 is defined as MSR_IFSB_BUSQ0 in SDM.
**/
#define MSR_PENTIUM_4_IFSB_BUSQ0  0x000107CC

/**
  3, 4. Shared. IFSB BUSQ Event Control and Counter Register (R/W).

  @param  ECX  MSR_PENTIUM_4_IFSB_BUSQ1 (0x000107CD)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_IFSB_BUSQ1);
  AsmWriteMsr64 (MSR_PENTIUM_4_IFSB_BUSQ1, Msr);
  @endcode
  @note MSR_PENTIUM_4_IFSB_BUSQ1 is defined as MSR_IFSB_BUSQ1 in SDM.
**/
#define MSR_PENTIUM_4_IFSB_BUSQ1  0x000107CD

/**
  3, 4. Shared. IFSB SNPQ Event Control and Counter Register (R/W) See Section
  18.6.6, "Performance Monitoring on 64bit Intel Xeon Processor MP with Up to
  8-MByte L3 Cache.".

  @param  ECX  MSR_PENTIUM_4_IFSB_SNPQ0 (0x000107CE)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_IFSB_SNPQ0);
  AsmWriteMsr64 (MSR_PENTIUM_4_IFSB_SNPQ0, Msr);
  @endcode
  @note MSR_PENTIUM_4_IFSB_SNPQ0 is defined as MSR_IFSB_SNPQ0 in SDM.
**/
#define MSR_PENTIUM_4_IFSB_SNPQ0  0x000107CE

/**
  3, 4. Shared. IFSB SNPQ Event Control and Counter Register (R/W).

  @param  ECX  MSR_PENTIUM_4_IFSB_SNPQ1 (0x000107CF)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_IFSB_SNPQ1);
  AsmWriteMsr64 (MSR_PENTIUM_4_IFSB_SNPQ1, Msr);
  @endcode
  @note MSR_PENTIUM_4_IFSB_SNPQ1 is defined as MSR_IFSB_SNPQ1 in SDM.
**/
#define MSR_PENTIUM_4_IFSB_SNPQ1  0x000107CF

/**
  3, 4. Shared. EFSB DRDY Event Control and Counter Register (R/W) See Section
  18.6.6, "Performance Monitoring on 64bit Intel Xeon Processor MP with Up to
  8-MByte L3 Cache.".

  @param  ECX  MSR_PENTIUM_4_EFSB_DRDY0 (0x000107D0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_EFSB_DRDY0);
  AsmWriteMsr64 (MSR_PENTIUM_4_EFSB_DRDY0, Msr);
  @endcode
  @note MSR_PENTIUM_4_EFSB_DRDY0 is defined as MSR_EFSB_DRDY0 in SDM.
**/
#define MSR_PENTIUM_4_EFSB_DRDY0  0x000107D0

/**
  3, 4. Shared. EFSB DRDY Event Control and Counter Register (R/W).

  @param  ECX  MSR_PENTIUM_4_EFSB_DRDY1 (0x000107D1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_EFSB_DRDY1);
  AsmWriteMsr64 (MSR_PENTIUM_4_EFSB_DRDY1, Msr);
  @endcode
  @note MSR_PENTIUM_4_EFSB_DRDY1 is defined as MSR_EFSB_DRDY1 in SDM.
**/
#define MSR_PENTIUM_4_EFSB_DRDY1  0x000107D1

/**
  3, 4. Shared. IFSB Latency Event Control Register (R/W) See Section 18.6.6,
  "Performance Monitoring on 64bit Intel Xeon Processor MP with Up to 8-MByte
  L3 Cache.".

  @param  ECX  MSR_PENTIUM_4_IFSB_CTL6 (0x000107D2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_IFSB_CTL6);
  AsmWriteMsr64 (MSR_PENTIUM_4_IFSB_CTL6, Msr);
  @endcode
  @note MSR_PENTIUM_4_IFSB_CTL6 is defined as MSR_IFSB_CTL6 in SDM.
**/
#define MSR_PENTIUM_4_IFSB_CTL6  0x000107D2

/**
  3, 4. Shared. IFSB Latency Event Counter Register (R/W) See Section 18.6.6,
  "Performance Monitoring on 64bit Intel Xeon Processor MP with Up to 8-MByte
  L3 Cache.".

  @param  ECX  MSR_PENTIUM_4_IFSB_CNTR7 (0x000107D3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_IFSB_CNTR7);
  AsmWriteMsr64 (MSR_PENTIUM_4_IFSB_CNTR7, Msr);
  @endcode
  @note MSR_PENTIUM_4_IFSB_CNTR7 is defined as MSR_IFSB_CNTR7 in SDM.
**/
#define MSR_PENTIUM_4_IFSB_CNTR7  0x000107D3

/**
  6. Shared. GBUSQ Event Control and Counter Register (R/W) See Section
  18.6.6, "Performance Monitoring on 64-bit Intel Xeon Processor MP with Up to
  8MByte L3 Cache.".

  @param  ECX  MSR_PENTIUM_4_EMON_L3_CTR_CTL0 (0x000107CC)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL0);
  AsmWriteMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL0, Msr);
  @endcode
  @note MSR_PENTIUM_4_EMON_L3_CTR_CTL0 is defined as MSR_EMON_L3_CTR_CTL0 in SDM.
**/
#define MSR_PENTIUM_4_EMON_L3_CTR_CTL0  0x000107CC

/**
  6. Shared. GBUSQ Event Control and Counter Register (R/W).

  @param  ECX  MSR_PENTIUM_4_EMON_L3_CTR_CTL1 (0x000107CD)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL1);
  AsmWriteMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL1, Msr);
  @endcode
  @note MSR_PENTIUM_4_EMON_L3_CTR_CTL1 is defined as MSR_EMON_L3_CTR_CTL1 in SDM.
**/
#define MSR_PENTIUM_4_EMON_L3_CTR_CTL1  0x000107CD

/**
  6. Shared. GSNPQ Event Control and Counter Register (R/W) See Section
  18.6.6, "Performance Monitoring on 64-bit Intel Xeon Processor MP with Up to
  8MByte L3 Cache.".

  @param  ECX  MSR_PENTIUM_4_EMON_L3_CTR_CTL2 (0x000107CE)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL2);
  AsmWriteMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL2, Msr);
  @endcode
  @note MSR_PENTIUM_4_EMON_L3_CTR_CTL2 is defined as MSR_EMON_L3_CTR_CTL2 in SDM.
**/
#define MSR_PENTIUM_4_EMON_L3_CTR_CTL2  0x000107CE

/**
  6. Shared. GSNPQ Event Control and Counter Register (R/W).

  @param  ECX  MSR_PENTIUM_4_EMON_L3_CTR_CTL3 (0x000107CF)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL3);
  AsmWriteMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL3, Msr);
  @endcode
  @note MSR_PENTIUM_4_EMON_L3_CTR_CTL3 is defined as MSR_EMON_L3_CTR_CTL3 in SDM.
**/
#define MSR_PENTIUM_4_EMON_L3_CTR_CTL3  0x000107CF

/**
  6. Shared. FSB Event Control and Counter Register (R/W) See Section 18.6.6,
  "Performance Monitoring on 64-bit Intel Xeon Processor MP with Up to 8MByte
  L3 Cache.".

  @param  ECX  MSR_PENTIUM_4_EMON_L3_CTR_CTL4 (0x000107D0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL4);
  AsmWriteMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL4, Msr);
  @endcode
  @note MSR_PENTIUM_4_EMON_L3_CTR_CTL4 is defined as MSR_EMON_L3_CTR_CTL4 in SDM.
**/
#define MSR_PENTIUM_4_EMON_L3_CTR_CTL4  0x000107D0

/**
  6. Shared. FSB Event Control and Counter Register (R/W).

  @param  ECX  MSR_PENTIUM_4_EMON_L3_CTR_CTL5 (0x000107D1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL5);
  AsmWriteMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL5, Msr);
  @endcode
  @note MSR_PENTIUM_4_EMON_L3_CTR_CTL5 is defined as MSR_EMON_L3_CTR_CTL5 in SDM.
**/
#define MSR_PENTIUM_4_EMON_L3_CTR_CTL5  0x000107D1

/**
  6. Shared. FSB Event Control and Counter Register (R/W).

  @param  ECX  MSR_PENTIUM_4_EMON_L3_CTR_CTL6 (0x000107D2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL6);
  AsmWriteMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL6, Msr);
  @endcode
  @note MSR_PENTIUM_4_EMON_L3_CTR_CTL6 is defined as MSR_EMON_L3_CTR_CTL6 in SDM.
**/
#define MSR_PENTIUM_4_EMON_L3_CTR_CTL6  0x000107D2

/**
  6. Shared. FSB Event Control and Counter Register (R/W).

  @param  ECX  MSR_PENTIUM_4_EMON_L3_CTR_CTL7 (0x000107D3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL7);
  AsmWriteMsr64 (MSR_PENTIUM_4_EMON_L3_CTR_CTL7, Msr);
  @endcode
  @note MSR_PENTIUM_4_EMON_L3_CTR_CTL7 is defined as MSR_EMON_L3_CTR_CTL7 in SDM.
**/
#define MSR_PENTIUM_4_EMON_L3_CTR_CTL7  0x000107D3

#endif
