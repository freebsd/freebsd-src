/** @file
   MSR Definitions for the Intel(R) Core(TM) 2 Processor Family.

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

#ifndef __CORE2_MSR_H__
#define __CORE2_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>

/**
  Is Intel(R) Core(TM) 2 Processor Family?

  @param   DisplayFamily  Display Family ID
  @param   DisplayModel   Display Model ID

  @retval  TRUE   Yes, it is.
  @retval  FALSE  No, it isn't.
**/
#define IS_CORE2_PROCESSOR(DisplayFamily, DisplayModel) \
  (DisplayFamily == 0x06 && \
   (                        \
    DisplayModel == 0x0F || \
    DisplayModel == 0x17    \
    )                       \
   )

/**
  Shared. Model Specific Platform ID (R).

  @param  ECX  MSR_CORE2_PLATFORM_ID (0x00000017)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE2_PLATFORM_ID_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE2_PLATFORM_ID_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE2_PLATFORM_ID_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_CORE2_PLATFORM_ID);
  @endcode
  @note MSR_CORE2_PLATFORM_ID is defined as MSR_PLATFORM_ID in SDM.
**/
#define MSR_CORE2_PLATFORM_ID  0x00000017

/**
  MSR information returned for MSR index #MSR_CORE2_PLATFORM_ID
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1             : 8;
    ///
    /// [Bits 12:8] Maximum Qualified Ratio (R)  The maximum allowed bus ratio.
    ///
    UINT32    MaximumQualifiedRatio : 5;
    UINT32    Reserved2             : 19;
    UINT32    Reserved3             : 18;
    ///
    /// [Bits 52:50] See Table 2-2.
    ///
    UINT32    PlatformId            : 3;
    UINT32    Reserved4             : 11;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_CORE2_PLATFORM_ID_REGISTER;

/**
  Shared. Processor Hard Power-On Configuration (R/W) Enables and disables
  processor features; (R) indicates current processor configuration.

  @param  ECX  MSR_CORE2_EBL_CR_POWERON (0x0000002A)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE2_EBL_CR_POWERON_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE2_EBL_CR_POWERON_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE2_EBL_CR_POWERON_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_CORE2_EBL_CR_POWERON);
  AsmWriteMsr64 (MSR_CORE2_EBL_CR_POWERON, Msr.Uint64);
  @endcode
  @note MSR_CORE2_EBL_CR_POWERON is defined as MSR_EBL_CR_POWERON in SDM.
**/
#define MSR_CORE2_EBL_CR_POWERON  0x0000002A

/**
  MSR information returned for MSR index #MSR_CORE2_EBL_CR_POWERON
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1                   : 1;
    ///
    /// [Bit 1] Data Error Checking Enable (R/W) 1 = Enabled; 0 = Disabled
    /// Note: Not all processor implements R/W.
    ///
    UINT32    DataErrorCheckingEnable     : 1;
    ///
    /// [Bit 2] Response Error Checking Enable (R/W) 1 = Enabled; 0 = Disabled
    /// Note: Not all processor implements R/W.
    ///
    UINT32    ResponseErrorCheckingEnable : 1;
    ///
    /// [Bit 3] MCERR# Drive Enable (R/W)  1 = Enabled; 0 = Disabled Note: Not
    /// all processor implements R/W.
    ///
    UINT32    MCERR_DriveEnable           : 1;
    ///
    /// [Bit 4] Address Parity Enable (R/W) 1 = Enabled; 0 = Disabled Note:
    /// Not all processor implements R/W.
    ///
    UINT32    AddressParityEnable         : 1;
    UINT32    Reserved2                   : 1;
    UINT32    Reserved3                   : 1;
    ///
    /// [Bit 7] BINIT# Driver Enable (R/W) 1 = Enabled; 0 = Disabled Note: Not
    /// all processor implements R/W.
    ///
    UINT32    BINIT_DriverEnable          : 1;
    ///
    /// [Bit 8] Output Tri-state Enabled (R/O) 1 = Enabled; 0 = Disabled.
    ///
    UINT32    OutputTriStateEnable        : 1;
    ///
    /// [Bit 9] Execute BIST (R/O) 1 = Enabled; 0 = Disabled.
    ///
    UINT32    ExecuteBIST                 : 1;
    ///
    /// [Bit 10] MCERR# Observation Enabled (R/O) 1 = Enabled; 0 = Disabled.
    ///
    UINT32    MCERR_ObservationEnabled    : 1;
    ///
    /// [Bit 11] Intel TXT Capable Chipset. (R/O) 1 = Present; 0 = Not Present.
    ///
    UINT32    IntelTXTCapableChipset      : 1;
    ///
    /// [Bit 12] BINIT# Observation Enabled (R/O) 1 = Enabled; 0 = Disabled.
    ///
    UINT32    BINIT_ObservationEnabled    : 1;
    UINT32    Reserved4                   : 1;
    ///
    /// [Bit 14] 1 MByte Power on Reset Vector (R/O) 1 = 1 MByte; 0 = 4 GBytes.
    ///
    UINT32    ResetVector                 : 1;
    UINT32    Reserved5                   : 1;
    ///
    /// [Bits 17:16] APIC Cluster ID (R/O).
    ///
    UINT32    APICClusterID               : 2;
    ///
    /// [Bit 18] N/2 Non-Integer Bus Ratio (R/O) 0 = Integer ratio; 1 =
    /// Non-integer ratio.
    ///
    UINT32    NonIntegerBusRatio          : 1;
    UINT32    Reserved6                   : 1;
    ///
    /// [Bits 21:20] Symmetric Arbitration ID (R/O).
    ///
    UINT32    SymmetricArbitrationID      : 2;
    ///
    /// [Bits 26:22] Integer Bus Frequency Ratio (R/O).
    ///
    UINT32    IntegerBusFrequencyRatio    : 5;
    UINT32    Reserved7                   : 5;
    UINT32    Reserved8                   : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_CORE2_EBL_CR_POWERON_REGISTER;

/**
  Unique. Control Features in Intel 64 Processor (R/W) See Table 2-2.

  @param  ECX  MSR_CORE2_FEATURE_CONTROL (0x0000003A)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE2_FEATURE_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE2_FEATURE_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE2_FEATURE_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_CORE2_FEATURE_CONTROL);
  AsmWriteMsr64 (MSR_CORE2_FEATURE_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_CORE2_FEATURE_CONTROL is defined as MSR_FEATURE_CONTROL in SDM.
**/
#define MSR_CORE2_FEATURE_CONTROL  0x0000003A

/**
  MSR information returned for MSR index #MSR_CORE2_FEATURE_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1  : 3;
    ///
    /// [Bit 3] Unique. SMRR Enable (R/WL) When this bit is set and the lock
    /// bit is set makes the SMRR_PHYS_BASE and SMRR_PHYS_MASK registers read
    /// visible and writeable while in SMM.
    ///
    UINT32    SMRREnable : 1;
    UINT32    Reserved2  : 28;
    UINT32    Reserved3  : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_CORE2_FEATURE_CONTROL_REGISTER;

/**
  Unique. Last Branch Record n From IP (R/W) One of four pairs of last branch
  record registers on the last branch record stack. The From_IP part of the
  stack contains pointers to the source instruction. See also: -  Last Branch
  Record Stack TOS at 1C9H -  Section 17.5.

  @param  ECX  MSR_CORE2_LASTBRANCH_n_FROM_IP
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE2_LASTBRANCH_0_FROM_IP);
  AsmWriteMsr64 (MSR_CORE2_LASTBRANCH_0_FROM_IP, Msr);
  @endcode
  @note MSR_CORE2_LASTBRANCH_0_FROM_IP is defined as MSR_LASTBRANCH_0_FROM_IP in SDM.
        MSR_CORE2_LASTBRANCH_1_FROM_IP is defined as MSR_LASTBRANCH_1_FROM_IP in SDM.
        MSR_CORE2_LASTBRANCH_2_FROM_IP is defined as MSR_LASTBRANCH_2_FROM_IP in SDM.
        MSR_CORE2_LASTBRANCH_3_FROM_IP is defined as MSR_LASTBRANCH_3_FROM_IP in SDM.
  @{
**/
#define MSR_CORE2_LASTBRANCH_0_FROM_IP  0x00000040
#define MSR_CORE2_LASTBRANCH_1_FROM_IP  0x00000041
#define MSR_CORE2_LASTBRANCH_2_FROM_IP  0x00000042
#define MSR_CORE2_LASTBRANCH_3_FROM_IP  0x00000043
/// @}

/**
  Unique. Last Branch Record n To IP (R/W) One of four pairs of last branch
  record registers on the last branch record stack. This To_IP part of the
  stack contains pointers to the destination instruction.

  @param  ECX  MSR_CORE2_LASTBRANCH_n_TO_IP
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE2_LASTBRANCH_0_TO_IP);
  AsmWriteMsr64 (MSR_CORE2_LASTBRANCH_0_TO_IP, Msr);
  @endcode
  @note MSR_CORE2_LASTBRANCH_0_TO_IP is defined as MSR_LASTBRANCH_0_TO_IP in SDM.
        MSR_CORE2_LASTBRANCH_1_TO_IP is defined as MSR_LASTBRANCH_1_TO_IP in SDM.
        MSR_CORE2_LASTBRANCH_2_TO_IP is defined as MSR_LASTBRANCH_2_TO_IP in SDM.
        MSR_CORE2_LASTBRANCH_3_TO_IP is defined as MSR_LASTBRANCH_3_TO_IP in SDM.
  @{
**/
#define MSR_CORE2_LASTBRANCH_0_TO_IP  0x00000060
#define MSR_CORE2_LASTBRANCH_1_TO_IP  0x00000061
#define MSR_CORE2_LASTBRANCH_2_TO_IP  0x00000062
#define MSR_CORE2_LASTBRANCH_3_TO_IP  0x00000063
/// @}

/**
  Unique. System Management Mode Base Address register (WO in SMM)
  Model-specific implementation of SMRR-like interface, read visible and write
  only in SMM.

  @param  ECX  MSR_CORE2_SMRR_PHYSBASE (0x000000A0)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE2_SMRR_PHYSBASE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE2_SMRR_PHYSBASE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE2_SMRR_PHYSBASE_REGISTER  Msr;

  Msr.Uint64 = 0;
  AsmWriteMsr64 (MSR_CORE2_SMRR_PHYSBASE, Msr.Uint64);
  @endcode
  @note MSR_CORE2_SMRR_PHYSBASE is defined as MSR_SMRR_PHYSBASE in SDM.
**/
#define MSR_CORE2_SMRR_PHYSBASE  0x000000A0

/**
  MSR information returned for MSR index #MSR_CORE2_SMRR_PHYSBASE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1 : 12;
    ///
    /// [Bits 31:12] PhysBase. SMRR physical Base Address.
    ///
    UINT32    PhysBase  : 20;
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
} MSR_CORE2_SMRR_PHYSBASE_REGISTER;

/**
  Unique. System Management Mode Physical Address Mask register (WO in SMM)
  Model-specific implementation of SMRR-like interface, read visible and write
  only in SMM.

  @param  ECX  MSR_CORE2_SMRR_PHYSMASK (0x000000A1)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE2_SMRR_PHYSMASK_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE2_SMRR_PHYSMASK_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE2_SMRR_PHYSMASK_REGISTER  Msr;

  Msr.Uint64 = 0;
  AsmWriteMsr64 (MSR_CORE2_SMRR_PHYSMASK, Msr.Uint64);
  @endcode
  @note MSR_CORE2_SMRR_PHYSMASK is defined as MSR_SMRR_PHYSMASK in SDM.
**/
#define MSR_CORE2_SMRR_PHYSMASK  0x000000A1

/**
  MSR information returned for MSR index #MSR_CORE2_SMRR_PHYSMASK
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1 : 11;
    ///
    /// [Bit 11] Valid. Physical address base and range mask are valid.
    ///
    UINT32    Valid     : 1;
    ///
    /// [Bits 31:12] PhysMask. SMRR physical address range mask.
    ///
    UINT32    PhysMask  : 20;
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
} MSR_CORE2_SMRR_PHYSMASK_REGISTER;

/**
  Shared. Scalable Bus Speed(RO) This field indicates the intended scalable
  bus clock speed for processors based on Intel Core microarchitecture:.

  @param  ECX  MSR_CORE2_FSB_FREQ (0x000000CD)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE2_FSB_FREQ_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE2_FSB_FREQ_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE2_FSB_FREQ_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_CORE2_FSB_FREQ);
  @endcode
  @note MSR_CORE2_FSB_FREQ is defined as MSR_FSB_FREQ in SDM.
**/
#define MSR_CORE2_FSB_FREQ  0x000000CD

/**
  MSR information returned for MSR index #MSR_CORE2_FSB_FREQ
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 2:0] - Scalable Bus Speed
    ///   101B: 100 MHz (FSB 400)
    ///   001B: 133 MHz (FSB 533)
    ///   011B: 167 MHz (FSB 667)
    ///   010B: 200 MHz (FSB 800)
    ///   000B: 267 MHz (FSB 1067)
    ///   100B: 333 MHz (FSB 1333)
    ///
    ///   133.33 MHz should be utilized if performing calculation with System
    ///   Bus Speed when encoding is 001B. 166.67 MHz should be utilized if
    ///   performing calculation with System Bus Speed when encoding is 011B.
    ///   266.67 MHz should be utilized if performing calculation with System
    ///   Bus Speed when encoding is 000B. 333.33 MHz should be utilized if
    ///   performing calculation with System Bus Speed when encoding is 100B.
    ///
    UINT32    ScalableBusSpeed : 3;
    UINT32    Reserved1        : 29;
    UINT32    Reserved2        : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_CORE2_FSB_FREQ_REGISTER;

/**
  Shared.

  @param  ECX  MSR_CORE2_PERF_STATUS (0x00000198)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE2_PERF_STATUS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE2_PERF_STATUS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE2_PERF_STATUS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_CORE2_PERF_STATUS);
  AsmWriteMsr64 (MSR_CORE2_PERF_STATUS, Msr.Uint64);
  @endcode
  @note MSR_CORE2_PERF_STATUS is defined as MSR_PERF_STATUS in SDM.
**/
#define MSR_CORE2_PERF_STATUS  0x00000198

/**
  MSR information returned for MSR index #MSR_CORE2_PERF_STATUS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 15:0] Current Performance State Value.
    ///
    UINT32    CurrentPerformanceStateValue : 16;
    UINT32    Reserved1                    : 15;
    ///
    /// [Bit 31] XE Operation (R/O). If set, XE operation is enabled. Default
    /// is cleared.
    ///
    UINT32    XEOperation                  : 1;
    UINT32    Reserved2                    : 8;
    ///
    /// [Bits 44:40] Maximum Bus Ratio (R/O) Indicates maximum bus ratio
    /// configured for the processor.
    ///
    UINT32    MaximumBusRatio              : 5;
    UINT32    Reserved3                    : 1;
    ///
    /// [Bit 46] Non-Integer Bus Ratio (R/O) Indicates non-integer bus ratio
    /// is enabled. Applies processors based on Enhanced Intel Core
    /// microarchitecture.
    ///
    UINT32    NonIntegerBusRatio           : 1;
    UINT32    Reserved4                    : 17;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_CORE2_PERF_STATUS_REGISTER;

/**
  Unique.

  @param  ECX  MSR_CORE2_THERM2_CTL (0x0000019D)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE2_THERM2_CTL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE2_THERM2_CTL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE2_THERM2_CTL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_CORE2_THERM2_CTL);
  AsmWriteMsr64 (MSR_CORE2_THERM2_CTL, Msr.Uint64);
  @endcode
  @note MSR_CORE2_THERM2_CTL is defined as MSR_THERM2_CTL in SDM.
**/
#define MSR_CORE2_THERM2_CTL  0x0000019D

/**
  MSR information returned for MSR index #MSR_CORE2_THERM2_CTL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1 : 16;
    ///
    /// [Bit 16] TM_SELECT (R/W)  Mode of automatic thermal monitor: 1. =
    /// Thermal Monitor 1 (thermally-initiated on-die modulation of the
    /// stop-clock duty cycle) 2. = Thermal Monitor 2 (thermally-initiated
    /// frequency transitions) If bit 3 of the IA32_MISC_ENABLE register is
    /// cleared, TM_SELECT has no effect. Neither TM1 nor TM2 are enabled.
    ///
    UINT32    TM_SELECT : 1;
    UINT32    Reserved2 : 15;
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
} MSR_CORE2_THERM2_CTL_REGISTER;

/**
  Enable Misc. Processor Features (R/W)  Allows a variety of processor
  functions to be enabled and disabled.

  @param  ECX  MSR_CORE2_IA32_MISC_ENABLE (0x000001A0)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE2_IA32_MISC_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE2_IA32_MISC_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE2_IA32_MISC_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_CORE2_IA32_MISC_ENABLE);
  AsmWriteMsr64 (MSR_CORE2_IA32_MISC_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_CORE2_IA32_MISC_ENABLE is defined as IA32_MISC_ENABLE in SDM.
**/
#define MSR_CORE2_IA32_MISC_ENABLE  0x000001A0

/**
  MSR information returned for MSR index #MSR_CORE2_IA32_MISC_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Fast-Strings Enable See Table 2-2.
    ///
    UINT32    FastStrings                    : 1;
    UINT32    Reserved1                      : 2;
    ///
    /// [Bit 3] Unique. Automatic Thermal Control Circuit Enable (R/W) See
    /// Table 2-2.
    ///
    UINT32    AutomaticThermalControlCircuit : 1;
    UINT32    Reserved2                      : 3;
    ///
    /// [Bit 7] Shared. Performance Monitoring Available (R) See Table 2-2.
    ///
    UINT32    PerformanceMonitoring          : 1;
    UINT32    Reserved3                      : 1;
    ///
    /// [Bit 9] Hardware Prefetcher Disable (R/W) When set, disables the
    /// hardware prefetcher operation on streams of data. When clear
    /// (default), enables the prefetch queue. Disabling of the hardware
    /// prefetcher may impact processor performance.
    ///
    UINT32    HardwarePrefetcherDisable      : 1;
    ///
    /// [Bit 10] Shared. FERR# Multiplexing Enable (R/W) 1 = FERR# asserted by
    /// the processor to indicate a pending break event within the processor 0
    /// = Indicates compatible FERR# signaling behavior This bit must be set
    /// to 1 to support XAPIC interrupt model usage.
    ///
    UINT32    FERR                           : 1;
    ///
    /// [Bit 11] Shared. Branch Trace Storage Unavailable (RO) See Table 2-2.
    ///
    UINT32    BTS                            : 1;
    ///
    /// [Bit 12] Shared. Processor Event Based Sampling Unavailable (RO) See
    /// Table 2-2.
    ///
    UINT32    PEBS                           : 1;
    ///
    /// [Bit 13] Shared. TM2 Enable (R/W) When this bit is set (1) and the
    /// thermal sensor indicates that the die temperature is at the
    /// pre-determined threshold, the Thermal Monitor 2 mechanism is engaged.
    /// TM2 will reduce the bus to core ratio and voltage according to the
    /// value last written to MSR_THERM2_CTL bits 15:0.
    ///   When this bit is clear (0, default), the processor does not change
    ///   the VID signals or the bus to core ratio when the processor enters a
    ///   thermally managed state. The BIOS must enable this feature if the
    ///   TM2 feature flag (CPUID.1:ECX[8]) is set; if the TM2 feature flag is
    ///   not set, this feature is not supported and BIOS must not alter the
    ///   contents of the TM2 bit location. The processor is operating out of
    ///   specification if both this bit and the TM1 bit are set to 0.
    ///
    UINT32    TM2       : 1;
    UINT32    Reserved4 : 2;
    ///
    /// [Bit 16] Shared. Enhanced Intel SpeedStep Technology Enable (R/W) See
    /// Table 2-2.
    ///
    UINT32    EIST      : 1;
    UINT32    Reserved5 : 1;
    ///
    /// [Bit 18] Shared. ENABLE MONITOR FSM (R/W) See Table 2-2.
    ///
    UINT32    MONITOR   : 1;
    ///
    /// [Bit 19] Shared. Adjacent Cache Line Prefetch Disable (R/W)  When set
    /// to 1, the processor fetches the cache line that contains data
    /// currently required by the processor. When set to 0, the processor
    /// fetches cache lines that comprise a cache line pair (128 bytes).
    /// Single processor platforms should not set this bit. Server platforms
    /// should set or clear this bit based on platform performance observed in
    /// validation and testing. BIOS may contain a setup option that controls
    /// the setting of this bit.
    ///
    UINT32    AdjacentCacheLinePrefetchDisable : 1;
    ///
    /// [Bit 20] Shared. Enhanced Intel SpeedStep Technology Select Lock
    /// (R/WO) When set, this bit causes the following bits to become
    /// read-only: -  Enhanced Intel SpeedStep Technology Select Lock (this
    /// bit), -  Enhanced Intel SpeedStep Technology Enable bit. The bit must
    /// be set before an Enhanced Intel SpeedStep Technology transition is
    /// requested. This bit is cleared on reset.
    ///
    UINT32    EISTLock             : 1;
    UINT32    Reserved6            : 1;
    ///
    /// [Bit 22] Shared. Limit CPUID Maxval (R/W) See Table 2-2.
    ///
    UINT32    LimitCpuidMaxval     : 1;
    ///
    /// [Bit 23] Shared. xTPR Message Disable (R/W) See Table 2-2.
    ///
    UINT32    xTPR_Message_Disable : 1;
    UINT32    Reserved7            : 8;
    UINT32    Reserved8            : 2;
    ///
    /// [Bit 34] Unique. XD Bit Disable (R/W) See Table 2-2.
    ///
    UINT32    XD                   : 1;
    UINT32    Reserved9            : 2;
    ///
    /// [Bit 37] Unique. DCU Prefetcher Disable (R/W) When set to 1, The DCU
    /// L1 data cache prefetcher is disabled. The default value after reset is
    /// 0. BIOS may write '1' to disable this feature. The DCU prefetcher is
    /// an L1 data cache prefetcher. When the DCU prefetcher detects multiple
    /// loads from the same line done within a time limit, the DCU prefetcher
    /// assumes the next line will be required. The next line is prefetched in
    /// to the L1 data cache from memory or L2.
    ///
    UINT32    DCUPrefetcherDisable : 1;
    ///
    /// [Bit 38] Shared. IDA Disable (R/W) When set to 1 on processors that
    /// support IDA, the Intel Dynamic Acceleration feature (IDA) is disabled
    /// and the IDA_Enable feature flag will be clear (CPUID.06H: EAX[1]=0).
    /// When set to a 0 on processors that support IDA, CPUID.06H: EAX[1]
    /// reports the processor's support of IDA is enabled. Note: the power-on
    /// default value is used by BIOS to detect hardware support of IDA. If
    /// power-on default value is 1, IDA is available in the processor. If
    /// power-on default value is 0, IDA is not available.
    ///
    UINT32    IDADisable : 1;
    ///
    /// [Bit 39] Unique. IP Prefetcher Disable (R/W) When set to 1, The IP
    /// prefetcher is disabled. The default value after reset is 0. BIOS may
    /// write '1' to disable this feature. The IP prefetcher is an L1 data
    /// cache prefetcher. The IP prefetcher looks for sequential load history
    /// to determine whether to prefetch the next expected data into the L1
    /// cache from memory or L2.
    ///
    UINT32    IPPrefetcherDisable : 1;
    UINT32    Reserved10          : 24;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_CORE2_IA32_MISC_ENABLE_REGISTER;

/**
  Unique. Last Branch Record Stack TOS (R/W)  Contains an index (bits 0-3)
  that points to the MSR containing the most recent branch record. See
  MSR_LASTBRANCH_0_FROM_IP (at 40H).

  @param  ECX  MSR_CORE2_LASTBRANCH_TOS (0x000001C9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE2_LASTBRANCH_TOS);
  AsmWriteMsr64 (MSR_CORE2_LASTBRANCH_TOS, Msr);
  @endcode
  @note MSR_CORE2_LASTBRANCH_TOS is defined as MSR_LASTBRANCH_TOS in SDM.
**/
#define MSR_CORE2_LASTBRANCH_TOS  0x000001C9

/**
  Unique. Last Exception Record From Linear IP (R)  Contains a pointer to the
  last branch instruction that the processor executed prior to the last
  exception that was generated or the last interrupt that was handled.

  @param  ECX  MSR_CORE2_LER_FROM_LIP (0x000001DD)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE2_LER_FROM_LIP);
  @endcode
  @note MSR_CORE2_LER_FROM_LIP is defined as MSR_LER_FROM_LIP in SDM.
**/
#define MSR_CORE2_LER_FROM_LIP  0x000001DD

/**
  Unique. Last Exception Record To Linear IP (R)  This area contains a pointer
  to the target of the last branch instruction that the processor executed
  prior to the last exception that was generated or the last interrupt that
  was handled.

  @param  ECX  MSR_CORE2_LER_TO_LIP (0x000001DE)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE2_LER_TO_LIP);
  @endcode
  @note MSR_CORE2_LER_TO_LIP is defined as MSR_LER_TO_LIP in SDM.
**/
#define MSR_CORE2_LER_TO_LIP  0x000001DE

/**
  Unique. Fixed-Function Performance Counter Register n (R/W).

  @param  ECX  MSR_CORE2_PERF_FIXED_CTRn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE2_PERF_FIXED_CTR0);
  AsmWriteMsr64 (MSR_CORE2_PERF_FIXED_CTR0, Msr);
  @endcode
  @note MSR_CORE2_PERF_FIXED_CTR0 is defined as MSR_PERF_FIXED_CTR0 in SDM.
        MSR_CORE2_PERF_FIXED_CTR1 is defined as MSR_PERF_FIXED_CTR1 in SDM.
        MSR_CORE2_PERF_FIXED_CTR2 is defined as MSR_PERF_FIXED_CTR2 in SDM.
  @{
**/
#define MSR_CORE2_PERF_FIXED_CTR0  0x00000309
#define MSR_CORE2_PERF_FIXED_CTR1  0x0000030A
#define MSR_CORE2_PERF_FIXED_CTR2  0x0000030B
/// @}

/**
  Unique. RO. This applies to processors that do not support architectural
  perfmon version 2.

  @param  ECX  MSR_CORE2_PERF_CAPABILITIES (0x00000345)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE2_PERF_CAPABILITIES_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE2_PERF_CAPABILITIES_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE2_PERF_CAPABILITIES_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_CORE2_PERF_CAPABILITIES);
  AsmWriteMsr64 (MSR_CORE2_PERF_CAPABILITIES, Msr.Uint64);
  @endcode
  @note MSR_CORE2_PERF_CAPABILITIES is defined as MSR_PERF_CAPABILITIES in SDM.
**/
#define MSR_CORE2_PERF_CAPABILITIES  0x00000345

/**
  MSR information returned for MSR index #MSR_CORE2_PERF_CAPABILITIES
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 5:0] LBR Format. See Table 2-2.
    ///
    UINT32    LBR_FMT       : 6;
    ///
    /// [Bit 6] PEBS Record Format.
    ///
    UINT32    PEBS_FMT      : 1;
    ///
    /// [Bit 7] PEBSSaveArchRegs. See Table 2-2.
    ///
    UINT32    PEBS_ARCH_REG : 1;
    UINT32    Reserved1     : 24;
    UINT32    Reserved2     : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_CORE2_PERF_CAPABILITIES_REGISTER;

/**
  Unique. Fixed-Function-Counter Control Register (R/W).

  @param  ECX  MSR_CORE2_PERF_FIXED_CTR_CTRL (0x0000038D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE2_PERF_FIXED_CTR_CTRL);
  AsmWriteMsr64 (MSR_CORE2_PERF_FIXED_CTR_CTRL, Msr);
  @endcode
  @note MSR_CORE2_PERF_FIXED_CTR_CTRL is defined as MSR_PERF_FIXED_CTR_CTRL in SDM.
**/
#define MSR_CORE2_PERF_FIXED_CTR_CTRL  0x0000038D

/**
  Unique. See Section 18.6.2.2, "Global Counter Control Facilities.".

  @param  ECX  MSR_CORE2_PERF_GLOBAL_STATUS (0x0000038E)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE2_PERF_GLOBAL_STATUS);
  AsmWriteMsr64 (MSR_CORE2_PERF_GLOBAL_STATUS, Msr);
  @endcode
  @note MSR_CORE2_PERF_GLOBAL_STATUS is defined as MSR_PERF_GLOBAL_STATUS in SDM.
**/
#define MSR_CORE2_PERF_GLOBAL_STATUS  0x0000038E

/**
  Unique. See Section 18.6.2.2, "Global Counter Control Facilities.".

  @param  ECX  MSR_CORE2_PERF_GLOBAL_CTRL (0x0000038F)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE2_PERF_GLOBAL_CTRL);
  AsmWriteMsr64 (MSR_CORE2_PERF_GLOBAL_CTRL, Msr);
  @endcode
  @note MSR_CORE2_PERF_GLOBAL_CTRL is defined as MSR_PERF_GLOBAL_CTRL in SDM.
**/
#define MSR_CORE2_PERF_GLOBAL_CTRL  0x0000038F

/**
  Unique. See Section 18.6.2.2, "Global Counter Control Facilities.".

  @param  ECX  MSR_CORE2_PERF_GLOBAL_OVF_CTRL (0x00000390)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE2_PERF_GLOBAL_OVF_CTRL);
  AsmWriteMsr64 (MSR_CORE2_PERF_GLOBAL_OVF_CTRL, Msr);
  @endcode
  @note MSR_CORE2_PERF_GLOBAL_OVF_CTRL is defined as MSR_PERF_GLOBAL_OVF_CTRL in SDM.
**/
#define MSR_CORE2_PERF_GLOBAL_OVF_CTRL  0x00000390

/**
  Unique. See Table 2-2. See Section 18.6.2.4, "Processor Event Based Sampling
  (PEBS).".

  @param  ECX  MSR_CORE2_PEBS_ENABLE (0x000003F1)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE2_PEBS_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE2_PEBS_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE2_PEBS_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_CORE2_PEBS_ENABLE);
  AsmWriteMsr64 (MSR_CORE2_PEBS_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_CORE2_PEBS_ENABLE is defined as MSR_PEBS_ENABLE in SDM.
**/
#define MSR_CORE2_PEBS_ENABLE  0x000003F1

/**
  MSR information returned for MSR index #MSR_CORE2_PEBS_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Enable PEBS on IA32_PMC0. (R/W).
    ///
    UINT32    Enable    : 1;
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
} MSR_CORE2_PEBS_ENABLE_REGISTER;

/**
  Unique. GBUSQ Event Control/Counter Register (R/W) Apply to Intel Xeon
  processor 7400 series (processor signature 06_1D) only. See Section 17.2.2.

  @param  ECX  MSR_CORE2_EMON_L3_CTR_CTLn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE2_EMON_L3_CTR_CTL0);
  AsmWriteMsr64 (MSR_CORE2_EMON_L3_CTR_CTL0, Msr);
  @endcode
  @note MSR_CORE2_EMON_L3_CTR_CTL0 is defined as MSR_EMON_L3_CTR_CTL0 in SDM.
        MSR_CORE2_EMON_L3_CTR_CTL1 is defined as MSR_EMON_L3_CTR_CTL1 in SDM.
        MSR_CORE2_EMON_L3_CTR_CTL2 is defined as MSR_EMON_L3_CTR_CTL2 in SDM.
        MSR_CORE2_EMON_L3_CTR_CTL3 is defined as MSR_EMON_L3_CTR_CTL3 in SDM.
        MSR_CORE2_EMON_L3_CTR_CTL4 is defined as MSR_EMON_L3_CTR_CTL4 in SDM.
        MSR_CORE2_EMON_L3_CTR_CTL5 is defined as MSR_EMON_L3_CTR_CTL5 in SDM.
        MSR_CORE2_EMON_L3_CTR_CTL6 is defined as MSR_EMON_L3_CTR_CTL6 in SDM.
        MSR_CORE2_EMON_L3_CTR_CTL7 is defined as MSR_EMON_L3_CTR_CTL7 in SDM.
  @{
**/
#define MSR_CORE2_EMON_L3_CTR_CTL0  0x000107CC
#define MSR_CORE2_EMON_L3_CTR_CTL1  0x000107CD
#define MSR_CORE2_EMON_L3_CTR_CTL2  0x000107CE
#define MSR_CORE2_EMON_L3_CTR_CTL3  0x000107CF
#define MSR_CORE2_EMON_L3_CTR_CTL4  0x000107D0
#define MSR_CORE2_EMON_L3_CTR_CTL5  0x000107D1
#define MSR_CORE2_EMON_L3_CTR_CTL6  0x000107D2
#define MSR_CORE2_EMON_L3_CTR_CTL7  0x000107D3
/// @}

/**
  Unique. L3/FSB Common Control Register (R/W) Apply to Intel Xeon processor
  7400 series (processor signature 06_1D) only. See Section 17.2.2.

  @param  ECX  MSR_CORE2_EMON_L3_GL_CTL (0x000107D8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE2_EMON_L3_GL_CTL);
  AsmWriteMsr64 (MSR_CORE2_EMON_L3_GL_CTL, Msr);
  @endcode
  @note MSR_CORE2_EMON_L3_GL_CTL is defined as MSR_EMON_L3_GL_CTL in SDM.
**/
#define MSR_CORE2_EMON_L3_GL_CTL  0x000107D8

#endif
