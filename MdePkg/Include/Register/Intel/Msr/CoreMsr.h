/** @file
  MSR Definitions for Intel Core Solo and Intel Core Duo Processors.

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

#ifndef __CORE_MSR_H__
#define __CORE_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>

/**
  Is Intel Core Solo and Intel Core Duo Processors?

  @param   DisplayFamily  Display Family ID
  @param   DisplayModel   Display Model ID

  @retval  TRUE   Yes, it is.
  @retval  FALSE  No, it isn't.
**/
#define IS_CORE_PROCESSOR(DisplayFamily, DisplayModel) \
  (DisplayFamily == 0x06 && \
   ( \
    DisplayModel == 0x0E \
    ) \
   )

/**
  Unique. See Section 2.22, "MSRs in Pentium Processors," and see Table 2-2.

  @param  ECX  MSR_CORE_P5_MC_ADDR (0x00000000)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_P5_MC_ADDR);
  AsmWriteMsr64 (MSR_CORE_P5_MC_ADDR, Msr);
  @endcode
  @note MSR_CORE_P5_MC_ADDR is defined as P5_MC_ADDR in SDM.
**/
#define MSR_CORE_P5_MC_ADDR  0x00000000

/**
  Unique. See Section 2.22, "MSRs in Pentium Processors," and see Table 2-2.

  @param  ECX  MSR_CORE_P5_MC_TYPE (0x00000001)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_P5_MC_TYPE);
  AsmWriteMsr64 (MSR_CORE_P5_MC_TYPE, Msr);
  @endcode
  @note MSR_CORE_P5_MC_TYPE is defined as P5_MC_TYPE in SDM.
**/
#define MSR_CORE_P5_MC_TYPE  0x00000001

/**
  Shared. Processor Hard Power-On Configuration (R/W) Enables and disables
  processor features; (R) indicates current processor configuration.

  @param  ECX  MSR_CORE_EBL_CR_POWERON (0x0000002A)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE_EBL_CR_POWERON_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE_EBL_CR_POWERON_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE_EBL_CR_POWERON_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_CORE_EBL_CR_POWERON);
  AsmWriteMsr64 (MSR_CORE_EBL_CR_POWERON, Msr.Uint64);
  @endcode
  @note MSR_CORE_EBL_CR_POWERON is defined as MSR_EBL_CR_POWERON in SDM.
**/
#define MSR_CORE_EBL_CR_POWERON  0x0000002A

/**
  MSR information returned for MSR index #MSR_CORE_EBL_CR_POWERON
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
    UINT32    Reserved2                   : 2;
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
    UINT32    Reserved3                   : 1;
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
    /// [Bit 18] System Bus Frequency (R/O) 1. = 100 MHz 2. = Reserved.
    ///
    UINT32    SystemBusFrequency          : 1;
    UINT32    Reserved6                   : 1;
    ///
    /// [Bits 21:20] Symmetric Arbitration ID (R/O).
    ///
    UINT32    SymmetricArbitrationID      : 2;
    ///
    /// [Bits 26:22] Clock Frequency Ratio (R/O).
    ///
    UINT32    ClockFrequencyRatio         : 5;
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
} MSR_CORE_EBL_CR_POWERON_REGISTER;

/**
  Unique. Last Branch Record n (R/W) One of 8 last branch record registers on
  the last branch record stack: bits 31-0 hold the 'from' address and bits
  63-32 hold the 'to' address. See also: -  Last Branch Record Stack TOS at
  1C9H -  Section 17.15, "Last Branch, Interrupt, and Exception Recording
  (Pentium M Processors).".

  @param  ECX  MSR_CORE_LASTBRANCH_n
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_LASTBRANCH_0);
  AsmWriteMsr64 (MSR_CORE_LASTBRANCH_0, Msr);
  @endcode
  @note MSR_CORE_LASTBRANCH_0 is defined as MSR_LASTBRANCH_0 in SDM.
        MSR_CORE_LASTBRANCH_1 is defined as MSR_LASTBRANCH_1 in SDM.
        MSR_CORE_LASTBRANCH_2 is defined as MSR_LASTBRANCH_2 in SDM.
        MSR_CORE_LASTBRANCH_3 is defined as MSR_LASTBRANCH_3 in SDM.
        MSR_CORE_LASTBRANCH_4 is defined as MSR_LASTBRANCH_4 in SDM.
        MSR_CORE_LASTBRANCH_5 is defined as MSR_LASTBRANCH_5 in SDM.
        MSR_CORE_LASTBRANCH_6 is defined as MSR_LASTBRANCH_6 in SDM.
        MSR_CORE_LASTBRANCH_7 is defined as MSR_LASTBRANCH_7 in SDM.
  @{
**/
#define MSR_CORE_LASTBRANCH_0  0x00000040
#define MSR_CORE_LASTBRANCH_1  0x00000041
#define MSR_CORE_LASTBRANCH_2  0x00000042
#define MSR_CORE_LASTBRANCH_3  0x00000043
#define MSR_CORE_LASTBRANCH_4  0x00000044
#define MSR_CORE_LASTBRANCH_5  0x00000045
#define MSR_CORE_LASTBRANCH_6  0x00000046
#define MSR_CORE_LASTBRANCH_7  0x00000047
/// @}

/**
  Shared. Scalable Bus Speed (RO) This field indicates the scalable bus
  clock speed:.

  @param  ECX  MSR_CORE_FSB_FREQ (0x000000CD)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE_FSB_FREQ_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE_FSB_FREQ_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE_FSB_FREQ_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_CORE_FSB_FREQ);
  @endcode
  @note MSR_CORE_FSB_FREQ is defined as MSR_FSB_FREQ in SDM.
**/
#define MSR_CORE_FSB_FREQ  0x000000CD

/**
  MSR information returned for MSR index #MSR_CORE_FSB_FREQ
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
    ///
    /// 133.33 MHz should be utilized if performing calculation with System Bus
    /// Speed when encoding is 101B. 166.67 MHz should be utilized if
    /// performing calculation with System Bus Speed when encoding is 001B.
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
} MSR_CORE_FSB_FREQ_REGISTER;

/**
  Shared.

  @param  ECX  MSR_CORE_BBL_CR_CTL3 (0x0000011E)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE_BBL_CR_CTL3_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE_BBL_CR_CTL3_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE_BBL_CR_CTL3_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_CORE_BBL_CR_CTL3);
  AsmWriteMsr64 (MSR_CORE_BBL_CR_CTL3, Msr.Uint64);
  @endcode
  @note MSR_CORE_BBL_CR_CTL3 is defined as MSR_BBL_CR_CTL3 in SDM.
**/
#define MSR_CORE_BBL_CR_CTL3  0x0000011E

/**
  MSR information returned for MSR index #MSR_CORE_BBL_CR_CTL3
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] L2 Hardware Enabled (RO) 1 = If the L2 is hardware-enabled 0 =
    /// Indicates if the L2 is hardware-disabled.
    ///
    UINT32    L2HardwareEnabled : 1;
    UINT32    Reserved1         : 7;
    ///
    /// [Bit 8] L2 Enabled (R/W)  1 = L2 cache has been initialized 0 =
    /// Disabled (default) Until this bit is set the processor will not
    /// respond to the WBINVD instruction or the assertion of the FLUSH# input.
    ///
    UINT32    L2Enabled         : 1;
    UINT32    Reserved2         : 14;
    ///
    /// [Bit 23] L2 Not Present (RO)  1. = L2 Present 2. = L2 Not Present.
    ///
    UINT32    L2NotPresent      : 1;
    UINT32    Reserved3         : 8;
    UINT32    Reserved4         : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_CORE_BBL_CR_CTL3_REGISTER;

/**
  Unique.

  @param  ECX  MSR_CORE_THERM2_CTL (0x0000019D)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE_THERM2_CTL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE_THERM2_CTL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE_THERM2_CTL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_CORE_THERM2_CTL);
  AsmWriteMsr64 (MSR_CORE_THERM2_CTL, Msr.Uint64);
  @endcode
  @note MSR_CORE_THERM2_CTL is defined as MSR_THERM2_CTL in SDM.
**/
#define MSR_CORE_THERM2_CTL  0x0000019D

/**
  MSR information returned for MSR index #MSR_CORE_THERM2_CTL
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
    /// cleared, TM_SELECT has no effect. Neither TM1 nor TM2 will be enabled.
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
} MSR_CORE_THERM2_CTL_REGISTER;

/**
  Enable Miscellaneous Processor Features (R/W) Allows a variety of processor
  functions to be enabled and disabled.

  @param  ECX  MSR_CORE_IA32_MISC_ENABLE (0x000001A0)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE_IA32_MISC_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE_IA32_MISC_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE_IA32_MISC_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_CORE_IA32_MISC_ENABLE);
  AsmWriteMsr64 (MSR_CORE_IA32_MISC_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_CORE_IA32_MISC_ENABLE is defined as IA32_MISC_ENABLE in SDM.
**/
#define MSR_CORE_IA32_MISC_ENABLE  0x000001A0

/**
  MSR information returned for MSR index #MSR_CORE_IA32_MISC_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1                      : 3;
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
    UINT32    Reserved3                      : 2;
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
    UINT32    Reserved4                      : 1;
    ///
    /// [Bit 13] Shared. TM2 Enable (R/W) When this bit is set (1) and the
    /// thermal sensor indicates that the die temperature is at the
    /// pre-determined threshold, the Thermal Monitor 2 mechanism is engaged.
    /// TM2 will reduce the bus to core ratio and voltage according to the
    /// value last written to MSR_THERM2_CTL bits 15:0. When this bit is clear
    /// (0, default), the processor does not change the VID signals or the bus
    /// to core ratio when the processor enters a thermal managed state. If
    /// the TM2 feature flag (ECX[8]) is not set to 1 after executing CPUID
    /// with EAX = 1, then this feature is not supported and BIOS must not
    /// alter the contents of this bit location. The processor is operating
    /// out of spec if both this bit and the TM1 bit are set to disabled
    /// states.
    ///
    UINT32    TM2              : 1;
    UINT32    Reserved5        : 2;
    ///
    /// [Bit 16] Shared. Enhanced Intel SpeedStep Technology Enable (R/W) 1 =
    /// Enhanced Intel SpeedStep Technology enabled.
    ///
    UINT32    EIST             : 1;
    UINT32    Reserved6        : 1;
    ///
    /// [Bit 18] Shared. ENABLE MONITOR FSM (R/W) See Table 2-2.
    ///
    UINT32    MONITOR          : 1;
    UINT32    Reserved7        : 1;
    UINT32    Reserved8        : 2;
    ///
    /// [Bit 22] Shared. Limit CPUID Maxval (R/W) See Table 2-2. Setting this
    /// bit may cause behavior in software that depends on the availability of
    /// CPUID leaves greater than 2.
    ///
    UINT32    LimitCpuidMaxval : 1;
    UINT32    Reserved9        : 9;
    UINT32    Reserved10       : 2;
    ///
    /// [Bit 34] Shared. XD Bit Disable (R/W) See Table 2-2.
    ///
    UINT32    XD               : 1;
    UINT32    Reserved11       : 29;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_CORE_IA32_MISC_ENABLE_REGISTER;

/**
  Unique. Last Branch Record Stack TOS (R/W)  Contains an index (bits 0-3)
  that points to the MSR containing the most recent branch record. See
  MSR_LASTBRANCH_0_FROM_IP (at 40H).

  @param  ECX  MSR_CORE_LASTBRANCH_TOS (0x000001C9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_LASTBRANCH_TOS);
  AsmWriteMsr64 (MSR_CORE_LASTBRANCH_TOS, Msr);
  @endcode
  @note MSR_CORE_LASTBRANCH_TOS is defined as MSR_LASTBRANCH_TOS in SDM.
**/
#define MSR_CORE_LASTBRANCH_TOS  0x000001C9

/**
  Unique. Last Exception Record From Linear IP (R)  Contains a pointer to the
  last branch instruction that the processor executed prior to the last
  exception that was generated or the last interrupt that was handled.

  @param  ECX  MSR_CORE_LER_FROM_LIP (0x000001DD)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_LER_FROM_LIP);
  @endcode
  @note MSR_CORE_LER_FROM_LIP is defined as MSR_LER_FROM_LIP in SDM.
**/
#define MSR_CORE_LER_FROM_LIP  0x000001DD

/**
  Unique. Last Exception Record To Linear IP (R)  This area contains a pointer
  to the target of the last branch instruction that the processor executed
  prior to the last exception that was generated or the last interrupt that
  was handled.

  @param  ECX  MSR_CORE_LER_TO_LIP (0x000001DE)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_LER_TO_LIP);
  @endcode
  @note MSR_CORE_LER_TO_LIP is defined as MSR_LER_TO_LIP in SDM.
**/
#define MSR_CORE_LER_TO_LIP  0x000001DE

/**
  Unique.

  @param  ECX  MSR_CORE_MTRRPHYSBASEn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MTRRPHYSBASE0);
  AsmWriteMsr64 (MSR_CORE_MTRRPHYSBASE0, Msr);
  @endcode
  @note MSR_CORE_MTRRPHYSBASE0 is defined as MTRRPHYSBASE0 in SDM.
        MSR_CORE_MTRRPHYSBASE1 is defined as MTRRPHYSBASE1 in SDM.
        MSR_CORE_MTRRPHYSBASE2 is defined as MTRRPHYSBASE2 in SDM.
        MSR_CORE_MTRRPHYSBASE3 is defined as MTRRPHYSBASE3 in SDM.
        MSR_CORE_MTRRPHYSBASE4 is defined as MTRRPHYSBASE4 in SDM.
        MSR_CORE_MTRRPHYSBASE5 is defined as MTRRPHYSBASE5 in SDM.
        MSR_CORE_MTRRPHYSMASK6 is defined as MTRRPHYSMASK6 in SDM.
        MSR_CORE_MTRRPHYSMASK7 is defined as MTRRPHYSMASK7 in SDM.
  @{
**/
#define MSR_CORE_MTRRPHYSBASE0  0x00000200
#define MSR_CORE_MTRRPHYSBASE1  0x00000202
#define MSR_CORE_MTRRPHYSBASE2  0x00000204
#define MSR_CORE_MTRRPHYSBASE3  0x00000206
#define MSR_CORE_MTRRPHYSBASE4  0x00000208
#define MSR_CORE_MTRRPHYSBASE5  0x0000020A
#define MSR_CORE_MTRRPHYSMASK6  0x0000020D
#define MSR_CORE_MTRRPHYSMASK7  0x0000020F
/// @}

/**
  Unique.

  @param  ECX  MSR_CORE_MTRRPHYSMASKn (0x00000201)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MTRRPHYSMASK0);
  AsmWriteMsr64 (MSR_CORE_MTRRPHYSMASK0, Msr);
  @endcode
  @note MSR_CORE_MTRRPHYSMASK0 is defined as MTRRPHYSMASK0 in SDM.
        MSR_CORE_MTRRPHYSMASK1 is defined as MTRRPHYSMASK1 in SDM.
        MSR_CORE_MTRRPHYSMASK2 is defined as MTRRPHYSMASK2 in SDM.
        MSR_CORE_MTRRPHYSMASK3 is defined as MTRRPHYSMASK3 in SDM.
        MSR_CORE_MTRRPHYSMASK4 is defined as MTRRPHYSMASK4 in SDM.
        MSR_CORE_MTRRPHYSMASK5 is defined as MTRRPHYSMASK5 in SDM.
        MSR_CORE_MTRRPHYSBASE6 is defined as MTRRPHYSBASE6 in SDM.
        MSR_CORE_MTRRPHYSBASE7 is defined as MTRRPHYSBASE7 in SDM.
  @{
**/
#define MSR_CORE_MTRRPHYSMASK0  0x00000201
#define MSR_CORE_MTRRPHYSMASK1  0x00000203
#define MSR_CORE_MTRRPHYSMASK2  0x00000205
#define MSR_CORE_MTRRPHYSMASK3  0x00000207
#define MSR_CORE_MTRRPHYSMASK4  0x00000209
#define MSR_CORE_MTRRPHYSMASK5  0x0000020B
#define MSR_CORE_MTRRPHYSBASE6  0x0000020C
#define MSR_CORE_MTRRPHYSBASE7  0x0000020E
/// @}

/**
  Unique.

  @param  ECX  MSR_CORE_MTRRFIX64K_00000 (0x00000250)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MTRRFIX64K_00000);
  AsmWriteMsr64 (MSR_CORE_MTRRFIX64K_00000, Msr);
  @endcode
  @note MSR_CORE_MTRRFIX64K_00000 is defined as MTRRFIX64K_00000 in SDM.
**/
#define MSR_CORE_MTRRFIX64K_00000  0x00000250

/**
  Unique.

  @param  ECX  MSR_CORE_MTRRFIX16K_80000 (0x00000258)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MTRRFIX16K_80000);
  AsmWriteMsr64 (MSR_CORE_MTRRFIX16K_80000, Msr);
  @endcode
  @note MSR_CORE_MTRRFIX16K_80000 is defined as MTRRFIX16K_80000 in SDM.
**/
#define MSR_CORE_MTRRFIX16K_80000  0x00000258

/**
  Unique.

  @param  ECX  MSR_CORE_MTRRFIX16K_A0000 (0x00000259)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MTRRFIX16K_A0000);
  AsmWriteMsr64 (MSR_CORE_MTRRFIX16K_A0000, Msr);
  @endcode
  @note MSR_CORE_MTRRFIX16K_A0000 is defined as MTRRFIX16K_A0000 in SDM.
**/
#define MSR_CORE_MTRRFIX16K_A0000  0x00000259

/**
  Unique.

  @param  ECX  MSR_CORE_MTRRFIX4K_C0000 (0x00000268)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MTRRFIX4K_C0000);
  AsmWriteMsr64 (MSR_CORE_MTRRFIX4K_C0000, Msr);
  @endcode
  @note MSR_CORE_MTRRFIX4K_C0000 is defined as MTRRFIX4K_C0000 in SDM.
**/
#define MSR_CORE_MTRRFIX4K_C0000  0x00000268

/**
  Unique.

  @param  ECX  MSR_CORE_MTRRFIX4K_C8000 (0x00000269)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MTRRFIX4K_C8000);
  AsmWriteMsr64 (MSR_CORE_MTRRFIX4K_C8000, Msr);
  @endcode
  @note MSR_CORE_MTRRFIX4K_C8000 is defined as MTRRFIX4K_C8000 in SDM.
**/
#define MSR_CORE_MTRRFIX4K_C8000  0x00000269

/**
  Unique.

  @param  ECX  MSR_CORE_MTRRFIX4K_D0000 (0x0000026A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MTRRFIX4K_D0000);
  AsmWriteMsr64 (MSR_CORE_MTRRFIX4K_D0000, Msr);
  @endcode
  @note MSR_CORE_MTRRFIX4K_D0000 is defined as MTRRFIX4K_D0000 in SDM.
**/
#define MSR_CORE_MTRRFIX4K_D0000  0x0000026A

/**
  Unique.

  @param  ECX  MSR_CORE_MTRRFIX4K_D8000 (0x0000026B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MTRRFIX4K_D8000);
  AsmWriteMsr64 (MSR_CORE_MTRRFIX4K_D8000, Msr);
  @endcode
  @note MSR_CORE_MTRRFIX4K_D8000 is defined as MTRRFIX4K_D8000 in SDM.
**/
#define MSR_CORE_MTRRFIX4K_D8000  0x0000026B

/**
  Unique.

  @param  ECX  MSR_CORE_MTRRFIX4K_E0000 (0x0000026C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MTRRFIX4K_E0000);
  AsmWriteMsr64 (MSR_CORE_MTRRFIX4K_E0000, Msr);
  @endcode
  @note MSR_CORE_MTRRFIX4K_E0000 is defined as MTRRFIX4K_E0000 in SDM.
**/
#define MSR_CORE_MTRRFIX4K_E0000  0x0000026C

/**
  Unique.

  @param  ECX  MSR_CORE_MTRRFIX4K_E8000 (0x0000026D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MTRRFIX4K_E8000);
  AsmWriteMsr64 (MSR_CORE_MTRRFIX4K_E8000, Msr);
  @endcode
  @note MSR_CORE_MTRRFIX4K_E8000 is defined as MTRRFIX4K_E8000 in SDM.
**/
#define MSR_CORE_MTRRFIX4K_E8000  0x0000026D

/**
  Unique.

  @param  ECX  MSR_CORE_MTRRFIX4K_F0000 (0x0000026E)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MTRRFIX4K_F0000);
  AsmWriteMsr64 (MSR_CORE_MTRRFIX4K_F0000, Msr);
  @endcode
  @note MSR_CORE_MTRRFIX4K_F0000 is defined as MTRRFIX4K_F0000 in SDM.
**/
#define MSR_CORE_MTRRFIX4K_F0000  0x0000026E

/**
  Unique.

  @param  ECX  MSR_CORE_MTRRFIX4K_F8000 (0x0000026F)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MTRRFIX4K_F8000);
  AsmWriteMsr64 (MSR_CORE_MTRRFIX4K_F8000, Msr);
  @endcode
  @note MSR_CORE_MTRRFIX4K_F8000 is defined as MTRRFIX4K_F8000 in SDM.
**/
#define MSR_CORE_MTRRFIX4K_F8000  0x0000026F

/**
  Unique. See Section 15.3.2.1, "IA32_MCi_CTL MSRs.".

  @param  ECX  MSR_CORE_MC4_CTL (0x0000040C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MC4_CTL);
  AsmWriteMsr64 (MSR_CORE_MC4_CTL, Msr);
  @endcode
  @note MSR_CORE_MC4_CTL is defined as MSR_MC4_CTL in SDM.
**/
#define MSR_CORE_MC4_CTL  0x0000040C

/**
  Unique. See Section 15.3.2.2, "IA32_MCi_STATUS MSRS.".

  @param  ECX  MSR_CORE_MC4_STATUS (0x0000040D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MC4_STATUS);
  AsmWriteMsr64 (MSR_CORE_MC4_STATUS, Msr);
  @endcode
  @note MSR_CORE_MC4_STATUS is defined as MSR_MC4_STATUS in SDM.
**/
#define MSR_CORE_MC4_STATUS  0x0000040D

/**
  Unique. See Section 15.3.2.3, "IA32_MCi_ADDR MSRs." The MSR_MC4_ADDR
  register is either not implemented or contains no address if the ADDRV flag
  in the MSR_MC4_STATUS register is clear. When not implemented in the
  processor, all reads and writes to this MSR will cause a general-protection
  exception.

  @param  ECX  MSR_CORE_MC4_ADDR (0x0000040E)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MC4_ADDR);
  AsmWriteMsr64 (MSR_CORE_MC4_ADDR, Msr);
  @endcode
  @note MSR_CORE_MC4_ADDR is defined as MSR_MC4_ADDR in SDM.
**/
#define MSR_CORE_MC4_ADDR  0x0000040E

/**
  Unique. See Section 15.3.2.3, "IA32_MCi_ADDR MSRs." The MSR_MC3_ADDR
  register is either not implemented or contains no address if the ADDRV flag
  in the MSR_MC3_STATUS register is clear. When not implemented in the
  processor, all reads and writes to this MSR will cause a general-protection
  exception.

  @param  ECX  MSR_CORE_MC3_ADDR (0x00000412)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MC3_ADDR);
  AsmWriteMsr64 (MSR_CORE_MC3_ADDR, Msr);
  @endcode
  @note MSR_CORE_MC3_ADDR is defined as MSR_MC3_ADDR in SDM.
**/
#define MSR_CORE_MC3_ADDR  0x00000412

/**
  Unique.

  @param  ECX  MSR_CORE_MC3_MISC (0x00000413)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MC3_MISC);
  AsmWriteMsr64 (MSR_CORE_MC3_MISC, Msr);
  @endcode
  @note MSR_CORE_MC3_MISC is defined as MSR_MC3_MISC in SDM.
**/
#define MSR_CORE_MC3_MISC  0x00000413

/**
  Unique.

  @param  ECX  MSR_CORE_MC5_CTL (0x00000414)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MC5_CTL);
  AsmWriteMsr64 (MSR_CORE_MC5_CTL, Msr);
  @endcode
  @note MSR_CORE_MC5_CTL is defined as MSR_MC5_CTL in SDM.
**/
#define MSR_CORE_MC5_CTL  0x00000414

/**
  Unique.

  @param  ECX  MSR_CORE_MC5_STATUS (0x00000415)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MC5_STATUS);
  AsmWriteMsr64 (MSR_CORE_MC5_STATUS, Msr);
  @endcode
  @note MSR_CORE_MC5_STATUS is defined as MSR_MC5_STATUS in SDM.
**/
#define MSR_CORE_MC5_STATUS  0x00000415

/**
  Unique.

  @param  ECX  MSR_CORE_MC5_ADDR (0x00000416)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MC5_ADDR);
  AsmWriteMsr64 (MSR_CORE_MC5_ADDR, Msr);
  @endcode
  @note MSR_CORE_MC5_ADDR is defined as MSR_MC5_ADDR in SDM.
**/
#define MSR_CORE_MC5_ADDR  0x00000416

/**
  Unique.

  @param  ECX  MSR_CORE_MC5_MISC (0x00000417)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_CORE_MC5_MISC);
  AsmWriteMsr64 (MSR_CORE_MC5_MISC, Msr);
  @endcode
  @note MSR_CORE_MC5_MISC is defined as MSR_MC5_MISC in SDM.
**/
#define MSR_CORE_MC5_MISC  0x00000417

/**
  Unique. See Table 2-2.

  @param  ECX  MSR_CORE_IA32_EFER (0xC0000080)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_CORE_IA32_EFER_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_CORE_IA32_EFER_REGISTER.

  <b>Example usage</b>
  @code
  MSR_CORE_IA32_EFER_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_CORE_IA32_EFER);
  AsmWriteMsr64 (MSR_CORE_IA32_EFER, Msr.Uint64);
  @endcode
  @note MSR_CORE_IA32_EFER is defined as IA32_EFER in SDM.
**/
#define MSR_CORE_IA32_EFER  0xC0000080

/**
  MSR information returned for MSR index #MSR_CORE_IA32_EFER
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1 : 11;
    ///
    /// [Bit 11] Execute Disable Bit Enable.
    ///
    UINT32    NXE       : 1;
    UINT32    Reserved2 : 20;
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
} MSR_CORE_IA32_EFER_REGISTER;

#endif
