/** @file
  MSR Definitions for Pentium M Processors.

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

#ifndef __PENTIUM_M_MSR_H__
#define __PENTIUM_M_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>

/**
  Is Pentium M Processors?

  @param   DisplayFamily  Display Family ID
  @param   DisplayModel   Display Model ID

  @retval  TRUE   Yes, it is.
  @retval  FALSE  No, it isn't.
**/
#define IS_PENTIUM_M_PROCESSOR(DisplayFamily, DisplayModel) \
  (DisplayFamily == 0x06 && \
   ( \
    DisplayModel == 0x0D \
    ) \
   )

/**
  See Section 2.22, "MSRs in Pentium Processors.".

  @param  ECX  MSR_PENTIUM_M_P5_MC_ADDR (0x00000000)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_M_P5_MC_ADDR);
  AsmWriteMsr64 (MSR_PENTIUM_M_P5_MC_ADDR, Msr);
  @endcode
  @note MSR_PENTIUM_M_P5_MC_ADDR is defined as P5_MC_ADDR in SDM.
**/
#define MSR_PENTIUM_M_P5_MC_ADDR  0x00000000

/**
  See Section 2.22, "MSRs in Pentium Processors.".

  @param  ECX  MSR_PENTIUM_M_P5_MC_TYPE (0x00000001)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_M_P5_MC_TYPE);
  AsmWriteMsr64 (MSR_PENTIUM_M_P5_MC_TYPE, Msr);
  @endcode
  @note MSR_PENTIUM_M_P5_MC_TYPE is defined as P5_MC_TYPE in SDM.
**/
#define MSR_PENTIUM_M_P5_MC_TYPE  0x00000001

/**
  Processor Hard Power-On Configuration (R/W) Enables and disables processor
  features. (R) Indicates current processor configuration.

  @param  ECX  MSR_PENTIUM_M_EBL_CR_POWERON (0x0000002A)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_PENTIUM_M_EBL_CR_POWERON_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_PENTIUM_M_EBL_CR_POWERON_REGISTER.

  <b>Example usage</b>
  @code
  MSR_PENTIUM_M_EBL_CR_POWERON_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_PENTIUM_M_EBL_CR_POWERON);
  AsmWriteMsr64 (MSR_PENTIUM_M_EBL_CR_POWERON, Msr.Uint64);
  @endcode
  @note MSR_PENTIUM_M_EBL_CR_POWERON is defined as MSR_EBL_CR_POWERON in SDM.
**/
#define MSR_PENTIUM_M_EBL_CR_POWERON  0x0000002A

/**
  MSR information returned for MSR index #MSR_PENTIUM_M_EBL_CR_POWERON
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1                   : 1;
    ///
    /// [Bit 1] Data Error Checking Enable (R) 0 = Disabled Always 0 on the
    /// Pentium M processor.
    ///
    UINT32    DataErrorCheckingEnable     : 1;
    ///
    /// [Bit 2] Response Error Checking Enable (R) 0 = Disabled Always 0 on
    /// the Pentium M processor.
    ///
    UINT32    ResponseErrorCheckingEnable : 1;
    ///
    /// [Bit 3] MCERR# Drive Enable (R)  0 = Disabled Always 0 on the Pentium
    /// M processor.
    ///
    UINT32    MCERR_DriveEnable           : 1;
    ///
    /// [Bit 4] Address Parity Enable (R) 0 = Disabled Always 0 on the Pentium
    /// M processor.
    ///
    UINT32    AddressParityEnable         : 1;
    UINT32    Reserved2                   : 2;
    ///
    /// [Bit 7] BINIT# Driver Enable (R) 1 = Enabled; 0 = Disabled Always 0 on
    /// the Pentium M processor.
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
    /// [Bit 10] MCERR# Observation Enabled (R/O) 1 = Enabled; 0 = Disabled
    /// Always 0 on the Pentium M processor.
    ///
    UINT32    MCERR_ObservationEnabled    : 1;
    UINT32    Reserved3                   : 1;
    ///
    /// [Bit 12] BINIT# Observation Enabled (R/O) 1 = Enabled; 0 = Disabled
    /// Always 0 on the Pentium M processor.
    ///
    UINT32    BINIT_ObservationEnabled    : 1;
    UINT32    Reserved4                   : 1;
    ///
    /// [Bit 14] 1 MByte Power on Reset Vector (R/O) 1 = 1 MByte; 0 = 4 GBytes
    /// Always 0 on the Pentium M processor.
    ///
    UINT32    ResetVector                 : 1;
    UINT32    Reserved5                   : 1;
    ///
    /// [Bits 17:16] APIC Cluster ID (R/O) Always 00B on the Pentium M
    /// processor.
    ///
    UINT32    APICClusterID               : 2;
    ///
    /// [Bit 18] System Bus Frequency (R/O) 1. = 100 MHz 2. = Reserved Always
    /// 0 on the Pentium M processor.
    ///
    UINT32    SystemBusFrequency          : 1;
    UINT32    Reserved6                   : 1;
    ///
    /// [Bits 21:20] Symmetric Arbitration ID (R/O) Always 00B on the Pentium
    /// M processor.
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
} MSR_PENTIUM_M_EBL_CR_POWERON_REGISTER;

/**
  Last Branch Record n (R/W) One of 8 last branch record registers on the last
  branch record stack: bits 31-0 hold the 'from' address and bits 63-32 hold
  the to address. See also: -  Last Branch Record Stack TOS at 1C9H -  Section
  17.15, "Last Branch, Interrupt, and Exception Recording (Pentium M
  Processors)".

  @param  ECX  MSR_PENTIUM_M_LASTBRANCH_n
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_M_LASTBRANCH_0);
  AsmWriteMsr64 (MSR_PENTIUM_M_LASTBRANCH_0, Msr);
  @endcode
  @note MSR_PENTIUM_M_LASTBRANCH_0 is defined as MSR_LASTBRANCH_0 in SDM.
        MSR_PENTIUM_M_LASTBRANCH_1 is defined as MSR_LASTBRANCH_1 in SDM.
        MSR_PENTIUM_M_LASTBRANCH_2 is defined as MSR_LASTBRANCH_2 in SDM.
        MSR_PENTIUM_M_LASTBRANCH_3 is defined as MSR_LASTBRANCH_3 in SDM.
        MSR_PENTIUM_M_LASTBRANCH_4 is defined as MSR_LASTBRANCH_4 in SDM.
        MSR_PENTIUM_M_LASTBRANCH_5 is defined as MSR_LASTBRANCH_5 in SDM.
        MSR_PENTIUM_M_LASTBRANCH_6 is defined as MSR_LASTBRANCH_6 in SDM.
        MSR_PENTIUM_M_LASTBRANCH_7 is defined as MSR_LASTBRANCH_7 in SDM.
  @{
**/
#define MSR_PENTIUM_M_LASTBRANCH_0  0x00000040
#define MSR_PENTIUM_M_LASTBRANCH_1  0x00000041
#define MSR_PENTIUM_M_LASTBRANCH_2  0x00000042
#define MSR_PENTIUM_M_LASTBRANCH_3  0x00000043
#define MSR_PENTIUM_M_LASTBRANCH_4  0x00000044
#define MSR_PENTIUM_M_LASTBRANCH_5  0x00000045
#define MSR_PENTIUM_M_LASTBRANCH_6  0x00000046
#define MSR_PENTIUM_M_LASTBRANCH_7  0x00000047
/// @}

/**
  Reserved.

  @param  ECX  MSR_PENTIUM_M_BBL_CR_CTL (0x00000119)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_M_BBL_CR_CTL);
  AsmWriteMsr64 (MSR_PENTIUM_M_BBL_CR_CTL, Msr);
  @endcode
  @note MSR_PENTIUM_M_BBL_CR_CTL is defined as MSR_BBL_CR_CTL in SDM.
**/
#define MSR_PENTIUM_M_BBL_CR_CTL  0x00000119

/**


  @param  ECX  MSR_PENTIUM_M_BBL_CR_CTL3 (0x0000011E)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_PENTIUM_M_BBL_CR_CTL3_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_PENTIUM_M_BBL_CR_CTL3_REGISTER.

  <b>Example usage</b>
  @code
  MSR_PENTIUM_M_BBL_CR_CTL3_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_PENTIUM_M_BBL_CR_CTL3);
  AsmWriteMsr64 (MSR_PENTIUM_M_BBL_CR_CTL3, Msr.Uint64);
  @endcode
  @note MSR_PENTIUM_M_BBL_CR_CTL3 is defined as MSR_BBL_CR_CTL3 in SDM.
**/
#define MSR_PENTIUM_M_BBL_CR_CTL3  0x0000011E

/**
  MSR information returned for MSR index #MSR_PENTIUM_M_BBL_CR_CTL3
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
    UINT32    Reserved1         : 4;
    ///
    /// [Bit 5] ECC Check Enable (RO) This bit enables ECC checking on the
    /// cache data bus. ECC is always generated on write cycles. 1. = Disabled
    /// (default) 2. = Enabled For the Pentium M processor, ECC checking on
    /// the cache data bus is always enabled.
    ///
    UINT32    ECCCheckEnable    : 1;
    UINT32    Reserved2         : 2;
    ///
    /// [Bit 8] L2 Enabled (R/W)  1 = L2 cache has been initialized 0 =
    /// Disabled (default) Until this bit is set the processor will not
    /// respond to the WBINVD instruction or the assertion of the FLUSH# input.
    ///
    UINT32    L2Enabled         : 1;
    UINT32    Reserved3         : 14;
    ///
    /// [Bit 23] L2 Not Present (RO)  1. = L2 Present 2. = L2 Not Present.
    ///
    UINT32    L2NotPresent      : 1;
    UINT32    Reserved4         : 8;
    UINT32    Reserved5         : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_PENTIUM_M_BBL_CR_CTL3_REGISTER;

/**


  @param  ECX  MSR_PENTIUM_M_THERM2_CTL (0x0000019D)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_PENTIUM_M_THERM2_CTL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_PENTIUM_M_THERM2_CTL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_PENTIUM_M_THERM2_CTL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_PENTIUM_M_THERM2_CTL);
  AsmWriteMsr64 (MSR_PENTIUM_M_THERM2_CTL, Msr.Uint64);
  @endcode
  @note MSR_PENTIUM_M_THERM2_CTL is defined as MSR_THERM2_CTL in SDM.
**/
#define MSR_PENTIUM_M_THERM2_CTL  0x0000019D

/**
  MSR information returned for MSR index #MSR_PENTIUM_M_THERM2_CTL
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
} MSR_PENTIUM_M_THERM2_CTL_REGISTER;

/**
  Enable Miscellaneous Processor Features (R/W) Allows a variety of processor
  functions to be enabled and disabled.

  @param  ECX  MSR_PENTIUM_M_IA32_MISC_ENABLE (0x000001A0)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_PENTIUM_M_IA32_MISC_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_PENTIUM_M_IA32_MISC_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_PENTIUM_M_IA32_MISC_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_PENTIUM_M_IA32_MISC_ENABLE);
  AsmWriteMsr64 (MSR_PENTIUM_M_IA32_MISC_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_PENTIUM_M_IA32_MISC_ENABLE is defined as IA32_MISC_ENABLE in SDM.
**/
#define MSR_PENTIUM_M_IA32_MISC_ENABLE  0x000001A0

/**
  MSR information returned for MSR index #MSR_PENTIUM_M_IA32_MISC_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1 : 3;
    ///
    /// [Bit 3] Automatic Thermal Control Circuit Enable (R/W)  1 = Setting
    /// this bit enables the thermal control circuit (TCC) portion of the
    /// Intel Thermal Monitor feature. This allows processor clocks to be
    /// automatically modulated based on the processor's thermal sensor
    /// operation. 0 = Disabled (default). The automatic thermal control
    /// circuit enable bit determines if the thermal control circuit (TCC)
    /// will be activated when the processor's internal thermal sensor
    /// determines the processor is about to exceed its maximum operating
    /// temperature. When the TCC is activated and TM1 is enabled, the
    /// processors clocks will be forced to a 50% duty cycle. BIOS must enable
    /// this feature. The bit should not be confused with the on-demand
    /// thermal control circuit enable bit.
    ///
    UINT32    AutomaticThermalControlCircuit : 1;
    UINT32    Reserved2                      : 3;
    ///
    /// [Bit 7] Performance Monitoring Available (R)  1 = Performance
    /// monitoring enabled 0 = Performance monitoring disabled.
    ///
    UINT32    PerformanceMonitoring          : 1;
    UINT32    Reserved3                      : 2;
    ///
    /// [Bit 10] FERR# Multiplexing Enable (R/W) 1 = FERR# asserted by the
    /// processor to indicate a pending break event within the processor 0 =
    /// Indicates compatible FERR# signaling behavior This bit must be set to
    /// 1 to support XAPIC interrupt model usage.
    ///   **Branch Trace Storage Unavailable (RO)** 1 = Processor doesn't
    ///   support branch trace storage (BTS) 0 = BTS is supported
    ///
    UINT32    FERR                 : 1;
    ///
    /// [Bit 11] Branch Trace Storage Unavailable (RO)
    /// 1 = Processor doesn't support branch trace storage (BTS)
    /// 0 = BTS is supported
    ///
    UINT32    BTS                  : 1;
    ///
    /// [Bit 12] Processor Event Based Sampling Unavailable (RO)  1 =
    /// Processor does not support processor event based sampling (PEBS); 0 =
    /// PEBS is supported. The Pentium M processor does not support PEBS.
    ///
    UINT32    PEBS                 : 1;
    UINT32    Reserved5            : 3;
    ///
    /// [Bit 16] Enhanced Intel SpeedStep Technology Enable (R/W)  1 =
    /// Enhanced Intel SpeedStep Technology enabled. On the Pentium M
    /// processor, this bit may be configured to be read-only.
    ///
    UINT32    EIST                 : 1;
    UINT32    Reserved6            : 6;
    ///
    /// [Bit 23] xTPR Message Disable (R/W) When set to 1, xTPR messages are
    /// disabled. xTPR messages are optional messages that allow the processor
    /// to inform the chipset of its priority. The default is processor
    /// specific.
    ///
    UINT32    xTPR_Message_Disable : 1;
    UINT32    Reserved7            : 8;
    UINT32    Reserved8            : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_PENTIUM_M_IA32_MISC_ENABLE_REGISTER;

/**
  Last Branch Record Stack TOS (R/W)  Contains an index (bits 0-3) that points
  to the MSR containing the most recent branch record. See also: -
  MSR_LASTBRANCH_0_FROM_IP (at 40H) -  Section 17.13, "Last Branch, Interrupt,
  and Exception Recording (Pentium M Processors)".

  @param  ECX  MSR_PENTIUM_M_LASTBRANCH_TOS (0x000001C9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_M_LASTBRANCH_TOS);
  AsmWriteMsr64 (MSR_PENTIUM_M_LASTBRANCH_TOS, Msr);
  @endcode
  @note MSR_PENTIUM_M_LASTBRANCH_TOS is defined as MSR_LASTBRANCH_TOS in SDM.
**/
#define MSR_PENTIUM_M_LASTBRANCH_TOS  0x000001C9

/**
  Debug Control (R/W)  Controls how several debug features are used. Bit
  definitions are discussed in the referenced section. See Section 17.15,
  "Last Branch, Interrupt, and Exception Recording (Pentium M Processors).".

  @param  ECX  MSR_PENTIUM_M_DEBUGCTLB (0x000001D9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_M_DEBUGCTLB);
  AsmWriteMsr64 (MSR_PENTIUM_M_DEBUGCTLB, Msr);
  @endcode
  @note MSR_PENTIUM_M_DEBUGCTLB is defined as MSR_DEBUGCTLB in SDM.
**/
#define MSR_PENTIUM_M_DEBUGCTLB  0x000001D9

/**
  Last Exception Record To Linear IP (R)  This area contains a pointer to the
  target of the last branch instruction that the processor executed prior to
  the last exception that was generated or the last interrupt that was
  handled. See Section 17.15, "Last Branch, Interrupt, and Exception Recording
  (Pentium M Processors)" and Section 17.16.2, "Last Branch and Last Exception
  MSRs.".

  @param  ECX  MSR_PENTIUM_M_LER_TO_LIP (0x000001DD)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_M_LER_TO_LIP);
  @endcode
  @note MSR_PENTIUM_M_LER_TO_LIP is defined as MSR_LER_TO_LIP in SDM.
**/
#define MSR_PENTIUM_M_LER_TO_LIP  0x000001DD

/**
  Last Exception Record From Linear IP (R)  Contains a pointer to the last
  branch instruction that the processor executed prior to the last exception
  that was generated or the last interrupt that was handled. See Section
  17.15, "Last Branch, Interrupt, and Exception Recording (Pentium M
  Processors)" and Section 17.16.2, "Last Branch and Last Exception MSRs.".

  @param  ECX  MSR_PENTIUM_M_LER_FROM_LIP (0x000001DE)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_M_LER_FROM_LIP);
  @endcode
  @note MSR_PENTIUM_M_LER_FROM_LIP is defined as MSR_LER_FROM_LIP in SDM.
**/
#define MSR_PENTIUM_M_LER_FROM_LIP  0x000001DE

/**
  See Section 15.3.2.1, "IA32_MCi_CTL MSRs.".

  @param  ECX  MSR_PENTIUM_M_MC4_CTL (0x0000040C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_M_MC4_CTL);
  AsmWriteMsr64 (MSR_PENTIUM_M_MC4_CTL, Msr);
  @endcode
  @note MSR_PENTIUM_M_MC4_CTL is defined as MSR_MC4_CTL in SDM.
**/
#define MSR_PENTIUM_M_MC4_CTL  0x0000040C

/**
  See Section 15.3.2.2, "IA32_MCi_STATUS MSRS.".

  @param  ECX  MSR_PENTIUM_M_MC4_STATUS (0x0000040D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_M_MC4_STATUS);
  AsmWriteMsr64 (MSR_PENTIUM_M_MC4_STATUS, Msr);
  @endcode
  @note MSR_PENTIUM_M_MC4_STATUS is defined as MSR_MC4_STATUS in SDM.
**/
#define MSR_PENTIUM_M_MC4_STATUS  0x0000040D

/**
  See Section 15.3.2.3, "IA32_MCi_ADDR MSRs." The MSR_MC4_ADDR register is
  either not implemented or contains no address if the ADDRV flag in the
  MSR_MC4_STATUS register is clear. When not implemented in the processor, all
  reads and writes to this MSR will cause a general-protection exception.

  @param  ECX  MSR_PENTIUM_M_MC4_ADDR (0x0000040E)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_M_MC4_ADDR);
  AsmWriteMsr64 (MSR_PENTIUM_M_MC4_ADDR, Msr);
  @endcode
  @note MSR_PENTIUM_M_MC4_ADDR is defined as MSR_MC4_ADDR in SDM.
**/
#define MSR_PENTIUM_M_MC4_ADDR  0x0000040E

/**
  See Section 15.3.2.1, "IA32_MCi_CTL MSRs.".

  @param  ECX  MSR_PENTIUM_M_MC3_CTL (0x00000410)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_M_MC3_CTL);
  AsmWriteMsr64 (MSR_PENTIUM_M_MC3_CTL, Msr);
  @endcode
  @note MSR_PENTIUM_M_MC3_CTL is defined as MSR_MC3_CTL in SDM.
**/
#define MSR_PENTIUM_M_MC3_CTL  0x00000410

/**
  See Section 15.3.2.2, "IA32_MCi_STATUS MSRS.".

  @param  ECX  MSR_PENTIUM_M_MC3_STATUS (0x00000411)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_M_MC3_STATUS);
  AsmWriteMsr64 (MSR_PENTIUM_M_MC3_STATUS, Msr);
  @endcode
  @note MSR_PENTIUM_M_MC3_STATUS is defined as MSR_MC3_STATUS in SDM.
**/
#define MSR_PENTIUM_M_MC3_STATUS  0x00000411

/**
  See Section 15.3.2.3, "IA32_MCi_ADDR MSRs." The MSR_MC3_ADDR register is
  either not implemented or contains no address if the ADDRV flag in the
  MSR_MC3_STATUS register is clear. When not implemented in the processor, all
  reads and writes to this MSR will cause a general-protection exception.

  @param  ECX  MSR_PENTIUM_M_MC3_ADDR (0x00000412)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_PENTIUM_M_MC3_ADDR);
  AsmWriteMsr64 (MSR_PENTIUM_M_MC3_ADDR, Msr);
  @endcode
  @note MSR_PENTIUM_M_MC3_ADDR is defined as MSR_MC3_ADDR in SDM.
**/
#define MSR_PENTIUM_M_MC3_ADDR  0x00000412

#endif
