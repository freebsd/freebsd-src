/** @file
  MSR Definitions for Intel processors based on the Sandy Bridge microarchitecture.

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

#ifndef __SANDY_BRIDGE_MSR_H__
#define __SANDY_BRIDGE_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>

/**
  Is Intel processors based on the Sandy Bridge microarchitecture?

  @param   DisplayFamily  Display Family ID
  @param   DisplayModel   Display Model ID

  @retval  TRUE   Yes, it is.
  @retval  FALSE  No, it isn't.
**/
#define IS_SANDY_BRIDGE_PROCESSOR(DisplayFamily, DisplayModel) \
  (DisplayFamily == 0x06 && \
   (                        \
    DisplayModel == 0x2A || \
    DisplayModel == 0x2D    \
    )                       \
   )

/**
  Thread. SMI Counter (R/O).

  @param  ECX  MSR_SANDY_BRIDGE_SMI_COUNT (0x00000034)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_SMI_COUNT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_SMI_COUNT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_SMI_COUNT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_SMI_COUNT);
  @endcode
  @note MSR_SANDY_BRIDGE_SMI_COUNT is defined as MSR_SMI_COUNT in SDM.
**/
#define MSR_SANDY_BRIDGE_SMI_COUNT               0x00000034

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_SMI_COUNT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 31:0] SMI Count (R/O) Count SMIs.
    ///
    UINT32  SMICount:32;
    UINT32  Reserved:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_SMI_COUNT_REGISTER;


/**
  Package. Platform Information Contains power management and other model
  specific features enumeration. See http://biosbits.org.

  @param  ECX  MSR_SANDY_BRIDGE_PLATFORM_INFO (0x000000CE)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PLATFORM_INFO_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PLATFORM_INFO_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_PLATFORM_INFO_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_PLATFORM_INFO);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PLATFORM_INFO, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_PLATFORM_INFO is defined as MSR_PLATFORM_INFO in SDM.
**/
#define MSR_SANDY_BRIDGE_PLATFORM_INFO           0x000000CE

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_PLATFORM_INFO
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:8;
    ///
    /// [Bits 15:8] Package. Maximum Non-Turbo Ratio (R/O)  The is the ratio
    /// of the frequency that invariant TSC runs at. Frequency = ratio * 100
    /// MHz.
    ///
    UINT32  MaximumNonTurboRatio:8;
    UINT32  Reserved2:12;
    ///
    /// [Bit 28] Package. Programmable Ratio Limit for Turbo Mode (R/O)  When
    /// set to 1, indicates that Programmable Ratio Limits for Turbo mode is
    /// enabled, and when set to 0, indicates Programmable Ratio Limits for
    /// Turbo mode is disabled.
    ///
    UINT32  RatioLimit:1;
    ///
    /// [Bit 29] Package. Programmable TDP Limit for Turbo Mode (R/O)  When
    /// set to 1, indicates that TDP Limits for Turbo mode are programmable,
    /// and when set to 0, indicates TDP Limit for Turbo mode is not
    /// programmable.
    ///
    UINT32  TDPLimit:1;
    UINT32  Reserved3:2;
    UINT32  Reserved4:8;
    ///
    /// [Bits 47:40] Package. Maximum Efficiency Ratio (R/O)  The is the
    /// minimum ratio (maximum efficiency) that the processor can operates, in
    /// units of 100MHz.
    ///
    UINT32  MaximumEfficiencyRatio:8;
    UINT32  Reserved5:16;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_PLATFORM_INFO_REGISTER;


/**
  Core. C-State Configuration Control (R/W)  Note: C-state values are
  processor specific C-state code names, unrelated to MWAIT extension C-state
  parameters or ACPI CStates. See http://biosbits.org.

  @param  ECX  MSR_SANDY_BRIDGE_PKG_CST_CONFIG_CONTROL (0x000000E2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PKG_CST_CONFIG_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PKG_CST_CONFIG_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_PKG_CST_CONFIG_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_PKG_CST_CONFIG_CONTROL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PKG_CST_CONFIG_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_PKG_CST_CONFIG_CONTROL is defined as MSR_PKG_CST_CONFIG_CONTROL in SDM.
**/
#define MSR_SANDY_BRIDGE_PKG_CST_CONFIG_CONTROL  0x000000E2

/**
  MSR information returned for MSR index
  #MSR_SANDY_BRIDGE_PKG_CST_CONFIG_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 2:0] Package C-State Limit (R/W)  Specifies the lowest
    /// processor-specific C-state code name (consuming the least power). for
    /// the package. The default is set as factory-configured package C-state
    /// limit. The following C-state code name encodings are supported: 000b:
    /// C0/C1 (no package C-sate support) 001b: C2 010b: C6 no retention 011b:
    /// C6 retention 100b: C7 101b: C7s 111: No package C-state limit. Note:
    /// This field cannot be used to limit package C-state to C3.
    ///
    UINT32  Limit:3;
    UINT32  Reserved1:7;
    ///
    /// [Bit 10] I/O MWAIT Redirection Enable (R/W)  When set, will map
    /// IO_read instructions sent to IO register specified by
    /// MSR_PMG_IO_CAPTURE_BASE to MWAIT instructions.
    ///
    UINT32  IO_MWAIT:1;
    UINT32  Reserved2:4;
    ///
    /// [Bit 15] CFG Lock (R/WO)  When set, lock bits 15:0 of this register
    /// until next reset.
    ///
    UINT32  CFGLock:1;
    UINT32  Reserved3:9;
    ///
    /// [Bit 25] C3 state auto demotion enable (R/W)  When set, the processor
    /// will conditionally demote C6/C7 requests to C3 based on uncore
    /// auto-demote information.
    ///
    UINT32  C3AutoDemotion:1;
    ///
    /// [Bit 26] C1 state auto demotion enable (R/W)  When set, the processor
    /// will conditionally demote C3/C6/C7 requests to C1 based on uncore
    /// auto-demote information.
    ///
    UINT32  C1AutoDemotion:1;
    ///
    /// [Bit 27] Enable C3 undemotion (R/W)  When set, enables undemotion from
    /// demoted C3.
    ///
    UINT32  C3Undemotion:1;
    ///
    /// [Bit 28] Enable C1 undemotion (R/W)  When set, enables undemotion from
    /// demoted C1.
    ///
    UINT32  C1Undemotion:1;
    UINT32  Reserved4:3;
    UINT32  Reserved5:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_PKG_CST_CONFIG_CONTROL_REGISTER;


/**
  Core. Power Management IO Redirection in C-state (R/W) See
  http://biosbits.org.

  @param  ECX  MSR_SANDY_BRIDGE_PMG_IO_CAPTURE_BASE (0x000000E4)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PMG_IO_CAPTURE_BASE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PMG_IO_CAPTURE_BASE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_PMG_IO_CAPTURE_BASE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_PMG_IO_CAPTURE_BASE);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PMG_IO_CAPTURE_BASE, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_PMG_IO_CAPTURE_BASE is defined as MSR_PMG_IO_CAPTURE_BASE in SDM.
**/
#define MSR_SANDY_BRIDGE_PMG_IO_CAPTURE_BASE     0x000000E4

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_PMG_IO_CAPTURE_BASE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 15:0] LVL_2 Base Address (R/W)  Specifies the base address
    /// visible to software for IO redirection. If IO MWAIT Redirection is
    /// enabled, reads to this address will be consumed by the power
    /// management logic and decoded to MWAIT instructions. When IO port
    /// address redirection is enabled, this is the IO port address reported
    /// to the OS/software.
    ///
    UINT32  Lvl2Base:16;
    ///
    /// [Bits 18:16] C-state Range (R/W)  Specifies the encoding value of the
    /// maximum C-State code name to be included when IO read to MWAIT
    /// redirection is enabled by MSR_PKG_CST_CONFIG_CONTROL[bit10]: 000b - C3
    /// is the max C-State to include 001b - C6 is the max C-State to include
    /// 010b - C7 is the max C-State to include.
    ///
    UINT32  CStateRange:3;
    UINT32  Reserved1:13;
    UINT32  Reserved2:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_PMG_IO_CAPTURE_BASE_REGISTER;


/**
  Core. AES Configuration (RW-L) Privileged post-BIOS agent must provide a #GP
  handler to handle unsuccessful read of this MSR.

  @param  ECX  MSR_SANDY_BRIDGE_FEATURE_CONFIG (0x0000013C)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_FEATURE_CONFIG_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_FEATURE_CONFIG_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_FEATURE_CONFIG_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_FEATURE_CONFIG);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_FEATURE_CONFIG, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_FEATURE_CONFIG is defined as MSR_FEATURE_CONFIG in SDM.
**/
#define MSR_SANDY_BRIDGE_FEATURE_CONFIG          0x0000013C

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_FEATURE_CONFIG
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 1:0] AES Configuration (RW-L)  Upon a successful read of this
    /// MSR, the configuration of AES instruction set availability is as
    /// follows: 11b: AES instructions are not available until next RESET.
    /// otherwise, AES instructions are available. Note, AES instruction set
    /// is not available if read is unsuccessful. If the configuration is not
    /// 01b, AES instruction can be mis-configured if a privileged agent
    /// unintentionally writes 11b.
    ///
    UINT32  AESConfiguration:2;
    UINT32  Reserved1:30;
    UINT32  Reserved2:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_FEATURE_CONFIG_REGISTER;


/**
  Core. See Table 2-2. If CPUID.0AH:EAX[15:8] = 8.

  @param  ECX  MSR_SANDY_BRIDGE_IA32_PERFEVTSELn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_IA32_PERFEVTSEL4);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_IA32_PERFEVTSEL4, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_IA32_PERFEVTSEL4 is defined as IA32_PERFEVTSEL4 in SDM.
        MSR_SANDY_BRIDGE_IA32_PERFEVTSEL5 is defined as IA32_PERFEVTSEL5 in SDM.
        MSR_SANDY_BRIDGE_IA32_PERFEVTSEL6 is defined as IA32_PERFEVTSEL6 in SDM.
        MSR_SANDY_BRIDGE_IA32_PERFEVTSEL7 is defined as IA32_PERFEVTSEL7 in SDM.
  @{
**/
#define MSR_SANDY_BRIDGE_IA32_PERFEVTSEL4        0x0000018A
#define MSR_SANDY_BRIDGE_IA32_PERFEVTSEL5        0x0000018B
#define MSR_SANDY_BRIDGE_IA32_PERFEVTSEL6        0x0000018C
#define MSR_SANDY_BRIDGE_IA32_PERFEVTSEL7        0x0000018D
/// @}


/**
  Package.

  @param  ECX  MSR_SANDY_BRIDGE_PERF_STATUS (0x00000198)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PERF_STATUS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PERF_STATUS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_PERF_STATUS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_PERF_STATUS);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PERF_STATUS, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_PERF_STATUS is defined as MSR_PERF_STATUS in SDM.
**/
#define MSR_SANDY_BRIDGE_PERF_STATUS             0x00000198

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_PERF_STATUS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:32;
    ///
    /// [Bits 47:32] Core Voltage (R/O) P-state core voltage can be computed
    /// by MSR_PERF_STATUS[37:32] * (float) 1/(2^13).
    ///
    UINT32  CoreVoltage:16;
    UINT32  Reserved2:16;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_PERF_STATUS_REGISTER;


/**
  Thread. Clock Modulation (R/W) See Table 2-2. IA32_CLOCK_MODULATION MSR was
  originally named IA32_THERM_CONTROL MSR.

  @param  ECX  MSR_SANDY_BRIDGE_IA32_CLOCK_MODULATION (0x0000019A)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_IA32_CLOCK_MODULATION_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_IA32_CLOCK_MODULATION_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_IA32_CLOCK_MODULATION_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_IA32_CLOCK_MODULATION);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_IA32_CLOCK_MODULATION, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_IA32_CLOCK_MODULATION is defined as IA32_CLOCK_MODULATION in SDM.
**/
#define MSR_SANDY_BRIDGE_IA32_CLOCK_MODULATION   0x0000019A

/**
  MSR information returned for MSR index
  #MSR_SANDY_BRIDGE_IA32_CLOCK_MODULATION
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 3:0] On demand Clock Modulation Duty Cycle (R/W) In 6.25%
    /// increment.
    ///
    UINT32  OnDemandClockModulationDutyCycle:4;
    ///
    /// [Bit 4] On demand Clock Modulation Enable (R/W).
    ///
    UINT32  OnDemandClockModulationEnable:1;
    UINT32  Reserved1:27;
    UINT32  Reserved2:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_IA32_CLOCK_MODULATION_REGISTER;


/**
  Enable Misc. Processor Features (R/W)  Allows a variety of processor
  functions to be enabled and disabled.

  @param  ECX  MSR_SANDY_BRIDGE_IA32_MISC_ENABLE (0x000001A0)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_IA32_MISC_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_IA32_MISC_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_IA32_MISC_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_IA32_MISC_ENABLE);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_IA32_MISC_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_IA32_MISC_ENABLE is defined as IA32_MISC_ENABLE in SDM.
**/
#define MSR_SANDY_BRIDGE_IA32_MISC_ENABLE        0x000001A0

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_IA32_MISC_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Thread. Fast-Strings Enable See Table 2-2.
    ///
    UINT32  FastStrings:1;
    UINT32  Reserved1:6;
    ///
    /// [Bit 7] Thread. Performance Monitoring Available (R) See Table 2-2.
    ///
    UINT32  PerformanceMonitoring:1;
    UINT32  Reserved2:3;
    ///
    /// [Bit 11] Thread. Branch Trace Storage Unavailable (RO) See Table 2-2.
    ///
    UINT32  BTS:1;
    ///
    /// [Bit 12] Thread. Processor Event Based Sampling Unavailable (RO) See
    /// Table 2-2.
    ///
    UINT32  PEBS:1;
    UINT32  Reserved3:3;
    ///
    /// [Bit 16] Package. Enhanced Intel SpeedStep Technology Enable (R/W) See
    /// Table 2-2.
    ///
    UINT32  EIST:1;
    UINT32  Reserved4:1;
    ///
    /// [Bit 18] Thread. ENABLE MONITOR FSM (R/W) See Table 2-2.
    ///
    UINT32  MONITOR:1;
    UINT32  Reserved5:3;
    ///
    /// [Bit 22] Thread. Limit CPUID Maxval (R/W) See Table 2-2.
    ///
    UINT32  LimitCpuidMaxval:1;
    ///
    /// [Bit 23] Thread. xTPR Message Disable (R/W) See Table 2-2.
    ///
    UINT32  xTPR_Message_Disable:1;
    UINT32  Reserved6:8;
    UINT32  Reserved7:2;
    ///
    /// [Bit 34] Thread. XD Bit Disable (R/W) See Table 2-2.
    ///
    UINT32  XD:1;
    UINT32  Reserved8:3;
    ///
    /// [Bit 38] Package. Turbo Mode Disable (R/W) When set to 1 on processors
    /// that support Intel Turbo Boost Technology, the turbo mode feature is
    /// disabled and the IDA_Enable feature flag will be clear (CPUID.06H:
    /// EAX[1]=0). When set to a 0 on processors that support IDA, CPUID.06H:
    /// EAX[1] reports the processor's support of turbo mode is enabled. Note:
    /// the power-on default value is used by BIOS to detect hardware support
    /// of turbo mode. If power-on default value is 1, turbo mode is available
    /// in the processor. If power-on default value is 0, turbo mode is not
    /// available.
    ///
    UINT32  TurboModeDisable:1;
    UINT32  Reserved9:25;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_IA32_MISC_ENABLE_REGISTER;


/**
  Unique.

  @param  ECX  MSR_SANDY_BRIDGE_TEMPERATURE_TARGET (0x000001A2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_TEMPERATURE_TARGET_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_TEMPERATURE_TARGET_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_TEMPERATURE_TARGET_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_TEMPERATURE_TARGET);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_TEMPERATURE_TARGET, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_TEMPERATURE_TARGET is defined as MSR_TEMPERATURE_TARGET in SDM.
**/
#define MSR_SANDY_BRIDGE_TEMPERATURE_TARGET      0x000001A2

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_TEMPERATURE_TARGET
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:16;
    ///
    /// [Bits 23:16] Temperature Target (R)  The minimum temperature at which
    /// PROCHOT# will be asserted. The value is degree C.
    ///
    UINT32  TemperatureTarget:8;
    UINT32  Reserved2:8;
    UINT32  Reserved3:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_TEMPERATURE_TARGET_REGISTER;


/**
  Miscellaneous Feature Control (R/W).

  @param  ECX  MSR_SANDY_BRIDGE_MISC_FEATURE_CONTROL (0x000001A4)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_MISC_FEATURE_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_MISC_FEATURE_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_MISC_FEATURE_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_MISC_FEATURE_CONTROL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_MISC_FEATURE_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_MISC_FEATURE_CONTROL is defined as MSR_MISC_FEATURE_CONTROL in SDM.
**/
#define MSR_SANDY_BRIDGE_MISC_FEATURE_CONTROL    0x000001A4

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_MISC_FEATURE_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Core. L2 Hardware Prefetcher Disable (R/W)  If 1, disables the
    /// L2 hardware prefetcher, which fetches additional lines of code or data
    /// into the L2 cache.
    ///
    UINT32  L2HardwarePrefetcherDisable:1;
    ///
    /// [Bit 1] Core. L2 Adjacent Cache Line Prefetcher Disable (R/W)  If 1,
    /// disables the adjacent cache line prefetcher, which fetches the cache
    /// line that comprises a cache line pair (128 bytes).
    ///
    UINT32  L2AdjacentCacheLinePrefetcherDisable:1;
    ///
    /// [Bit 2] Core. DCU Hardware Prefetcher Disable (R/W)  If 1, disables
    /// the L1 data cache prefetcher, which fetches the next cache line into
    /// L1 data cache.
    ///
    UINT32  DCUHardwarePrefetcherDisable:1;
    ///
    /// [Bit 3] Core. DCU IP Prefetcher Disable (R/W)  If 1, disables the L1
    /// data cache IP prefetcher, which uses sequential load history (based on
    /// instruction Pointer of previous loads) to determine whether to
    /// prefetch additional lines.
    ///
    UINT32  DCUIPPrefetcherDisable:1;
    UINT32  Reserved1:28;
    UINT32  Reserved2:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_MISC_FEATURE_CONTROL_REGISTER;


/**
  Thread. Offcore Response Event Select Register (R/W).

  @param  ECX  MSR_SANDY_BRIDGE_OFFCORE_RSP_0 (0x000001A6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_OFFCORE_RSP_0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_OFFCORE_RSP_0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_OFFCORE_RSP_0 is defined as MSR_OFFCORE_RSP_0 in SDM.
**/
#define MSR_SANDY_BRIDGE_OFFCORE_RSP_0           0x000001A6


/**
  Thread. Offcore Response Event Select Register (R/W).

  @param  ECX  MSR_SANDY_BRIDGE_OFFCORE_RSP_1 (0x000001A7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_OFFCORE_RSP_1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_OFFCORE_RSP_1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_OFFCORE_RSP_1 is defined as MSR_OFFCORE_RSP_1 in SDM.
**/
#define MSR_SANDY_BRIDGE_OFFCORE_RSP_1           0x000001A7


/**
  See http://biosbits.org.

  @param  ECX  MSR_SANDY_BRIDGE_MISC_PWR_MGMT (0x000001AA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_MISC_PWR_MGMT);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_MISC_PWR_MGMT, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_MISC_PWR_MGMT is defined as MSR_MISC_PWR_MGMT in SDM.
**/
#define MSR_SANDY_BRIDGE_MISC_PWR_MGMT           0x000001AA


/**
  Thread. Last Branch Record Filtering Select Register (R/W) See Section
  17.9.2, "Filtering of Last Branch Records.".

  @param  ECX  MSR_SANDY_BRIDGE_LBR_SELECT (0x000001C8)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_LBR_SELECT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_LBR_SELECT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_LBR_SELECT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_LBR_SELECT);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_LBR_SELECT, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_LBR_SELECT is defined as MSR_LBR_SELECT in SDM.
**/
#define MSR_SANDY_BRIDGE_LBR_SELECT              0x000001C8

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_LBR_SELECT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] CPL_EQ_0.
    ///
    UINT32  CPL_EQ_0:1;
    ///
    /// [Bit 1] CPL_NEQ_0.
    ///
    UINT32  CPL_NEQ_0:1;
    ///
    /// [Bit 2] JCC.
    ///
    UINT32  JCC:1;
    ///
    /// [Bit 3] NEAR_REL_CALL.
    ///
    UINT32  NEAR_REL_CALL:1;
    ///
    /// [Bit 4] NEAR_IND_CALL.
    ///
    UINT32  NEAR_IND_CALL:1;
    ///
    /// [Bit 5] NEAR_RET.
    ///
    UINT32  NEAR_RET:1;
    ///
    /// [Bit 6] NEAR_IND_JMP.
    ///
    UINT32  NEAR_IND_JMP:1;
    ///
    /// [Bit 7] NEAR_REL_JMP.
    ///
    UINT32  NEAR_REL_JMP:1;
    ///
    /// [Bit 8] FAR_BRANCH.
    ///
    UINT32  FAR_BRANCH:1;
    UINT32  Reserved1:23;
    UINT32  Reserved2:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_LBR_SELECT_REGISTER;


/**
  Thread. Last Branch Record Stack TOS (R/W)  Contains an index (bits 0-3)
  that points to the MSR containing the most recent branch record. See
  MSR_LASTBRANCH_0_FROM_IP (at 680H).

  @param  ECX  MSR_SANDY_BRIDGE_LASTBRANCH_TOS (0x000001C9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_LASTBRANCH_TOS);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_LASTBRANCH_TOS, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_LASTBRANCH_TOS is defined as MSR_LASTBRANCH_TOS in SDM.
**/
#define MSR_SANDY_BRIDGE_LASTBRANCH_TOS          0x000001C9


/**
  Thread. Last Exception Record From Linear IP (R)  Contains a pointer to the
  last branch instruction that the processor executed prior to the last
  exception that was generated or the last interrupt that was handled.

  @param  ECX  MSR_SANDY_BRIDGE_LER_FROM_LIP (0x000001DD)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_LER_FROM_LIP);
  @endcode
  @note MSR_SANDY_BRIDGE_LER_FROM_LIP is defined as MSR_LER_FROM_LIP in SDM.
**/
#define MSR_SANDY_BRIDGE_LER_FROM_LIP            0x000001DD


/**
  Thread. Last Exception Record To Linear IP (R)  This area contains a pointer
  to the target of the last branch instruction that the processor executed
  prior to the last exception that was generated or the last interrupt that
  was handled.

  @param  ECX  MSR_SANDY_BRIDGE_LER_TO_LIP (0x000001DE)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_LER_TO_LIP);
  @endcode
  @note MSR_SANDY_BRIDGE_LER_TO_LIP is defined as MSR_LER_TO_LIP in SDM.
**/
#define MSR_SANDY_BRIDGE_LER_TO_LIP              0x000001DE


/**
  Core. See http://biosbits.org.

  @param  ECX  MSR_SANDY_BRIDGE_POWER_CTL (0x000001FC)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_POWER_CTL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_POWER_CTL, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_POWER_CTL is defined as MSR_POWER_CTL in SDM.
**/
#define MSR_SANDY_BRIDGE_POWER_CTL               0x000001FC


/**
  Package. Always 0 (CMCI not supported).

  @param  ECX  MSR_SANDY_BRIDGE_IA32_MC4_CTL2 (0x00000284)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_IA32_MC4_CTL2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_IA32_MC4_CTL2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_IA32_MC4_CTL2 is defined as IA32_MC4_CTL2 in SDM.
**/
#define MSR_SANDY_BRIDGE_IA32_MC4_CTL2           0x00000284


/**
  See Table 2-2. See Section 18.6.2.2, "Global Counter Control Facilities.".

  @param  ECX  MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_STATUS (0x0000038E)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_STATUS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_STATUS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_STATUS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_STATUS);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_STATUS, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_STATUS is defined as IA32_PERF_GLOBAL_STATUS in SDM.
**/
#define MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_STATUS 0x0000038E

/**
  MSR information returned for MSR index
  #MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_STATUS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Thread. Ovf_PMC0.
    ///
    UINT32  Ovf_PMC0:1;
    ///
    /// [Bit 1] Thread. Ovf_PMC1.
    ///
    UINT32  Ovf_PMC1:1;
    ///
    /// [Bit 2] Thread. Ovf_PMC2.
    ///
    UINT32  Ovf_PMC2:1;
    ///
    /// [Bit 3] Thread. Ovf_PMC3.
    ///
    UINT32  Ovf_PMC3:1;
    ///
    /// [Bit 4] Core. Ovf_PMC4 (if CPUID.0AH:EAX[15:8] > 4).
    ///
    UINT32  Ovf_PMC4:1;
    ///
    /// [Bit 5] Core. Ovf_PMC5 (if CPUID.0AH:EAX[15:8] > 5).
    ///
    UINT32  Ovf_PMC5:1;
    ///
    /// [Bit 6] Core. Ovf_PMC6 (if CPUID.0AH:EAX[15:8] > 6).
    ///
    UINT32  Ovf_PMC6:1;
    ///
    /// [Bit 7] Core. Ovf_PMC7 (if CPUID.0AH:EAX[15:8] > 7).
    ///
    UINT32  Ovf_PMC7:1;
    UINT32  Reserved1:24;
    ///
    /// [Bit 32] Thread. Ovf_FixedCtr0.
    ///
    UINT32  Ovf_FixedCtr0:1;
    ///
    /// [Bit 33] Thread. Ovf_FixedCtr1.
    ///
    UINT32  Ovf_FixedCtr1:1;
    ///
    /// [Bit 34] Thread. Ovf_FixedCtr2.
    ///
    UINT32  Ovf_FixedCtr2:1;
    UINT32  Reserved2:26;
    ///
    /// [Bit 61] Thread. Ovf_Uncore.
    ///
    UINT32  Ovf_Uncore:1;
    ///
    /// [Bit 62] Thread. Ovf_BufDSSAVE.
    ///
    UINT32  Ovf_BufDSSAVE:1;
    ///
    /// [Bit 63] Thread. CondChgd.
    ///
    UINT32  CondChgd:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_STATUS_REGISTER;


/**
  Thread. See Table 2-2. See Section 18.6.2.2, "Global Counter Control
  Facilities.".

  @param  ECX  MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_CTRL (0x0000038F)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_CTRL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_CTRL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_CTRL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_CTRL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_CTRL, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_CTRL is defined as IA32_PERF_GLOBAL_CTRL in SDM.
**/
#define MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_CTRL   0x0000038F

/**
  MSR information returned for MSR index
  #MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_CTRL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Thread. Set 1 to enable PMC0 to count.
    ///
    UINT32  PCM0_EN:1;
    ///
    /// [Bit 1] Thread. Set 1 to enable PMC1 to count.
    ///
    UINT32  PCM1_EN:1;
    ///
    /// [Bit 2] Thread. Set 1 to enable PMC2 to count.
    ///
    UINT32  PCM2_EN:1;
    ///
    /// [Bit 3] Thread. Set 1 to enable PMC3 to count.
    ///
    UINT32  PCM3_EN:1;
    ///
    /// [Bit 4] Core. Set 1 to enable PMC4 to count (if CPUID.0AH:EAX[15:8] >
    /// 4).
    ///
    UINT32  PCM4_EN:1;
    ///
    /// [Bit 5] Core. Set 1 to enable PMC5 to count (if CPUID.0AH:EAX[15:8] >
    /// 5).
    ///
    UINT32  PCM5_EN:1;
    ///
    /// [Bit 6] Core. Set 1 to enable PMC6 to count (if CPUID.0AH:EAX[15:8] >
    /// 6).
    ///
    UINT32  PCM6_EN:1;
    ///
    /// [Bit 7] Core. Set 1 to enable PMC7 to count (if CPUID.0AH:EAX[15:8] >
    /// 7).
    ///
    UINT32  PCM7_EN:1;
    UINT32  Reserved1:24;
    ///
    /// [Bit 32] Thread. Set 1 to enable FixedCtr0 to count.
    ///
    UINT32  FIXED_CTR0:1;
    ///
    /// [Bit 33] Thread. Set 1 to enable FixedCtr1 to count.
    ///
    UINT32  FIXED_CTR1:1;
    ///
    /// [Bit 34] Thread. Set 1 to enable FixedCtr2 to count.
    ///
    UINT32  FIXED_CTR2:1;
    UINT32  Reserved2:29;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_CTRL_REGISTER;


/**
  See Table 2-2. See Section 18.6.2.2, "Global Counter Control Facilities.".

  @param  ECX  MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_OVF_CTRL (0x00000390)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_OVF_CTRL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_OVF_CTRL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_OVF_CTRL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_OVF_CTRL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_OVF_CTRL, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_OVF_CTRL is defined as IA32_PERF_GLOBAL_OVF_CTRL in SDM.
**/
#define MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_OVF_CTRL 0x00000390

/**
  MSR information returned for MSR index
  #MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_OVF_CTRL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Thread. Set 1 to clear Ovf_PMC0.
    ///
    UINT32  Ovf_PMC0:1;
    ///
    /// [Bit 1] Thread. Set 1 to clear Ovf_PMC1.
    ///
    UINT32  Ovf_PMC1:1;
    ///
    /// [Bit 2] Thread. Set 1 to clear Ovf_PMC2.
    ///
    UINT32  Ovf_PMC2:1;
    ///
    /// [Bit 3] Thread. Set 1 to clear Ovf_PMC3.
    ///
    UINT32  Ovf_PMC3:1;
    ///
    /// [Bit 4] Core. Set 1 to clear Ovf_PMC4 (if CPUID.0AH:EAX[15:8] > 4).
    ///
    UINT32  Ovf_PMC4:1;
    ///
    /// [Bit 5] Core. Set 1 to clear Ovf_PMC5 (if CPUID.0AH:EAX[15:8] > 5).
    ///
    UINT32  Ovf_PMC5:1;
    ///
    /// [Bit 6] Core. Set 1 to clear Ovf_PMC6 (if CPUID.0AH:EAX[15:8] > 6).
    ///
    UINT32  Ovf_PMC6:1;
    ///
    /// [Bit 7] Core. Set 1 to clear Ovf_PMC7 (if CPUID.0AH:EAX[15:8] > 7).
    ///
    UINT32  Ovf_PMC7:1;
    UINT32  Reserved1:24;
    ///
    /// [Bit 32] Thread. Set 1 to clear Ovf_FixedCtr0.
    ///
    UINT32  Ovf_FixedCtr0:1;
    ///
    /// [Bit 33] Thread. Set 1 to clear Ovf_FixedCtr1.
    ///
    UINT32  Ovf_FixedCtr1:1;
    ///
    /// [Bit 34] Thread. Set 1 to clear Ovf_FixedCtr2.
    ///
    UINT32  Ovf_FixedCtr2:1;
    UINT32  Reserved2:26;
    ///
    /// [Bit 61] Thread. Set 1 to clear Ovf_Uncore.
    ///
    UINT32  Ovf_Uncore:1;
    ///
    /// [Bit 62] Thread. Set 1 to clear Ovf_BufDSSAVE.
    ///
    UINT32  Ovf_BufDSSAVE:1;
    ///
    /// [Bit 63] Thread. Set 1 to clear CondChgd.
    ///
    UINT32  CondChgd:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_IA32_PERF_GLOBAL_OVF_CTRL_REGISTER;


/**
  Thread. See Section 18.3.1.1.1, "Processor Event Based Sampling (PEBS).".

  @param  ECX  MSR_SANDY_BRIDGE_PEBS_ENABLE (0x000003F1)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PEBS_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PEBS_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_PEBS_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_PEBS_ENABLE);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PEBS_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_PEBS_ENABLE is defined as MSR_PEBS_ENABLE in SDM.
**/
#define MSR_SANDY_BRIDGE_PEBS_ENABLE             0x000003F1

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_PEBS_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Enable PEBS on IA32_PMC0. (R/W).
    ///
    UINT32  PEBS_EN_PMC0:1;
    ///
    /// [Bit 1] Enable PEBS on IA32_PMC1. (R/W).
    ///
    UINT32  PEBS_EN_PMC1:1;
    ///
    /// [Bit 2] Enable PEBS on IA32_PMC2. (R/W).
    ///
    UINT32  PEBS_EN_PMC2:1;
    ///
    /// [Bit 3] Enable PEBS on IA32_PMC3. (R/W).
    ///
    UINT32  PEBS_EN_PMC3:1;
    UINT32  Reserved1:28;
    ///
    /// [Bit 32] Enable Load Latency on IA32_PMC0. (R/W).
    ///
    UINT32  LL_EN_PMC0:1;
    ///
    /// [Bit 33] Enable Load Latency on IA32_PMC1. (R/W).
    ///
    UINT32  LL_EN_PMC1:1;
    ///
    /// [Bit 34] Enable Load Latency on IA32_PMC2. (R/W).
    ///
    UINT32  LL_EN_PMC2:1;
    ///
    /// [Bit 35] Enable Load Latency on IA32_PMC3. (R/W).
    ///
    UINT32  LL_EN_PMC3:1;
    UINT32  Reserved2:27;
    ///
    /// [Bit 63] Enable Precise Store. (R/W).
    ///
    UINT32  PS_EN:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_PEBS_ENABLE_REGISTER;


/**
  Thread. See Section 18.3.1.1.2, "Load Latency Performance Monitoring
  Facility.".

  @param  ECX  MSR_SANDY_BRIDGE_PEBS_LD_LAT (0x000003F6)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PEBS_LD_LAT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PEBS_LD_LAT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_PEBS_LD_LAT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_PEBS_LD_LAT);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PEBS_LD_LAT, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_PEBS_LD_LAT is defined as MSR_PEBS_LD_LAT in SDM.
**/
#define MSR_SANDY_BRIDGE_PEBS_LD_LAT             0x000003F6

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_PEBS_LD_LAT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 15:0] Minimum threshold latency value of tagged load operation
    /// that will be counted. (R/W).
    ///
    UINT32  MinimumThreshold:16;
    UINT32  Reserved1:16;
    UINT32  Reserved2:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_PEBS_LD_LAT_REGISTER;


/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. Package C3
  Residency Counter. (R/O) Value since last reset that this package is in
  processor-specific C3 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_SANDY_BRIDGE_PKG_C3_RESIDENCY (0x000003F8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PKG_C3_RESIDENCY);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PKG_C3_RESIDENCY, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PKG_C3_RESIDENCY is defined as MSR_PKG_C3_RESIDENCY in SDM.
**/
#define MSR_SANDY_BRIDGE_PKG_C3_RESIDENCY        0x000003F8


/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. Package C6
  Residency Counter. (R/O) Value since last reset that this package is in
  processor-specific C6 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_SANDY_BRIDGE_PKG_C6_RESIDENCY (0x000003F9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PKG_C6_RESIDENCY);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PKG_C6_RESIDENCY, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PKG_C6_RESIDENCY is defined as MSR_PKG_C6_RESIDENCY in SDM.
**/
#define MSR_SANDY_BRIDGE_PKG_C6_RESIDENCY        0x000003F9


/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. Package C7
  Residency Counter. (R/O) Value since last reset that this package is in
  processor-specific C7 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_SANDY_BRIDGE_PKG_C7_RESIDENCY (0x000003FA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PKG_C7_RESIDENCY);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PKG_C7_RESIDENCY, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PKG_C7_RESIDENCY is defined as MSR_PKG_C7_RESIDENCY in SDM.
**/
#define MSR_SANDY_BRIDGE_PKG_C7_RESIDENCY        0x000003FA


/**
  Core. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. CORE C3
  Residency Counter. (R/O) Value since last reset that this core is in
  processor-specific C3 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_SANDY_BRIDGE_CORE_C3_RESIDENCY (0x000003FC)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_CORE_C3_RESIDENCY);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_CORE_C3_RESIDENCY, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_CORE_C3_RESIDENCY is defined as MSR_CORE_C3_RESIDENCY in SDM.
**/
#define MSR_SANDY_BRIDGE_CORE_C3_RESIDENCY       0x000003FC


/**
  Core. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. CORE C6
  Residency Counter. (R/O) Value since last reset that this core is in
  processor-specific C6 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_SANDY_BRIDGE_CORE_C6_RESIDENCY (0x000003FD)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_CORE_C6_RESIDENCY);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_CORE_C6_RESIDENCY, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_CORE_C6_RESIDENCY is defined as MSR_CORE_C6_RESIDENCY in SDM.
**/
#define MSR_SANDY_BRIDGE_CORE_C6_RESIDENCY       0x000003FD


/**
  Core. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. CORE C7
  Residency Counter. (R/O) Value since last reset that this core is in
  processor-specific C7 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_SANDY_BRIDGE_CORE_C7_RESIDENCY (0x000003FE)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_CORE_C7_RESIDENCY);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_CORE_C7_RESIDENCY, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_CORE_C7_RESIDENCY is defined as MSR_CORE_C7_RESIDENCY in SDM.
**/
#define MSR_SANDY_BRIDGE_CORE_C7_RESIDENCY       0x000003FE


/**
  Core. See Section 15.3.2.1, "IA32_MCi_CTL MSRs.".

  @param  ECX  MSR_SANDY_BRIDGE_IA32_MC4_CTL (0x00000410)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_IA32_MC4_CTL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_IA32_MC4_CTL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_IA32_MC4_CTL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_IA32_MC4_CTL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_IA32_MC4_CTL, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_IA32_MC4_CTL is defined as IA32_MC4_CTL in SDM.
**/
#define MSR_SANDY_BRIDGE_IA32_MC4_CTL            0x00000410

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_IA32_MC4_CTL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] PCU Hardware Error (R/W)  When set, enables signaling of PCU
    /// hardware detected errors.
    ///
    UINT32  PCUHardwareError:1;
    ///
    /// [Bit 1] PCU Controller Error (R/W)  When set, enables signaling of PCU
    /// controller detected errors.
    ///
    UINT32  PCUControllerError:1;
    ///
    /// [Bit 2] PCU Firmware Error (R/W)  When set, enables signaling of PCU
    /// firmware detected errors.
    ///
    UINT32  PCUFirmwareError:1;
    UINT32  Reserved1:29;
    UINT32  Reserved2:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_IA32_MC4_CTL_REGISTER;


/**
  Thread. Capability Reporting Register of EPT and VPID (R/O) See Table 2-2.

  @param  ECX  MSR_SANDY_BRIDGE_IA32_VMX_EPT_VPID_ENUM (0x0000048C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_IA32_VMX_EPT_VPID_ENUM);
  @endcode
  @note MSR_SANDY_BRIDGE_IA32_VMX_EPT_VPID_ENUM is defined as IA32_VMX_EPT_VPID_ENUM in SDM.
**/
#define MSR_SANDY_BRIDGE_IA32_VMX_EPT_VPID_ENUM  0x0000048C


/**
  Package. Unit Multipliers used in RAPL Interfaces (R/O) See Section 14.9.1,
  "RAPL Interfaces.".

  @param  ECX  MSR_SANDY_BRIDGE_RAPL_POWER_UNIT (0x00000606)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_RAPL_POWER_UNIT);
  @endcode
  @note MSR_SANDY_BRIDGE_RAPL_POWER_UNIT is defined as MSR_RAPL_POWER_UNIT in SDM.
**/
#define MSR_SANDY_BRIDGE_RAPL_POWER_UNIT         0x00000606


/**
  Package. Package C3 Interrupt Response Limit (R/W)  Note: C-state values are
  processor specific C-state code names, unrelated to MWAIT extension C-state
  parameters or ACPI CStates.

  @param  ECX  MSR_SANDY_BRIDGE_PKGC3_IRTL (0x0000060A)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PKGC3_IRTL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PKGC3_IRTL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_PKGC3_IRTL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_PKGC3_IRTL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PKGC3_IRTL, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_PKGC3_IRTL is defined as MSR_PKGC3_IRTL in SDM.
**/
#define MSR_SANDY_BRIDGE_PKGC3_IRTL              0x0000060A

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_PKGC3_IRTL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 9:0] Interrupt response time limit (R/W)  Specifies the limit
    /// that should be used to decide if the package should be put into a
    /// package C3 state.
    ///
    UINT32  TimeLimit:10;
    ///
    /// [Bits 12:10] Time Unit (R/W)  Specifies the encoding value of time
    /// unit of the interrupt response time limit. The following time unit
    /// encodings are supported: 000b: 1 ns 001b: 32 ns 010b: 1024 ns 011b:
    /// 32768 ns 100b: 1048576 ns 101b: 33554432 ns.
    ///
    UINT32  TimeUnit:3;
    UINT32  Reserved1:2;
    ///
    /// [Bit 15] Valid (R/W)  Indicates whether the values in bits 12:0 are
    /// valid and can be used by the processor for package C-sate management.
    ///
    UINT32  Valid:1;
    UINT32  Reserved2:16;
    UINT32  Reserved3:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_PKGC3_IRTL_REGISTER;


/**
  Package. Package C6 Interrupt Response Limit (R/W)  This MSR defines the
  budget allocated for the package to exit from C6 to a C0 state, where
  interrupt request can be delivered to the core and serviced. Additional
  core-exit latency amy be applicable depending on the actual C-state the core
  is in. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates.

  @param  ECX  MSR_SANDY_BRIDGE_PKGC6_IRTL (0x0000060B)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PKGC6_IRTL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PKGC6_IRTL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_PKGC6_IRTL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_PKGC6_IRTL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PKGC6_IRTL, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_PKGC6_IRTL is defined as MSR_PKGC6_IRTL in SDM.
**/
#define MSR_SANDY_BRIDGE_PKGC6_IRTL              0x0000060B

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_PKGC6_IRTL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 9:0] Interrupt response time limit (R/W)  Specifies the limit
    /// that should be used to decide if the package should be put into a
    /// package C6 state.
    ///
    UINT32  TimeLimit:10;
    ///
    /// [Bits 12:10] Time Unit (R/W)  Specifies the encoding value of time
    /// unit of the interrupt response time limit. The following time unit
    /// encodings are supported: 000b: 1 ns 001b: 32 ns 010b: 1024 ns 011b:
    /// 32768 ns 100b: 1048576 ns 101b: 33554432 ns.
    ///
    UINT32  TimeUnit:3;
    UINT32  Reserved1:2;
    ///
    /// [Bit 15] Valid (R/W)  Indicates whether the values in bits 12:0 are
    /// valid and can be used by the processor for package C-sate management.
    ///
    UINT32  Valid:1;
    UINT32  Reserved2:16;
    UINT32  Reserved3:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_PKGC6_IRTL_REGISTER;


/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. Package C2
  Residency Counter. (R/O) Value since last reset that this package is in
  processor-specific C2 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_SANDY_BRIDGE_PKG_C2_RESIDENCY (0x0000060D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PKG_C2_RESIDENCY);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PKG_C2_RESIDENCY, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PKG_C2_RESIDENCY is defined as MSR_PKG_C2_RESIDENCY in SDM.
**/
#define MSR_SANDY_BRIDGE_PKG_C2_RESIDENCY        0x0000060D


/**
  Package. PKG RAPL Power Limit Control (R/W) See Section 14.9.3, "Package
  RAPL Domain.".

  @param  ECX  MSR_SANDY_BRIDGE_PKG_POWER_LIMIT (0x00000610)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PKG_POWER_LIMIT);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PKG_POWER_LIMIT, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PKG_POWER_LIMIT is defined as MSR_PKG_POWER_LIMIT in SDM.
**/
#define MSR_SANDY_BRIDGE_PKG_POWER_LIMIT         0x00000610


/**
  Package. PKG Energy Status (R/O) See Section 14.9.3, "Package RAPL Domain.".

  @param  ECX  MSR_SANDY_BRIDGE_PKG_ENERGY_STATUS (0x00000611)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PKG_ENERGY_STATUS);
  @endcode
  @note MSR_SANDY_BRIDGE_PKG_ENERGY_STATUS is defined as MSR_PKG_ENERGY_STATUS in SDM.
**/
#define MSR_SANDY_BRIDGE_PKG_ENERGY_STATUS       0x00000611


/**
  Package. PKG RAPL Parameters (R/W) See Section 14.9.3, "Package RAPL
  Domain.".

  @param  ECX  MSR_SANDY_BRIDGE_PKG_POWER_INFO (0x00000614)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PKG_POWER_INFO);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PKG_POWER_INFO, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PKG_POWER_INFO is defined as MSR_PKG_POWER_INFO in SDM.
**/
#define MSR_SANDY_BRIDGE_PKG_POWER_INFO          0x00000614


/**
  Package. PP0 RAPL Power Limit Control (R/W)  See Section 14.9.4, "PP0/PP1
  RAPL Domains.".

  @param  ECX  MSR_SANDY_BRIDGE_PP0_POWER_LIMIT (0x00000638)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PP0_POWER_LIMIT);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PP0_POWER_LIMIT, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PP0_POWER_LIMIT is defined as MSR_PP0_POWER_LIMIT in SDM.
**/
#define MSR_SANDY_BRIDGE_PP0_POWER_LIMIT         0x00000638


/**
  Package. PP0 Energy Status (R/O)  See Section 14.9.4, "PP0/PP1 RAPL
  Domains.".

  @param  ECX  MSR_SANDY_BRIDGE_PP0_ENERGY_STATUS (0x00000639)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PP0_ENERGY_STATUS);
  @endcode
  @note MSR_SANDY_BRIDGE_PP0_ENERGY_STATUS is defined as MSR_PP0_ENERGY_STATUS in SDM.
**/
#define MSR_SANDY_BRIDGE_PP0_ENERGY_STATUS       0x00000639


/**
  Thread. Last Branch Record n From IP (R/W) One of sixteen pairs of last
  branch record registers on the last branch record stack. This part of the
  stack contains pointers to the source instruction. See also: -  Last Branch
  Record Stack TOS at 1C9H -  Section 17.7.1 and record format in Section
  17.4.8.1.

  @param  ECX  MSR_SANDY_BRIDGE_LASTBRANCH_n_FROM_IP
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_LASTBRANCH_0_FROM_IP);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_LASTBRANCH_0_FROM_IP, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_LASTBRANCH_0_FROM_IP  is defined as MSR_LASTBRANCH_0_FROM_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_1_FROM_IP  is defined as MSR_LASTBRANCH_1_FROM_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_2_FROM_IP  is defined as MSR_LASTBRANCH_2_FROM_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_3_FROM_IP  is defined as MSR_LASTBRANCH_3_FROM_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_4_FROM_IP  is defined as MSR_LASTBRANCH_4_FROM_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_5_FROM_IP  is defined as MSR_LASTBRANCH_5_FROM_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_6_FROM_IP  is defined as MSR_LASTBRANCH_6_FROM_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_7_FROM_IP  is defined as MSR_LASTBRANCH_7_FROM_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_8_FROM_IP  is defined as MSR_LASTBRANCH_8_FROM_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_9_FROM_IP  is defined as MSR_LASTBRANCH_9_FROM_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_10_FROM_IP is defined as MSR_LASTBRANCH_10_FROM_IP in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_11_FROM_IP is defined as MSR_LASTBRANCH_11_FROM_IP in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_12_FROM_IP is defined as MSR_LASTBRANCH_12_FROM_IP in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_13_FROM_IP is defined as MSR_LASTBRANCH_13_FROM_IP in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_14_FROM_IP is defined as MSR_LASTBRANCH_14_FROM_IP in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_15_FROM_IP is defined as MSR_LASTBRANCH_15_FROM_IP in SDM.
  @{
**/
#define MSR_SANDY_BRIDGE_LASTBRANCH_0_FROM_IP    0x00000680
#define MSR_SANDY_BRIDGE_LASTBRANCH_1_FROM_IP    0x00000681
#define MSR_SANDY_BRIDGE_LASTBRANCH_2_FROM_IP    0x00000682
#define MSR_SANDY_BRIDGE_LASTBRANCH_3_FROM_IP    0x00000683
#define MSR_SANDY_BRIDGE_LASTBRANCH_4_FROM_IP    0x00000684
#define MSR_SANDY_BRIDGE_LASTBRANCH_5_FROM_IP    0x00000685
#define MSR_SANDY_BRIDGE_LASTBRANCH_6_FROM_IP    0x00000686
#define MSR_SANDY_BRIDGE_LASTBRANCH_7_FROM_IP    0x00000687
#define MSR_SANDY_BRIDGE_LASTBRANCH_8_FROM_IP    0x00000688
#define MSR_SANDY_BRIDGE_LASTBRANCH_9_FROM_IP    0x00000689
#define MSR_SANDY_BRIDGE_LASTBRANCH_10_FROM_IP   0x0000068A
#define MSR_SANDY_BRIDGE_LASTBRANCH_11_FROM_IP   0x0000068B
#define MSR_SANDY_BRIDGE_LASTBRANCH_12_FROM_IP   0x0000068C
#define MSR_SANDY_BRIDGE_LASTBRANCH_13_FROM_IP   0x0000068D
#define MSR_SANDY_BRIDGE_LASTBRANCH_14_FROM_IP   0x0000068E
#define MSR_SANDY_BRIDGE_LASTBRANCH_15_FROM_IP   0x0000068F
/// @}


/**
  Thread. Last Branch Record n To IP (R/W) One of sixteen pairs of last branch
  record registers on the last branch record stack. This part of the stack
  contains pointers to the destination instruction.

  @param  ECX  MSR_SANDY_BRIDGE_LASTBRANCH_n_TO_IP
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_LASTBRANCH_0_TO_IP);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_LASTBRANCH_0_TO_IP, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_LASTBRANCH_0_TO_IP  is defined as MSR_LASTBRANCH_0_TO_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_1_TO_IP  is defined as MSR_LASTBRANCH_1_TO_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_2_TO_IP  is defined as MSR_LASTBRANCH_2_TO_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_3_TO_IP  is defined as MSR_LASTBRANCH_3_TO_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_4_TO_IP  is defined as MSR_LASTBRANCH_4_TO_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_5_TO_IP  is defined as MSR_LASTBRANCH_5_TO_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_6_TO_IP  is defined as MSR_LASTBRANCH_6_TO_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_7_TO_IP  is defined as MSR_LASTBRANCH_7_TO_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_8_TO_IP  is defined as MSR_LASTBRANCH_8_TO_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_9_TO_IP  is defined as MSR_LASTBRANCH_9_TO_IP  in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_10_TO_IP is defined as MSR_LASTBRANCH_10_TO_IP in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_11_TO_IP is defined as MSR_LASTBRANCH_11_TO_IP in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_12_TO_IP is defined as MSR_LASTBRANCH_12_TO_IP in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_13_TO_IP is defined as MSR_LASTBRANCH_13_TO_IP in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_14_TO_IP is defined as MSR_LASTBRANCH_14_TO_IP in SDM.
        MSR_SANDY_BRIDGE_LASTBRANCH_15_TO_IP is defined as MSR_LASTBRANCH_15_TO_IP in SDM.
  @{
**/
#define MSR_SANDY_BRIDGE_LASTBRANCH_0_TO_IP      0x000006C0
#define MSR_SANDY_BRIDGE_LASTBRANCH_1_TO_IP      0x000006C1
#define MSR_SANDY_BRIDGE_LASTBRANCH_2_TO_IP      0x000006C2
#define MSR_SANDY_BRIDGE_LASTBRANCH_3_TO_IP      0x000006C3
#define MSR_SANDY_BRIDGE_LASTBRANCH_4_TO_IP      0x000006C4
#define MSR_SANDY_BRIDGE_LASTBRANCH_5_TO_IP      0x000006C5
#define MSR_SANDY_BRIDGE_LASTBRANCH_6_TO_IP      0x000006C6
#define MSR_SANDY_BRIDGE_LASTBRANCH_7_TO_IP      0x000006C7
#define MSR_SANDY_BRIDGE_LASTBRANCH_8_TO_IP      0x000006C8
#define MSR_SANDY_BRIDGE_LASTBRANCH_9_TO_IP      0x000006C9
#define MSR_SANDY_BRIDGE_LASTBRANCH_10_TO_IP     0x000006CA
#define MSR_SANDY_BRIDGE_LASTBRANCH_11_TO_IP     0x000006CB
#define MSR_SANDY_BRIDGE_LASTBRANCH_12_TO_IP     0x000006CC
#define MSR_SANDY_BRIDGE_LASTBRANCH_13_TO_IP     0x000006CD
#define MSR_SANDY_BRIDGE_LASTBRANCH_14_TO_IP     0x000006CE
#define MSR_SANDY_BRIDGE_LASTBRANCH_15_TO_IP     0x000006CF
/// @}


/**
  Package. Maximum Ratio Limit of Turbo Mode RO if MSR_PLATFORM_INFO.[28] = 0,
  RW if MSR_PLATFORM_INFO.[28] = 1.

  @param  ECX  MSR_SANDY_BRIDGE_TURBO_RATIO_LIMIT (0x000001AD)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_TURBO_RATIO_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_TURBO_RATIO_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_TURBO_RATIO_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_TURBO_RATIO_LIMIT);
  @endcode
  @note MSR_SANDY_BRIDGE_TURBO_RATIO_LIMIT is defined as MSR_TURBO_RATIO_LIMIT in SDM.
**/
#define MSR_SANDY_BRIDGE_TURBO_RATIO_LIMIT       0x000001AD

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_TURBO_RATIO_LIMIT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] Package. Maximum Ratio Limit for 1C Maximum turbo ratio
    /// limit of 1 core active.
    ///
    UINT32  Maximum1C:8;
    ///
    /// [Bits 15:8] Package. Maximum Ratio Limit for 2C Maximum turbo ratio
    /// limit of 2 core active.
    ///
    UINT32  Maximum2C:8;
    ///
    /// [Bits 23:16] Package. Maximum Ratio Limit for 3C Maximum turbo ratio
    /// limit of 3 core active.
    ///
    UINT32  Maximum3C:8;
    ///
    /// [Bits 31:24] Package. Maximum Ratio Limit for 4C Maximum turbo ratio
    /// limit of 4 core active.
    ///
    UINT32  Maximum4C:8;
    ///
    /// [Bits 39:32] Package. Maximum Ratio Limit for 5C Maximum turbo ratio
    /// limit of 5 core active.
    ///
    UINT32  Maximum5C:8;
    ///
    /// [Bits 47:40] Package. Maximum Ratio Limit for 6C Maximum turbo ratio
    /// limit of 6 core active.
    ///
    UINT32  Maximum6C:8;
    ///
    /// [Bits 55:48] Package. Maximum Ratio Limit for 7C Maximum turbo ratio
    /// limit of 7 core active.
    ///
    UINT32  Maximum7C:8;
    ///
    /// [Bits 63:56] Package. Maximum Ratio Limit for 8C Maximum turbo ratio
    /// limit of 8 core active.
    ///
    UINT32  Maximum8C:8;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_TURBO_RATIO_LIMIT_REGISTER;


/**
  Package. Uncore PMU global control.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_CTRL (0x00000391)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_CTRL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_CTRL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_CTRL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_CTRL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_CTRL, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_CTRL is defined as MSR_UNC_PERF_GLOBAL_CTRL in SDM.
**/
#define MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_CTRL    0x00000391

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_CTRL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Slice 0 select.
    ///
    UINT32  PMI_Sel_Slice0:1;
    ///
    /// [Bit 1] Slice 1 select.
    ///
    UINT32  PMI_Sel_Slice1:1;
    ///
    /// [Bit 2] Slice 2 select.
    ///
    UINT32  PMI_Sel_Slice2:1;
    ///
    /// [Bit 3] Slice 3 select.
    ///
    UINT32  PMI_Sel_Slice3:1;
    ///
    /// [Bit 4] Slice 4 select.
    ///
    UINT32  PMI_Sel_Slice4:1;
    UINT32  Reserved1:14;
    UINT32  Reserved2:10;
    ///
    /// [Bit 29] Enable all uncore counters.
    ///
    UINT32  EN:1;
    ///
    /// [Bit 30] Enable wake on PMI.
    ///
    UINT32  WakePMI:1;
    ///
    /// [Bit 31] Enable Freezing counter when overflow.
    ///
    UINT32  FREEZE:1;
    UINT32  Reserved3:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_CTRL_REGISTER;


/**
  Package. Uncore PMU main status.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_STATUS (0x00000392)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_STATUS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_STATUS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_STATUS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_STATUS);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_STATUS, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_STATUS is defined as MSR_UNC_PERF_GLOBAL_STATUS in SDM.
**/
#define MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_STATUS  0x00000392

/**
  MSR information returned for MSR index
  #MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_STATUS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Fixed counter overflowed.
    ///
    UINT32  Fixed:1;
    ///
    /// [Bit 1] An ARB counter overflowed.
    ///
    UINT32  ARB:1;
    UINT32  Reserved1:1;
    ///
    /// [Bit 3] A CBox counter overflowed (on any slice).
    ///
    UINT32  CBox:1;
    UINT32  Reserved2:28;
    UINT32  Reserved3:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_UNC_PERF_GLOBAL_STATUS_REGISTER;


/**
  Package. Uncore fixed counter control (R/W).

  @param  ECX  MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTRL (0x00000394)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTRL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTRL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTRL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTRL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTRL, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTRL is defined as MSR_UNC_PERF_FIXED_CTRL in SDM.
**/
#define MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTRL     0x00000394

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTRL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:20;
    ///
    /// [Bit 20] Enable overflow propagation.
    ///
    UINT32  EnableOverflow:1;
    UINT32  Reserved2:1;
    ///
    /// [Bit 22] Enable counting.
    ///
    UINT32  EnableCounting:1;
    UINT32  Reserved3:9;
    UINT32  Reserved4:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTRL_REGISTER;


/**
  Package. Uncore fixed counter.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTR (0x00000395)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTR_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTR_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTR_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTR);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTR, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTR is defined as MSR_UNC_PERF_FIXED_CTR in SDM.
**/
#define MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTR      0x00000395

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTR
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 31:0] Current count.
    ///
    UINT32  CurrentCount:32;
    ///
    /// [Bits 47:32] Current count.
    ///
    UINT32  CurrentCountHi:16;
    UINT32  Reserved:16;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_UNC_PERF_FIXED_CTR_REGISTER;


/**
  Package. Uncore C-Box configuration information (R/O).

  @param  ECX  MSR_SANDY_BRIDGE_UNC_CBO_CONFIG (0x00000396)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_UNC_CBO_CONFIG_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_UNC_CBO_CONFIG_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_UNC_CBO_CONFIG_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_CONFIG);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_CBO_CONFIG is defined as MSR_UNC_CBO_CONFIG in SDM.
**/
#define MSR_SANDY_BRIDGE_UNC_CBO_CONFIG          0x00000396

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_UNC_CBO_CONFIG
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 3:0] Report the number of C-Box units with performance counters,
    /// including processor cores and processor graphics".
    ///
    UINT32  CBox:4;
    UINT32  Reserved1:28;
    UINT32  Reserved2:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_UNC_CBO_CONFIG_REGISTER;


/**
  Package. Uncore Arb unit, performance counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_ARB_PERFCTR0 (0x000003B0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_ARB_PERFCTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_ARB_PERFCTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_ARB_PERFCTR0 is defined as MSR_UNC_ARB_PERFCTR0 in SDM.
**/
#define MSR_SANDY_BRIDGE_UNC_ARB_PERFCTR0        0x000003B0


/**
  Package. Uncore Arb unit, performance counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_ARB_PERFCTR1 (0x000003B1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_ARB_PERFCTR1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_ARB_PERFCTR1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_ARB_PERFCTR1 is defined as MSR_UNC_ARB_PERFCTR1 in SDM.
**/
#define MSR_SANDY_BRIDGE_UNC_ARB_PERFCTR1        0x000003B1


/**
  Package. Uncore Arb unit, counter 0 event select MSR.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_ARB_PERFEVTSEL0 (0x000003B2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_ARB_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_ARB_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_ARB_PERFEVTSEL0 is defined as MSR_UNC_ARB_PERFEVTSEL0 in SDM.
**/
#define MSR_SANDY_BRIDGE_UNC_ARB_PERFEVTSEL0     0x000003B2


/**
  Package. Uncore Arb unit, counter 1 event select MSR.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_ARB_PERFEVTSEL1 (0x000003B3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_ARB_PERFEVTSEL1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_ARB_PERFEVTSEL1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_ARB_PERFEVTSEL1 is defined as MSR_UNC_ARB_PERFEVTSEL1 in SDM.
**/
#define MSR_SANDY_BRIDGE_UNC_ARB_PERFEVTSEL1     0x000003B3


/**
  Package. Package C7 Interrupt Response Limit (R/W)  This MSR defines the
  budget allocated for the package to exit from C7 to a C0 state, where
  interrupt request can be delivered to the core and serviced. Additional
  core-exit latency amy be applicable depending on the actual C-state the core
  is in. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates.

  @param  ECX  MSR_SANDY_BRIDGE_PKGC7_IRTL (0x0000060C)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PKGC7_IRTL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PKGC7_IRTL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_PKGC7_IRTL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_PKGC7_IRTL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PKGC7_IRTL, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_PKGC7_IRTL is defined as MSR_PKGC7_IRTL in SDM.
**/
#define MSR_SANDY_BRIDGE_PKGC7_IRTL              0x0000060C

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_PKGC7_IRTL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 9:0] Interrupt response time limit (R/W)  Specifies the limit
    /// that should be used to decide if the package should be put into a
    /// package C7 state.
    ///
    UINT32  TimeLimit:10;
    ///
    /// [Bits 12:10] Time Unit (R/W)  Specifies the encoding value of time
    /// unit of the interrupt response time limit. The following time unit
    /// encodings are supported: 000b: 1 ns 001b: 32 ns 010b: 1024 ns 011b:
    /// 32768 ns 100b: 1048576 ns 101b: 33554432 ns.
    ///
    UINT32  TimeUnit:3;
    UINT32  Reserved1:2;
    ///
    /// [Bit 15] Valid (R/W)  Indicates whether the values in bits 12:0 are
    /// valid and can be used by the processor for package C-sate management.
    ///
    UINT32  Valid:1;
    UINT32  Reserved2:16;
    UINT32  Reserved3:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_PKGC7_IRTL_REGISTER;


/**
  Package. PP0 Balance Policy (R/W) See Section 14.9.4, "PP0/PP1 RAPL
  Domains.".

  @param  ECX  MSR_SANDY_BRIDGE_PP0_POLICY (0x0000063A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PP0_POLICY);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PP0_POLICY, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PP0_POLICY is defined as MSR_PP0_POLICY in SDM.
**/
#define MSR_SANDY_BRIDGE_PP0_POLICY              0x0000063A


/**
  Package. PP1 RAPL Power Limit Control (R/W) See Section 14.9.4, "PP0/PP1
  RAPL Domains.".

  @param  ECX  MSR_SANDY_BRIDGE_PP1_POWER_LIMIT (0x00000640)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PP1_POWER_LIMIT);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PP1_POWER_LIMIT, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PP1_POWER_LIMIT is defined as MSR_PP1_POWER_LIMIT in SDM.
**/
#define MSR_SANDY_BRIDGE_PP1_POWER_LIMIT         0x00000640


/**
  Package. PP1 Energy Status (R/O)  See Section 14.9.4, "PP0/PP1 RAPL
  Domains.".

  @param  ECX  MSR_SANDY_BRIDGE_PP1_ENERGY_STATUS (0x00000641)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PP1_ENERGY_STATUS);
  @endcode
  @note MSR_SANDY_BRIDGE_PP1_ENERGY_STATUS is defined as MSR_PP1_ENERGY_STATUS in SDM.
**/
#define MSR_SANDY_BRIDGE_PP1_ENERGY_STATUS       0x00000641


/**
  Package. PP1 Balance Policy (R/W) See Section 14.9.4, "PP0/PP1 RAPL
  Domains.".

  @param  ECX  MSR_SANDY_BRIDGE_PP1_POLICY (0x00000642)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PP1_POLICY);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PP1_POLICY, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PP1_POLICY is defined as MSR_PP1_POLICY in SDM.
**/
#define MSR_SANDY_BRIDGE_PP1_POLICY              0x00000642


/**
  Package. Uncore C-Box 0, counter n event select MSR.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_CBO_0_PERFEVTSELn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_0_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_0_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_CBO_0_PERFEVTSEL0 is defined as MSR_UNC_CBO_0_PERFEVTSEL0 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_0_PERFEVTSEL1 is defined as MSR_UNC_CBO_0_PERFEVTSEL1 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_0_PERFEVTSEL2 is defined as MSR_UNC_CBO_0_PERFEVTSEL2 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_0_PERFEVTSEL3 is defined as MSR_UNC_CBO_0_PERFEVTSEL3 in SDM.
  @{
**/
#define MSR_SANDY_BRIDGE_UNC_CBO_0_PERFEVTSEL0   0x00000700
#define MSR_SANDY_BRIDGE_UNC_CBO_0_PERFEVTSEL1   0x00000701
#define MSR_SANDY_BRIDGE_UNC_CBO_0_PERFEVTSEL2   0x00000702
#define MSR_SANDY_BRIDGE_UNC_CBO_0_PERFEVTSEL3   0x00000703
/// @}


/**
  Package. Uncore C-Box n, unit status for counter 0-3.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_CBO_n_UNIT_STATUS
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_0_UNIT_STATUS);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_0_UNIT_STATUS, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_CBO_0_UNIT_STATUS is defined as MSR_UNC_CBO_0_UNIT_STATUS in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_1_UNIT_STATUS is defined as MSR_UNC_CBO_1_UNIT_STATUS in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_2_UNIT_STATUS is defined as MSR_UNC_CBO_2_UNIT_STATUS in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_3_UNIT_STATUS is defined as MSR_UNC_CBO_3_UNIT_STATUS in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_4_UNIT_STATUS is defined as MSR_UNC_CBO_4_UNIT_STATUS in SDM.
  @{
**/
#define MSR_SANDY_BRIDGE_UNC_CBO_0_UNIT_STATUS   0x00000705
#define MSR_SANDY_BRIDGE_UNC_CBO_1_UNIT_STATUS   0x00000715
#define MSR_SANDY_BRIDGE_UNC_CBO_2_UNIT_STATUS   0x00000725
#define MSR_SANDY_BRIDGE_UNC_CBO_3_UNIT_STATUS   0x00000735
#define MSR_SANDY_BRIDGE_UNC_CBO_4_UNIT_STATUS   0x00000745
/// @}


/**
  Package. Uncore C-Box 0, performance counter n.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_CBO_0_PERFCTRn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_0_PERFCTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_0_PERFCTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_CBO_0_PERFCTR0 is defined as MSR_UNC_CBO_0_PERFCTR0 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_0_PERFCTR1 is defined as MSR_UNC_CBO_0_PERFCTR1 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_0_PERFCTR2 is defined as MSR_UNC_CBO_0_PERFCTR2 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_0_PERFCTR3 is defined as MSR_UNC_CBO_0_PERFCTR3 in SDM.
  @{
**/
#define MSR_SANDY_BRIDGE_UNC_CBO_0_PERFCTR0      0x00000706
#define MSR_SANDY_BRIDGE_UNC_CBO_0_PERFCTR1      0x00000707
#define MSR_SANDY_BRIDGE_UNC_CBO_0_PERFCTR2      0x00000708
#define MSR_SANDY_BRIDGE_UNC_CBO_0_PERFCTR3      0x00000709
/// @}


/**
  Package. Uncore C-Box 1, counter n event select MSR.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_CBO_1_PERFEVTSELn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_1_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_1_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_CBO_1_PERFEVTSEL0 is defined as MSR_UNC_CBO_1_PERFEVTSEL0 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_1_PERFEVTSEL1 is defined as MSR_UNC_CBO_1_PERFEVTSEL1 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_1_PERFEVTSEL2 is defined as MSR_UNC_CBO_1_PERFEVTSEL2 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_1_PERFEVTSEL3 is defined as MSR_UNC_CBO_1_PERFEVTSEL3 in SDM.
  @{
**/
#define MSR_SANDY_BRIDGE_UNC_CBO_1_PERFEVTSEL0   0x00000710
#define MSR_SANDY_BRIDGE_UNC_CBO_1_PERFEVTSEL1   0x00000711
#define MSR_SANDY_BRIDGE_UNC_CBO_1_PERFEVTSEL2   0x00000712
#define MSR_SANDY_BRIDGE_UNC_CBO_1_PERFEVTSEL3   0x00000713
/// @}


/**
  Package. Uncore C-Box 1, performance counter n.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_CBO_1_PERFCTRn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_1_PERFCTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_1_PERFCTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_CBO_1_PERFCTR0 is defined as MSR_UNC_CBO_1_PERFCTR0 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_1_PERFCTR1 is defined as MSR_UNC_CBO_1_PERFCTR1 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_1_PERFCTR2 is defined as MSR_UNC_CBO_1_PERFCTR2 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_1_PERFCTR3 is defined as MSR_UNC_CBO_1_PERFCTR3 in SDM.
  @{
**/
#define MSR_SANDY_BRIDGE_UNC_CBO_1_PERFCTR0      0x00000716
#define MSR_SANDY_BRIDGE_UNC_CBO_1_PERFCTR1      0x00000717
#define MSR_SANDY_BRIDGE_UNC_CBO_1_PERFCTR2      0x00000718
#define MSR_SANDY_BRIDGE_UNC_CBO_1_PERFCTR3      0x00000719
/// @}


/**
  Package. Uncore C-Box 2, counter n event select MSR.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_CBO_2_PERFEVTSELn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_2_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_2_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_CBO_2_PERFEVTSEL0 is defined as MSR_UNC_CBO_2_PERFEVTSEL0 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_2_PERFEVTSEL1 is defined as MSR_UNC_CBO_2_PERFEVTSEL1 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_2_PERFEVTSEL2 is defined as MSR_UNC_CBO_2_PERFEVTSEL2 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_2_PERFEVTSEL3 is defined as MSR_UNC_CBO_2_PERFEVTSEL3 in SDM.
  @{
**/
#define MSR_SANDY_BRIDGE_UNC_CBO_2_PERFEVTSEL0   0x00000720
#define MSR_SANDY_BRIDGE_UNC_CBO_2_PERFEVTSEL1   0x00000721
#define MSR_SANDY_BRIDGE_UNC_CBO_2_PERFEVTSEL2   0x00000722
#define MSR_SANDY_BRIDGE_UNC_CBO_2_PERFEVTSEL3   0x00000723
/// @}


/**
  Package. Uncore C-Box 2, performance counter n.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_CBO_2_PERFCTRn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_2_PERFCTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_2_PERFCTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_CBO_2_PERFCTR0 is defined as MSR_UNC_CBO_2_PERFCTR0 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_2_PERFCTR1 is defined as MSR_UNC_CBO_2_PERFCTR1 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_2_PERFCTR2 is defined as MSR_UNC_CBO_2_PERFCTR2 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_2_PERFCTR3 is defined as MSR_UNC_CBO_2_PERFCTR3 in SDM.
  @{
**/
#define MSR_SANDY_BRIDGE_UNC_CBO_2_PERFCTR0      0x00000726
#define MSR_SANDY_BRIDGE_UNC_CBO_2_PERFCTR1      0x00000727
#define MSR_SANDY_BRIDGE_UNC_CBO_2_PERFCTR2      0x00000728
#define MSR_SANDY_BRIDGE_UNC_CBO_2_PERFCTR3      0x00000729
/// @}


/**
  Package. Uncore C-Box 3, counter n event select MSR.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_CBO_3_PERFEVTSELn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_3_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_3_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_CBO_3_PERFEVTSEL0 is defined as MSR_UNC_CBO_3_PERFEVTSEL0 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_3_PERFEVTSEL1 is defined as MSR_UNC_CBO_3_PERFEVTSEL1 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_3_PERFEVTSEL2 is defined as MSR_UNC_CBO_3_PERFEVTSEL2 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_3_PERFEVTSEL3 is defined as MSR_UNC_CBO_3_PERFEVTSEL3 in SDM.
  @{
**/
#define MSR_SANDY_BRIDGE_UNC_CBO_3_PERFEVTSEL0   0x00000730
#define MSR_SANDY_BRIDGE_UNC_CBO_3_PERFEVTSEL1   0x00000731
#define MSR_SANDY_BRIDGE_UNC_CBO_3_PERFEVTSEL2   0x00000732
#define MSR_SANDY_BRIDGE_UNC_CBO_3_PERFEVTSEL3   0x00000733
/// @}


/**
  Package. Uncore C-Box 3, performance counter n.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_CBO_3_PERFCTRn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_3_PERFCTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_3_PERFCTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_CBO_3_PERFCTR0 is defined as MSR_UNC_CBO_3_PERFCTR0 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_3_PERFCTR1 is defined as MSR_UNC_CBO_3_PERFCTR1 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_3_PERFCTR2 is defined as MSR_UNC_CBO_3_PERFCTR2 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_3_PERFCTR3 is defined as MSR_UNC_CBO_3_PERFCTR3 in SDM.
  @{
**/
#define MSR_SANDY_BRIDGE_UNC_CBO_3_PERFCTR0      0x00000736
#define MSR_SANDY_BRIDGE_UNC_CBO_3_PERFCTR1      0x00000737
#define MSR_SANDY_BRIDGE_UNC_CBO_3_PERFCTR2      0x00000738
#define MSR_SANDY_BRIDGE_UNC_CBO_3_PERFCTR3      0x00000739
/// @}


/**
  Package. Uncore C-Box 4, counter n event select MSR.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_CBO_4_PERFEVTSELn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_4_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_4_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_CBO_4_PERFEVTSEL0 is defined as MSR_UNC_CBO_4_PERFEVTSEL0 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_4_PERFEVTSEL1 is defined as MSR_UNC_CBO_4_PERFEVTSEL1 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_4_PERFEVTSEL2 is defined as MSR_UNC_CBO_4_PERFEVTSEL2 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_4_PERFEVTSEL3 is defined as MSR_UNC_CBO_4_PERFEVTSEL3 in SDM.
  @{
**/
#define MSR_SANDY_BRIDGE_UNC_CBO_4_PERFEVTSEL0   0x00000740
#define MSR_SANDY_BRIDGE_UNC_CBO_4_PERFEVTSEL1   0x00000741
#define MSR_SANDY_BRIDGE_UNC_CBO_4_PERFEVTSEL2   0x00000742
#define MSR_SANDY_BRIDGE_UNC_CBO_4_PERFEVTSEL3   0x00000743
/// @}


/**
  Package. Uncore C-Box 4, performance counter n.

  @param  ECX  MSR_SANDY_BRIDGE_UNC_CBO_4_PERFCTRn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_4_PERFCTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_UNC_CBO_4_PERFCTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_UNC_CBO_4_PERFCTR0 is defined as MSR_UNC_CBO_4_PERFCTR0 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_4_PERFCTR1 is defined as MSR_UNC_CBO_4_PERFCTR1 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_4_PERFCTR2 is defined as MSR_UNC_CBO_4_PERFCTR2 in SDM.
        MSR_SANDY_BRIDGE_UNC_CBO_4_PERFCTR3 is defined as MSR_UNC_CBO_4_PERFCTR3 in SDM.
  @{
**/
#define MSR_SANDY_BRIDGE_UNC_CBO_4_PERFCTR0      0x00000746
#define MSR_SANDY_BRIDGE_UNC_CBO_4_PERFCTR1      0x00000747
#define MSR_SANDY_BRIDGE_UNC_CBO_4_PERFCTR2      0x00000748
#define MSR_SANDY_BRIDGE_UNC_CBO_4_PERFCTR3      0x00000749
/// @}


/**
  Package. MC Bank Error Configuration (R/W).

  @param  ECX  MSR_SANDY_BRIDGE_ERROR_CONTROL (0x0000017F)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_ERROR_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_ERROR_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_ERROR_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_ERROR_CONTROL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_ERROR_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_ERROR_CONTROL is defined as MSR_ERROR_CONTROL in SDM.
**/
#define MSR_SANDY_BRIDGE_ERROR_CONTROL           0x0000017F

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_ERROR_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:1;
    ///
    /// [Bit 1] MemError Log Enable (R/W)  When set, enables IMC status bank
    /// to log additional info in bits 36:32.
    ///
    UINT32  MemErrorLogEnable:1;
    UINT32  Reserved2:30;
    UINT32  Reserved3:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_ERROR_CONTROL_REGISTER;


/**
  Package.

  @param  ECX  MSR_SANDY_BRIDGE_PEBS_NUM_ALT (0x0000039C)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PEBS_NUM_ALT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SANDY_BRIDGE_PEBS_NUM_ALT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SANDY_BRIDGE_PEBS_NUM_ALT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_PEBS_NUM_ALT);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PEBS_NUM_ALT, Msr.Uint64);
  @endcode
  @note MSR_SANDY_BRIDGE_PEBS_NUM_ALT is defined as MSR_PEBS_NUM_ALT in SDM.
**/
#define MSR_SANDY_BRIDGE_PEBS_NUM_ALT            0x0000039C

/**
  MSR information returned for MSR index #MSR_SANDY_BRIDGE_PEBS_NUM_ALT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] ENABLE_PEBS_NUM_ALT (RW) Write 1 to enable alternate PEBS
    /// counting logic for specific events requiring additional configuration,
    /// see Table 19-17.
    ///
    UINT32  ENABLE_PEBS_NUM_ALT:1;
    UINT32  Reserved1:31;
    UINT32  Reserved2:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SANDY_BRIDGE_PEBS_NUM_ALT_REGISTER;


/**
  Package. Package RAPL Perf Status (R/O).

  @param  ECX  MSR_SANDY_BRIDGE_PKG_PERF_STATUS (0x00000613)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PKG_PERF_STATUS);
  @endcode
  @note MSR_SANDY_BRIDGE_PKG_PERF_STATUS is defined as MSR_PKG_PERF_STATUS in SDM.
**/
#define MSR_SANDY_BRIDGE_PKG_PERF_STATUS         0x00000613


/**
  Package. DRAM RAPL Power Limit Control (R/W)  See Section 14.9.5, "DRAM RAPL
  Domain.".

  @param  ECX  MSR_SANDY_BRIDGE_DRAM_POWER_LIMIT (0x00000618)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_DRAM_POWER_LIMIT);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_DRAM_POWER_LIMIT, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_DRAM_POWER_LIMIT is defined as MSR_DRAM_POWER_LIMIT in SDM.
**/
#define MSR_SANDY_BRIDGE_DRAM_POWER_LIMIT        0x00000618


/**
  Package. DRAM Energy Status (R/O)  See Section 14.9.5, "DRAM RAPL Domain.".

  @param  ECX  MSR_SANDY_BRIDGE_DRAM_ENERGY_STATUS (0x00000619)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_DRAM_ENERGY_STATUS);
  @endcode
  @note MSR_SANDY_BRIDGE_DRAM_ENERGY_STATUS is defined as MSR_DRAM_ENERGY_STATUS in SDM.
**/
#define MSR_SANDY_BRIDGE_DRAM_ENERGY_STATUS      0x00000619


/**
  Package. DRAM Performance Throttling Status (R/O) See Section 14.9.5, "DRAM
  RAPL Domain.".

  @param  ECX  MSR_SANDY_BRIDGE_DRAM_PERF_STATUS (0x0000061B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_DRAM_PERF_STATUS);
  @endcode
  @note MSR_SANDY_BRIDGE_DRAM_PERF_STATUS is defined as MSR_DRAM_PERF_STATUS in SDM.
**/
#define MSR_SANDY_BRIDGE_DRAM_PERF_STATUS        0x0000061B


/**
  Package. DRAM RAPL Parameters (R/W) See Section 14.9.5, "DRAM RAPL Domain.".

  @param  ECX  MSR_SANDY_BRIDGE_DRAM_POWER_INFO (0x0000061C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_DRAM_POWER_INFO);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_DRAM_POWER_INFO, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_DRAM_POWER_INFO is defined as MSR_DRAM_POWER_INFO in SDM.
**/
#define MSR_SANDY_BRIDGE_DRAM_POWER_INFO         0x0000061C


/**
  Package. Uncore U-box UCLK fixed counter control.

  @param  ECX  MSR_SANDY_BRIDGE_U_PMON_UCLK_FIXED_CTL (0x00000C08)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_U_PMON_UCLK_FIXED_CTL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_U_PMON_UCLK_FIXED_CTL, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_U_PMON_UCLK_FIXED_CTL is defined as MSR_U_PMON_UCLK_FIXED_CTL in SDM.
**/
#define MSR_SANDY_BRIDGE_U_PMON_UCLK_FIXED_CTL   0x00000C08


/**
  Package. Uncore U-box UCLK fixed counter.

  @param  ECX  MSR_SANDY_BRIDGE_U_PMON_UCLK_FIXED_CTR (0x00000C09)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_U_PMON_UCLK_FIXED_CTR);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_U_PMON_UCLK_FIXED_CTR, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_U_PMON_UCLK_FIXED_CTR is defined as MSR_U_PMON_UCLK_FIXED_CTR in SDM.
**/
#define MSR_SANDY_BRIDGE_U_PMON_UCLK_FIXED_CTR   0x00000C09


/**
  Package. Uncore U-box perfmon event select for U-box counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_U_PMON_EVNTSEL0 (0x00000C10)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_U_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_U_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_U_PMON_EVNTSEL0 is defined as MSR_U_PMON_EVNTSEL0 in SDM.
**/
#define MSR_SANDY_BRIDGE_U_PMON_EVNTSEL0         0x00000C10


/**
  Package. Uncore U-box perfmon event select for U-box counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_U_PMON_EVNTSEL1 (0x00000C11)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_U_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_U_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_U_PMON_EVNTSEL1 is defined as MSR_U_PMON_EVNTSEL1 in SDM.
**/
#define MSR_SANDY_BRIDGE_U_PMON_EVNTSEL1         0x00000C11


/**
  Package. Uncore U-box perfmon counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_U_PMON_CTR0 (0x00000C16)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_U_PMON_CTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_U_PMON_CTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_U_PMON_CTR0 is defined as MSR_U_PMON_CTR0 in SDM.
**/
#define MSR_SANDY_BRIDGE_U_PMON_CTR0             0x00000C16


/**
  Package. Uncore U-box perfmon counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_U_PMON_CTR1 (0x00000C17)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_U_PMON_CTR1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_U_PMON_CTR1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_U_PMON_CTR1 is defined as MSR_U_PMON_CTR1 in SDM.
**/
#define MSR_SANDY_BRIDGE_U_PMON_CTR1             0x00000C17


/**
  Package. Uncore PCU perfmon for PCU-box-wide control.

  @param  ECX  MSR_SANDY_BRIDGE_PCU_PMON_BOX_CTL (0x00000C24)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PCU_PMON_BOX_CTL is defined as MSR_PCU_PMON_BOX_CTL in SDM.
**/
#define MSR_SANDY_BRIDGE_PCU_PMON_BOX_CTL        0x00000C24


/**
  Package. Uncore PCU perfmon event select for PCU counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL0 (0x00000C30)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL0 is defined as MSR_PCU_PMON_EVNTSEL0 in SDM.
**/
#define MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL0       0x00000C30


/**
  Package. Uncore PCU perfmon event select for PCU counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL1 (0x00000C31)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL1 is defined as MSR_PCU_PMON_EVNTSEL1 in SDM.
**/
#define MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL1       0x00000C31


/**
  Package. Uncore PCU perfmon event select for PCU counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL2 (0x00000C32)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL2 is defined as MSR_PCU_PMON_EVNTSEL2 in SDM.
**/
#define MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL2       0x00000C32


/**
  Package. Uncore PCU perfmon event select for PCU counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL3 (0x00000C33)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL3 is defined as MSR_PCU_PMON_EVNTSEL3 in SDM.
**/
#define MSR_SANDY_BRIDGE_PCU_PMON_EVNTSEL3       0x00000C33


/**
  Package. Uncore PCU perfmon box-wide filter.

  @param  ECX  MSR_SANDY_BRIDGE_PCU_PMON_BOX_FILTER (0x00000C34)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PCU_PMON_BOX_FILTER is defined as MSR_PCU_PMON_BOX_FILTER in SDM.
**/
#define MSR_SANDY_BRIDGE_PCU_PMON_BOX_FILTER     0x00000C34


/**
  Package. Uncore PCU perfmon counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_PCU_PMON_CTR0 (0x00000C36)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_CTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_CTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PCU_PMON_CTR0 is defined as MSR_PCU_PMON_CTR0 in SDM.
**/
#define MSR_SANDY_BRIDGE_PCU_PMON_CTR0           0x00000C36


/**
  Package. Uncore PCU perfmon counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_PCU_PMON_CTR1 (0x00000C37)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_CTR1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_CTR1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PCU_PMON_CTR1 is defined as MSR_PCU_PMON_CTR1 in SDM.
**/
#define MSR_SANDY_BRIDGE_PCU_PMON_CTR1           0x00000C37


/**
  Package. Uncore PCU perfmon counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_PCU_PMON_CTR2 (0x00000C38)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_CTR2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_CTR2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PCU_PMON_CTR2 is defined as MSR_PCU_PMON_CTR2 in SDM.
**/
#define MSR_SANDY_BRIDGE_PCU_PMON_CTR2           0x00000C38


/**
  Package. Uncore PCU perfmon counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_PCU_PMON_CTR3 (0x00000C39)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_CTR3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_PCU_PMON_CTR3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_PCU_PMON_CTR3 is defined as MSR_PCU_PMON_CTR3 in SDM.
**/
#define MSR_SANDY_BRIDGE_PCU_PMON_CTR3           0x00000C39


/**
  Package. Uncore C-box 0 perfmon local box wide control.

  @param  ECX  MSR_SANDY_BRIDGE_C0_PMON_BOX_CTL (0x00000D04)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C0_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C0_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C0_PMON_BOX_CTL is defined as MSR_C0_PMON_BOX_CTL in SDM.
**/
#define MSR_SANDY_BRIDGE_C0_PMON_BOX_CTL         0x00000D04


/**
  Package. Uncore C-box 0 perfmon event select for C-box 0 counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL0 (0x00000D10)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL0 is defined as MSR_C0_PMON_EVNTSEL0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL0        0x00000D10


/**
  Package. Uncore C-box 0 perfmon event select for C-box 0 counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL1 (0x00000D11)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL1 is defined as MSR_C0_PMON_EVNTSEL1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL1        0x00000D11


/**
  Package. Uncore C-box 0 perfmon event select for C-box 0 counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL2 (0x00000D12)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL2 is defined as MSR_C0_PMON_EVNTSEL2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL2        0x00000D12


/**
  Package. Uncore C-box 0 perfmon event select for C-box 0 counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL3 (0x00000D13)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL3 is defined as MSR_C0_PMON_EVNTSEL3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C0_PMON_EVNTSEL3        0x00000D13


/**
  Package. Uncore C-box 0 perfmon box wide filter.

  @param  ECX  MSR_SANDY_BRIDGE_C0_PMON_BOX_FILTER (0x00000D14)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C0_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C0_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C0_PMON_BOX_FILTER is defined as MSR_C0_PMON_BOX_FILTER in SDM.
**/
#define MSR_SANDY_BRIDGE_C0_PMON_BOX_FILTER      0x00000D14


/**
  Package. Uncore C-box 0 perfmon counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C0_PMON_CTR0 (0x00000D16)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C0_PMON_CTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C0_PMON_CTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C0_PMON_CTR0 is defined as MSR_C0_PMON_CTR0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C0_PMON_CTR0            0x00000D16


/**
  Package. Uncore C-box 0 perfmon counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C0_PMON_CTR1 (0x00000D17)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C0_PMON_CTR1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C0_PMON_CTR1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C0_PMON_CTR1 is defined as MSR_C0_PMON_CTR1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C0_PMON_CTR1            0x00000D17


/**
  Package. Uncore C-box 0 perfmon counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C0_PMON_CTR2 (0x00000D18)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C0_PMON_CTR2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C0_PMON_CTR2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C0_PMON_CTR2 is defined as MSR_C0_PMON_CTR2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C0_PMON_CTR2            0x00000D18


/**
  Package. Uncore C-box 0 perfmon counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C0_PMON_CTR3 (0x00000D19)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C0_PMON_CTR3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C0_PMON_CTR3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C0_PMON_CTR3 is defined as MSR_C0_PMON_CTR3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C0_PMON_CTR3            0x00000D19


/**
  Package. Uncore C-box 1 perfmon local box wide control.

  @param  ECX  MSR_SANDY_BRIDGE_C1_PMON_BOX_CTL (0x00000D24)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C1_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C1_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C1_PMON_BOX_CTL is defined as MSR_C1_PMON_BOX_CTL in SDM.
**/
#define MSR_SANDY_BRIDGE_C1_PMON_BOX_CTL         0x00000D24


/**
  Package. Uncore C-box 1 perfmon event select for C-box 1 counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL0 (0x00000D30)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL0 is defined as MSR_C1_PMON_EVNTSEL0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL0        0x00000D30


/**
  Package. Uncore C-box 1 perfmon event select for C-box 1 counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL1 (0x00000D31)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL1 is defined as MSR_C1_PMON_EVNTSEL1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL1        0x00000D31


/**
  Package. Uncore C-box 1 perfmon event select for C-box 1 counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL2 (0x00000D32)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL2 is defined as MSR_C1_PMON_EVNTSEL2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL2        0x00000D32


/**
  Package. Uncore C-box 1 perfmon event select for C-box 1 counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL3 (0x00000D33)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL3 is defined as MSR_C1_PMON_EVNTSEL3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C1_PMON_EVNTSEL3        0x00000D33


/**
  Package. Uncore C-box 1 perfmon box wide filter.

  @param  ECX  MSR_SANDY_BRIDGE_C1_PMON_BOX_FILTER (0x00000D34)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C1_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C1_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C1_PMON_BOX_FILTER is defined as MSR_C1_PMON_BOX_FILTER in SDM.
**/
#define MSR_SANDY_BRIDGE_C1_PMON_BOX_FILTER      0x00000D34


/**
  Package. Uncore C-box 1 perfmon counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C1_PMON_CTR0 (0x00000D36)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C1_PMON_CTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C1_PMON_CTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C1_PMON_CTR0 is defined as MSR_C1_PMON_CTR0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C1_PMON_CTR0            0x00000D36


/**
  Package. Uncore C-box 1 perfmon counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C1_PMON_CTR1 (0x00000D37)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C1_PMON_CTR1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C1_PMON_CTR1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C1_PMON_CTR1 is defined as MSR_C1_PMON_CTR1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C1_PMON_CTR1            0x00000D37


/**
  Package. Uncore C-box 1 perfmon counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C1_PMON_CTR2 (0x00000D38)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C1_PMON_CTR2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C1_PMON_CTR2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C1_PMON_CTR2 is defined as MSR_C1_PMON_CTR2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C1_PMON_CTR2            0x00000D38


/**
  Package. Uncore C-box 1 perfmon counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C1_PMON_CTR3 (0x00000D39)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C1_PMON_CTR3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C1_PMON_CTR3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C1_PMON_CTR3 is defined as MSR_C1_PMON_CTR3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C1_PMON_CTR3            0x00000D39


/**
  Package. Uncore C-box 2 perfmon local box wide control.

  @param  ECX  MSR_SANDY_BRIDGE_C2_PMON_BOX_CTL (0x00000D44)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C2_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C2_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C2_PMON_BOX_CTL is defined as MSR_C2_PMON_BOX_CTL in SDM.
**/
#define MSR_SANDY_BRIDGE_C2_PMON_BOX_CTL         0x00000D44


/**
  Package. Uncore C-box 2 perfmon event select for C-box 2 counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL0 (0x00000D50)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL0 is defined as MSR_C2_PMON_EVNTSEL0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL0        0x00000D50


/**
  Package. Uncore C-box 2 perfmon event select for C-box 2 counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL1 (0x00000D51)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL1 is defined as MSR_C2_PMON_EVNTSEL1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL1        0x00000D51


/**
  Package. Uncore C-box 2 perfmon event select for C-box 2 counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL2 (0x00000D52)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL2 is defined as MSR_C2_PMON_EVNTSEL2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL2        0x00000D52


/**
  Package. Uncore C-box 2 perfmon event select for C-box 2 counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL3 (0x00000D53)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL3 is defined as MSR_C2_PMON_EVNTSEL3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C2_PMON_EVNTSEL3        0x00000D53


/**
  Package. Uncore C-box 2 perfmon box wide filter.

  @param  ECX  MSR_SANDY_BRIDGE_C2_PMON_BOX_FILTER (0x00000D54)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C2_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C2_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C2_PMON_BOX_FILTER is defined as MSR_C2_PMON_BOX_FILTER in SDM.
**/
#define MSR_SANDY_BRIDGE_C2_PMON_BOX_FILTER      0x00000D54


/**
  Package. Uncore C-box 2 perfmon counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C2_PMON_CTR0 (0x00000D56)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C2_PMON_CTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C2_PMON_CTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C2_PMON_CTR0 is defined as MSR_C2_PMON_CTR0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C2_PMON_CTR0            0x00000D56


/**
  Package. Uncore C-box 2 perfmon counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C2_PMON_CTR1 (0x00000D57)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C2_PMON_CTR1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C2_PMON_CTR1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C2_PMON_CTR1 is defined as MSR_C2_PMON_CTR1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C2_PMON_CTR1            0x00000D57


/**
  Package. Uncore C-box 2 perfmon counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C2_PMON_CTR2 (0x00000D58)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C2_PMON_CTR2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C2_PMON_CTR2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C2_PMON_CTR2 is defined as MSR_C2_PMON_CTR2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C2_PMON_CTR2            0x00000D58


/**
  Package. Uncore C-box 2 perfmon counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C2_PMON_CTR3 (0x00000D59)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C2_PMON_CTR3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C2_PMON_CTR3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C2_PMON_CTR3 is defined as MSR_C2_PMON_CTR3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C2_PMON_CTR3            0x00000D59


/**
  Package. Uncore C-box 3 perfmon local box wide control.

  @param  ECX  MSR_SANDY_BRIDGE_C3_PMON_BOX_CTL (0x00000D64)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C3_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C3_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C3_PMON_BOX_CTL is defined as MSR_C3_PMON_BOX_CTL in SDM.
**/
#define MSR_SANDY_BRIDGE_C3_PMON_BOX_CTL         0x00000D64


/**
  Package. Uncore C-box 3 perfmon event select for C-box 3 counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL0 (0x00000D70)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL0 is defined as MSR_C3_PMON_EVNTSEL0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL0        0x00000D70


/**
  Package. Uncore C-box 3 perfmon event select for C-box 3 counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL1 (0x00000D71)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL1 is defined as MSR_C3_PMON_EVNTSEL1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL1        0x00000D71


/**
  Package. Uncore C-box 3 perfmon event select for C-box 3 counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL2 (0x00000D72)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL2 is defined as MSR_C3_PMON_EVNTSEL2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL2        0x00000D72


/**
  Package. Uncore C-box 3 perfmon event select for C-box 3 counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL3 (0x00000D73)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL3 is defined as MSR_C3_PMON_EVNTSEL3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C3_PMON_EVNTSEL3        0x00000D73


/**
  Package. Uncore C-box 3 perfmon box wide filter.

  @param  ECX  MSR_SANDY_BRIDGE_C3_PMON_BOX_FILTER (0x00000D74)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C3_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C3_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C3_PMON_BOX_FILTER is defined as MSR_C3_PMON_BOX_FILTER in SDM.
**/
#define MSR_SANDY_BRIDGE_C3_PMON_BOX_FILTER      0x00000D74


/**
  Package. Uncore C-box 3 perfmon counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C3_PMON_CTR0 (0x00000D76)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C3_PMON_CTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C3_PMON_CTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C3_PMON_CTR0 is defined as MSR_C3_PMON_CTR0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C3_PMON_CTR0            0x00000D76


/**
  Package. Uncore C-box 3 perfmon counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C3_PMON_CTR1 (0x00000D77)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C3_PMON_CTR1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C3_PMON_CTR1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C3_PMON_CTR1 is defined as MSR_C3_PMON_CTR1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C3_PMON_CTR1            0x00000D77


/**
  Package. Uncore C-box 3 perfmon counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C3_PMON_CTR2 (0x00000D78)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C3_PMON_CTR2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C3_PMON_CTR2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C3_PMON_CTR2 is defined as MSR_C3_PMON_CTR2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C3_PMON_CTR2            0x00000D78


/**
  Package. Uncore C-box 3 perfmon counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C3_PMON_CTR3 (0x00000D79)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C3_PMON_CTR3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C3_PMON_CTR3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C3_PMON_CTR3 is defined as MSR_C3_PMON_CTR3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C3_PMON_CTR3            0x00000D79


/**
  Package. Uncore C-box 4 perfmon local box wide control.

  @param  ECX  MSR_SANDY_BRIDGE_C4_PMON_BOX_CTL (0x00000D84)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C4_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C4_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C4_PMON_BOX_CTL is defined as MSR_C4_PMON_BOX_CTL in SDM.
**/
#define MSR_SANDY_BRIDGE_C4_PMON_BOX_CTL         0x00000D84


/**
  Package. Uncore C-box 4 perfmon event select for C-box 4 counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL0 (0x00000D90)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL0 is defined as MSR_C4_PMON_EVNTSEL0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL0        0x00000D90


/**
  Package. Uncore C-box 4 perfmon event select for C-box 4 counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL1 (0x00000D91)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL1 is defined as MSR_C4_PMON_EVNTSEL1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL1        0x00000D91


/**
  Package. Uncore C-box 4 perfmon event select for C-box 4 counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL2 (0x00000D92)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL2 is defined as MSR_C4_PMON_EVNTSEL2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL2        0x00000D92


/**
  Package. Uncore C-box 4 perfmon event select for C-box 4 counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL3 (0x00000D93)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL3 is defined as MSR_C4_PMON_EVNTSEL3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C4_PMON_EVNTSEL3        0x00000D93


/**
  Package. Uncore C-box 4 perfmon box wide filter.

  @param  ECX  MSR_SANDY_BRIDGE_C4_PMON_BOX_FILTER (0x00000D94)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C4_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C4_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C4_PMON_BOX_FILTER is defined as MSR_C4_PMON_BOX_FILTER in SDM.
**/
#define MSR_SANDY_BRIDGE_C4_PMON_BOX_FILTER      0x00000D94


/**
  Package. Uncore C-box 4 perfmon counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C4_PMON_CTR0 (0x00000D96)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C4_PMON_CTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C4_PMON_CTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C4_PMON_CTR0 is defined as MSR_C4_PMON_CTR0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C4_PMON_CTR0            0x00000D96


/**
  Package. Uncore C-box 4 perfmon counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C4_PMON_CTR1 (0x00000D97)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C4_PMON_CTR1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C4_PMON_CTR1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C4_PMON_CTR1 is defined as MSR_C4_PMON_CTR1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C4_PMON_CTR1            0x00000D97


/**
  Package. Uncore C-box 4 perfmon counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C4_PMON_CTR2 (0x00000D98)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C4_PMON_CTR2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C4_PMON_CTR2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C4_PMON_CTR2 is defined as MSR_C4_PMON_CTR2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C4_PMON_CTR2            0x00000D98


/**
  Package. Uncore C-box 4 perfmon counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C4_PMON_CTR3 (0x00000D99)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C4_PMON_CTR3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C4_PMON_CTR3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C4_PMON_CTR3 is defined as MSR_C4_PMON_CTR3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C4_PMON_CTR3            0x00000D99


/**
  Package. Uncore C-box 5 perfmon local box wide control.

  @param  ECX  MSR_SANDY_BRIDGE_C5_PMON_BOX_CTL (0x00000DA4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C5_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C5_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C5_PMON_BOX_CTL is defined as MSR_C5_PMON_BOX_CTL in SDM.
**/
#define MSR_SANDY_BRIDGE_C5_PMON_BOX_CTL         0x00000DA4


/**
  Package. Uncore C-box 5 perfmon event select for C-box 5 counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL0 (0x00000DB0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL0 is defined as MSR_C5_PMON_EVNTSEL0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL0        0x00000DB0


/**
  Package. Uncore C-box 5 perfmon event select for C-box 5 counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL1 (0x00000DB1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL1 is defined as MSR_C5_PMON_EVNTSEL1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL1        0x00000DB1


/**
  Package. Uncore C-box 5 perfmon event select for C-box 5 counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL2 (0x00000DB2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL2 is defined as MSR_C5_PMON_EVNTSEL2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL2        0x00000DB2


/**
  Package. Uncore C-box 5 perfmon event select for C-box 5 counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL3 (0x00000DB3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL3 is defined as MSR_C5_PMON_EVNTSEL3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C5_PMON_EVNTSEL3        0x00000DB3


/**
  Package. Uncore C-box 5 perfmon box wide filter.

  @param  ECX  MSR_SANDY_BRIDGE_C5_PMON_BOX_FILTER (0x00000DB4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C5_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C5_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C5_PMON_BOX_FILTER is defined as MSR_C5_PMON_BOX_FILTER in SDM.
**/
#define MSR_SANDY_BRIDGE_C5_PMON_BOX_FILTER      0x00000DB4


/**
  Package. Uncore C-box 5 perfmon counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C5_PMON_CTR0 (0x00000DB6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C5_PMON_CTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C5_PMON_CTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C5_PMON_CTR0 is defined as MSR_C5_PMON_CTR0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C5_PMON_CTR0            0x00000DB6


/**
  Package. Uncore C-box 5 perfmon counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C5_PMON_CTR1 (0x00000DB7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C5_PMON_CTR1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C5_PMON_CTR1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C5_PMON_CTR1 is defined as MSR_C5_PMON_CTR1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C5_PMON_CTR1            0x00000DB7


/**
  Package. Uncore C-box 5 perfmon counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C5_PMON_CTR2 (0x00000DB8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C5_PMON_CTR2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C5_PMON_CTR2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C5_PMON_CTR2 is defined as MSR_C5_PMON_CTR2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C5_PMON_CTR2            0x00000DB8


/**
  Package. Uncore C-box 5 perfmon counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C5_PMON_CTR3 (0x00000DB9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C5_PMON_CTR3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C5_PMON_CTR3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C5_PMON_CTR3 is defined as MSR_C5_PMON_CTR3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C5_PMON_CTR3            0x00000DB9


/**
  Package. Uncore C-box 6 perfmon local box wide control.

  @param  ECX  MSR_SANDY_BRIDGE_C6_PMON_BOX_CTL (0x00000DC4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C6_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C6_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C6_PMON_BOX_CTL is defined as MSR_C6_PMON_BOX_CTL in SDM.
**/
#define MSR_SANDY_BRIDGE_C6_PMON_BOX_CTL         0x00000DC4


/**
  Package. Uncore C-box 6 perfmon event select for C-box 6 counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL0 (0x00000DD0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL0 is defined as MSR_C6_PMON_EVNTSEL0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL0        0x00000DD0


/**
  Package. Uncore C-box 6 perfmon event select for C-box 6 counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL1 (0x00000DD1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL1 is defined as MSR_C6_PMON_EVNTSEL1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL1        0x00000DD1


/**
  Package. Uncore C-box 6 perfmon event select for C-box 6 counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL2 (0x00000DD2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL2 is defined as MSR_C6_PMON_EVNTSEL2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL2        0x00000DD2


/**
  Package. Uncore C-box 6 perfmon event select for C-box 6 counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL3 (0x00000DD3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL3 is defined as MSR_C6_PMON_EVNTSEL3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C6_PMON_EVNTSEL3        0x00000DD3


/**
  Package. Uncore C-box 6 perfmon box wide filter.

  @param  ECX  MSR_SANDY_BRIDGE_C6_PMON_BOX_FILTER (0x00000DD4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C6_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C6_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C6_PMON_BOX_FILTER is defined as MSR_C6_PMON_BOX_FILTER in SDM.
**/
#define MSR_SANDY_BRIDGE_C6_PMON_BOX_FILTER      0x00000DD4


/**
  Package. Uncore C-box 6 perfmon counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C6_PMON_CTR0 (0x00000DD6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C6_PMON_CTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C6_PMON_CTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C6_PMON_CTR0 is defined as MSR_C6_PMON_CTR0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C6_PMON_CTR0            0x00000DD6


/**
  Package. Uncore C-box 6 perfmon counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C6_PMON_CTR1 (0x00000DD7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C6_PMON_CTR1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C6_PMON_CTR1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C6_PMON_CTR1 is defined as MSR_C6_PMON_CTR1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C6_PMON_CTR1            0x00000DD7


/**
  Package. Uncore C-box 6 perfmon counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C6_PMON_CTR2 (0x00000DD8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C6_PMON_CTR2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C6_PMON_CTR2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C6_PMON_CTR2 is defined as MSR_C6_PMON_CTR2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C6_PMON_CTR2            0x00000DD8


/**
  Package. Uncore C-box 6 perfmon counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C6_PMON_CTR3 (0x00000DD9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C6_PMON_CTR3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C6_PMON_CTR3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C6_PMON_CTR3 is defined as MSR_C6_PMON_CTR3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C6_PMON_CTR3            0x00000DD9


/**
  Package. Uncore C-box 7 perfmon local box wide control.

  @param  ECX  MSR_SANDY_BRIDGE_C7_PMON_BOX_CTL (0x00000DE4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C7_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C7_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C7_PMON_BOX_CTL is defined as MSR_C7_PMON_BOX_CTL in SDM.
**/
#define MSR_SANDY_BRIDGE_C7_PMON_BOX_CTL         0x00000DE4


/**
  Package. Uncore C-box 7 perfmon event select for C-box 7 counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL0 (0x00000DF0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL0 is defined as MSR_C7_PMON_EVNTSEL0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL0        0x00000DF0


/**
  Package. Uncore C-box 7 perfmon event select for C-box 7 counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL1 (0x00000DF1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL1 is defined as MSR_C7_PMON_EVNTSEL1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL1        0x00000DF1


/**
  Package. Uncore C-box 7 perfmon event select for C-box 7 counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL2 (0x00000DF2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL2 is defined as MSR_C7_PMON_EVNTSEL2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL2        0x00000DF2


/**
  Package. Uncore C-box 7 perfmon event select for C-box 7 counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL3 (0x00000DF3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL3 is defined as MSR_C7_PMON_EVNTSEL3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C7_PMON_EVNTSEL3        0x00000DF3


/**
  Package. Uncore C-box 7 perfmon box wide filter.

  @param  ECX  MSR_SANDY_BRIDGE_C7_PMON_BOX_FILTER (0x00000DF4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C7_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C7_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C7_PMON_BOX_FILTER is defined as MSR_C7_PMON_BOX_FILTER in SDM.
**/
#define MSR_SANDY_BRIDGE_C7_PMON_BOX_FILTER      0x00000DF4


/**
  Package. Uncore C-box 7 perfmon counter 0.

  @param  ECX  MSR_SANDY_BRIDGE_C7_PMON_CTR0 (0x00000DF6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C7_PMON_CTR0);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C7_PMON_CTR0, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C7_PMON_CTR0 is defined as MSR_C7_PMON_CTR0 in SDM.
**/
#define MSR_SANDY_BRIDGE_C7_PMON_CTR0            0x00000DF6


/**
  Package. Uncore C-box 7 perfmon counter 1.

  @param  ECX  MSR_SANDY_BRIDGE_C7_PMON_CTR1 (0x00000DF7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C7_PMON_CTR1);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C7_PMON_CTR1, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C7_PMON_CTR1 is defined as MSR_C7_PMON_CTR1 in SDM.
**/
#define MSR_SANDY_BRIDGE_C7_PMON_CTR1            0x00000DF7


/**
  Package. Uncore C-box 7 perfmon counter 2.

  @param  ECX  MSR_SANDY_BRIDGE_C7_PMON_CTR2 (0x00000DF8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C7_PMON_CTR2);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C7_PMON_CTR2, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C7_PMON_CTR2 is defined as MSR_C7_PMON_CTR2 in SDM.
**/
#define MSR_SANDY_BRIDGE_C7_PMON_CTR2            0x00000DF8


/**
  Package. Uncore C-box 7 perfmon counter 3.

  @param  ECX  MSR_SANDY_BRIDGE_C7_PMON_CTR3 (0x00000DF9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SANDY_BRIDGE_C7_PMON_CTR3);
  AsmWriteMsr64 (MSR_SANDY_BRIDGE_C7_PMON_CTR3, Msr);
  @endcode
  @note MSR_SANDY_BRIDGE_C7_PMON_CTR3 is defined as MSR_C7_PMON_CTR3 in SDM.
**/
#define MSR_SANDY_BRIDGE_C7_PMON_CTR3            0x00000DF9

#endif
