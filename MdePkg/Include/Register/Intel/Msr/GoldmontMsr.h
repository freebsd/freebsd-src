/** @file
  MSR Definitions for Intel Atom processors based on the Goldmont microarchitecture.

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

#ifndef __GOLDMONT_MSR_H__
#define __GOLDMONT_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>

/**
  Is Intel Atom processors based on the Goldmont microarchitecture?

  @param   DisplayFamily  Display Family ID
  @param   DisplayModel   Display Model ID

  @retval  TRUE   Yes, it is.
  @retval  FALSE  No, it isn't.
**/
#define IS_GOLDMONT_PROCESSOR(DisplayFamily, DisplayModel) \
  (DisplayFamily == 0x06 && \
   (                        \
    DisplayModel == 0x5C    \
    )                       \
   )

/**
  Core. Control Features in Intel 64Processor (R/W).

  @param  ECX  MSR_GOLDMONT_FEATURE_CONTROL (0x0000003A)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_FEATURE_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_FEATURE_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_FEATURE_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_FEATURE_CONTROL);
  AsmWriteMsr64 (MSR_GOLDMONT_FEATURE_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_FEATURE_CONTROL is defined as IA32_FEATURE_CONTROL in SDM.
**/
#define MSR_GOLDMONT_FEATURE_CONTROL             0x0000003A

/**
  MSR information returned for MSR index #MSR_GOLDMONT_FEATURE_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Lock bit (R/WL)
    ///
    UINT32  Lock:1;
    ///
    /// [Bit 1] Enable VMX inside SMX operation (R/WL)
    ///
    UINT32  EnableVmxInsideSmx:1;
    ///
    /// [Bit 2] Enable VMX outside SMX operation (R/WL)
    ///
    UINT32  EnableVmxOutsideSmx:1;
    UINT32  Reserved1:5;
    ///
    /// [Bits 14:8] SENTER local function enables (R/WL)
    ///
    UINT32  SenterLocalFunctionEnables:7;
    ///
    /// [Bit 15] SENTER global functions enable (R/WL)
    ///
    UINT32  SenterGlobalEnable:1;
    UINT32  Reserved2:2;
    ///
    /// [Bit 18] SGX global functions enable (R/WL)
    ///
    UINT32  SgxEnable:1;
    UINT32  Reserved3:13;
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
} MSR_GOLDMONT_FEATURE_CONTROL_REGISTER;


/**
  Package. See http://biosbits.org.

  @param  ECX  MSR_GOLDMONT_PLATFORM_INFO (0x000000CE)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_PLATFORM_INFO_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_PLATFORM_INFO_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_PLATFORM_INFO_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_PLATFORM_INFO);
  AsmWriteMsr64 (MSR_GOLDMONT_PLATFORM_INFO, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_PLATFORM_INFO is defined as MSR_PLATFORM_INFO in SDM.
**/
#define MSR_GOLDMONT_PLATFORM_INFO               0x000000CE

/**
  MSR information returned for MSR index #MSR_GOLDMONT_PLATFORM_INFO
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
    ///
    /// [Bit 30] Package. Programmable TJ OFFSET (R/O)  When set to 1,
    /// indicates that MSR_TEMPERATURE_TARGET.[27:24] is valid and writable to
    /// specify an temperature offset.
    ///
    UINT32  TJOFFSET:1;
    UINT32  Reserved3:1;
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
} MSR_GOLDMONT_PLATFORM_INFO_REGISTER;


/**
  Core. C-State Configuration Control (R/W)  Note: C-state values are
  processor specific C-state code names, unrelated to MWAIT extension C-state
  parameters or ACPI CStates. See http://biosbits.org.

  @param  ECX  MSR_GOLDMONT_PKG_CST_CONFIG_CONTROL (0x000000E2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type
               MSR_GOLDMONT_PKG_CST_CONFIG_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type
               MSR_GOLDMONT_PKG_CST_CONFIG_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_PKG_CST_CONFIG_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_PKG_CST_CONFIG_CONTROL);
  AsmWriteMsr64 (MSR_GOLDMONT_PKG_CST_CONFIG_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_PKG_CST_CONFIG_CONTROL is defined as MSR_PKG_CST_CONFIG_CONTROL in SDM.
**/
#define MSR_GOLDMONT_PKG_CST_CONFIG_CONTROL      0x000000E2

/**
  MSR information returned for MSR index #MSR_GOLDMONT_PKG_CST_CONFIG_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 3:0] Package C-State Limit (R/W)  Specifies the lowest
    /// processor-specific C-state code name (consuming the least power). for
    /// the package. The default is set as factory-configured package C-state
    /// limit. The following C-state code name encodings are supported: 0000b:
    /// No limit 0001b: C1 0010b: C3 0011b: C6 0100b: C7 0101b: C7S 0110b: C8
    /// 0111b: C9 1000b: C10.
    ///
    UINT32  Limit:4;
    UINT32  Reserved1:6;
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
    UINT32  Reserved3:16;
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
} MSR_GOLDMONT_PKG_CST_CONFIG_CONTROL_REGISTER;


/**
  Core. Enhanced SMM Capabilities (SMM-RO) Reports SMM capability Enhancement.
  Accessible only while in SMM.

  @param  ECX  MSR_GOLDMONT_SMM_MCA_CAP (0x0000017D)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_SMM_MCA_CAP_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_SMM_MCA_CAP_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_SMM_MCA_CAP_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_SMM_MCA_CAP);
  AsmWriteMsr64 (MSR_GOLDMONT_SMM_MCA_CAP, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_SMM_MCA_CAP is defined as MSR_SMM_MCA_CAP in SDM.
**/
#define MSR_GOLDMONT_SMM_MCA_CAP                 0x0000017D

/**
  MSR information returned for MSR index #MSR_GOLDMONT_SMM_MCA_CAP
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:32;
    UINT32  Reserved2:26;
    ///
    /// [Bit 58] SMM_Code_Access_Chk (SMM-RO) If set to 1 indicates that the
    /// SMM code access restriction is supported and the
    /// MSR_SMM_FEATURE_CONTROL is supported.
    ///
    UINT32  SMM_Code_Access_Chk:1;
    ///
    /// [Bit 59] Long_Flow_Indication (SMM-RO) If set to 1 indicates that the
    /// SMM long flow indicator is supported and the MSR_SMM_DELAYED is
    /// supported.
    ///
    UINT32  Long_Flow_Indication:1;
    UINT32  Reserved3:4;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_GOLDMONT_SMM_MCA_CAP_REGISTER;


/**
  Enable Misc. Processor Features (R/W)  Allows a variety of processor
  functions to be enabled and disabled.

  @param  ECX  MSR_GOLDMONT_IA32_MISC_ENABLE (0x000001A0)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_IA32_MISC_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_IA32_MISC_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_IA32_MISC_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_IA32_MISC_ENABLE);
  AsmWriteMsr64 (MSR_GOLDMONT_IA32_MISC_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_IA32_MISC_ENABLE is defined as IA32_MISC_ENABLE in SDM.
**/
#define MSR_GOLDMONT_IA32_MISC_ENABLE            0x000001A0

/**
  MSR information returned for MSR index #MSR_GOLDMONT_IA32_MISC_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Core. Fast-Strings Enable See Table 2-2.
    ///
    UINT32  FastStrings:1;
    UINT32  Reserved1:2;
    ///
    /// [Bit 3] Package. Automatic Thermal Control Circuit Enable (R/W) See
    /// Table 2-2. Default value is 1.
    ///
    UINT32  AutomaticThermalControlCircuit:1;
    UINT32  Reserved2:3;
    ///
    /// [Bit 7] Core. Performance Monitoring Available (R) See Table 2-2.
    ///
    UINT32  PerformanceMonitoring:1;
    UINT32  Reserved3:3;
    ///
    /// [Bit 11] Core. Branch Trace Storage Unavailable (RO) See Table 2-2.
    ///
    UINT32  BTS:1;
    ///
    /// [Bit 12] Core. Processor Event Based Sampling Unavailable (RO) See
    /// Table 2-2.
    ///
    UINT32  PEBS:1;
    UINT32  Reserved4:3;
    ///
    /// [Bit 16] Package. Enhanced Intel SpeedStep Technology Enable (R/W) See
    /// Table 2-2.
    ///
    UINT32  EIST:1;
    UINT32  Reserved5:1;
    ///
    /// [Bit 18] Core. ENABLE MONITOR FSM (R/W) See Table 2-2.
    ///
    UINT32  MONITOR:1;
    UINT32  Reserved6:3;
    ///
    /// [Bit 22] Core. Limit CPUID Maxval (R/W) See Table 2-2.
    ///
    UINT32  LimitCpuidMaxval:1;
    ///
    /// [Bit 23] Package. xTPR Message Disable (R/W) See Table 2-2.
    ///
    UINT32  xTPR_Message_Disable:1;
    UINT32  Reserved7:8;
    UINT32  Reserved8:2;
    ///
    /// [Bit 34] Core. XD Bit Disable (R/W) See Table 2-2.
    ///
    UINT32  XD:1;
    UINT32  Reserved9:3;
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
    UINT32  Reserved10:25;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_GOLDMONT_IA32_MISC_ENABLE_REGISTER;


/**
  Miscellaneous Feature Control (R/W).

  @param  ECX  MSR_GOLDMONT_MISC_FEATURE_CONTROL (0x000001A4)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_MISC_FEATURE_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_MISC_FEATURE_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_MISC_FEATURE_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_MISC_FEATURE_CONTROL);
  AsmWriteMsr64 (MSR_GOLDMONT_MISC_FEATURE_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_MISC_FEATURE_CONTROL is defined as MSR_MISC_FEATURE_CONTROL in SDM.
**/
#define MSR_GOLDMONT_MISC_FEATURE_CONTROL        0x000001A4

/**
  MSR information returned for MSR index #MSR_GOLDMONT_MISC_FEATURE_CONTROL
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
    UINT32  Reserved1:1;
    ///
    /// [Bit 2] Core. DCU Hardware Prefetcher Disable (R/W)  If 1, disables
    /// the L1 data cache prefetcher, which fetches the next cache line into
    /// L1 data cache.
    ///
    UINT32  DCUHardwarePrefetcherDisable:1;
    UINT32  Reserved2:29;
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
} MSR_GOLDMONT_MISC_FEATURE_CONTROL_REGISTER;


/**
  Package. See http://biosbits.org.

  @param  ECX  MSR_GOLDMONT_MISC_PWR_MGMT (0x000001AA)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_MISC_PWR_MGMT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_MISC_PWR_MGMT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_MISC_PWR_MGMT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_MISC_PWR_MGMT);
  AsmWriteMsr64 (MSR_GOLDMONT_MISC_PWR_MGMT, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_MISC_PWR_MGMT is defined as MSR_MISC_PWR_MGMT in SDM.
**/
#define MSR_GOLDMONT_MISC_PWR_MGMT               0x000001AA

/**
  MSR information returned for MSR index #MSR_GOLDMONT_MISC_PWR_MGMT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] EIST Hardware Coordination Disable (R/W) When 0, enables
    /// hardware coordination of Enhanced Intel Speedstep Technology request
    /// from processor cores; When 1, disables hardware coordination of
    /// Enhanced Intel Speedstep Technology requests.
    ///
    UINT32  EISTHardwareCoordinationDisable:1;
    UINT32  Reserved1:21;
    ///
    /// [Bit 22] Thermal Interrupt Coordination Enable (R/W)  If set, then
    /// thermal interrupt on one core is routed to all cores.
    ///
    UINT32  ThermalInterruptCoordinationEnable:1;
    UINT32  Reserved2:9;
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
} MSR_GOLDMONT_MISC_PWR_MGMT_REGISTER;


/**
  Package. Maximum Ratio Limit of Turbo Mode by Core Groups (RW) Specifies
  Maximum Ratio Limit for each Core Group. Max ratio for groups with more
  cores must decrease monotonically. For groups with less than 4 cores, the
  max ratio must be 32 or less. For groups with 4-5 cores, the max ratio must
  be 22 or less. For groups with more than 5 cores, the max ratio must be 16
  or less..

  @param  ECX  MSR_GOLDMONT_TURBO_RATIO_LIMIT (0x000001AD)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_TURBO_RATIO_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_TURBO_RATIO_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_TURBO_RATIO_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_TURBO_RATIO_LIMIT);
  AsmWriteMsr64 (MSR_GOLDMONT_TURBO_RATIO_LIMIT, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_TURBO_RATIO_LIMIT is defined as MSR_TURBO_RATIO_LIMIT in SDM.
**/
#define MSR_GOLDMONT_TURBO_RATIO_LIMIT           0x000001AD

/**
  MSR information returned for MSR index #MSR_GOLDMONT_TURBO_RATIO_LIMIT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] Package. Maximum Ratio Limit for Active cores in Group 0
    /// Maximum turbo ratio limit when number of active cores is less or equal
    /// to Group 0 threshold.
    ///
    UINT32  MaxRatioLimitGroup0:8;
    ///
    /// [Bits 15:8] Package. Maximum Ratio Limit for Active cores in Group 1
    /// Maximum turbo ratio limit when number of active cores is less or equal
    /// to Group 1 threshold and greater than Group 0 threshold.
    ///
    UINT32  MaxRatioLimitGroup1:8;
    ///
    /// [Bits 23:16] Package. Maximum Ratio Limit for Active cores in Group 2
    /// Maximum turbo ratio limit when number of active cores is less or equal
    /// to Group 2 threshold and greater than Group 1 threshold.
    ///
    UINT32  MaxRatioLimitGroup2:8;
    ///
    /// [Bits 31:24] Package. Maximum Ratio Limit for Active cores in Group 3
    /// Maximum turbo ratio limit when number of active cores is less or equal
    /// to Group 3 threshold and greater than Group 2 threshold.
    ///
    UINT32  MaxRatioLimitGroup3:8;
    ///
    /// [Bits 39:32] Package. Maximum Ratio Limit for Active cores in Group 4
    /// Maximum turbo ratio limit when number of active cores is less or equal
    /// to Group 4 threshold and greater than Group 3 threshold.
    ///
    UINT32  MaxRatioLimitGroup4:8;
    ///
    /// [Bits 47:40] Package. Maximum Ratio Limit for Active cores in Group 5
    /// Maximum turbo ratio limit when number of active cores is less or equal
    /// to Group 5 threshold and greater than Group 4 threshold.
    ///
    UINT32  MaxRatioLimitGroup5:8;
    ///
    /// [Bits 55:48] Package. Maximum Ratio Limit for Active cores in Group 6
    /// Maximum turbo ratio limit when number of active cores is less or equal
    /// to Group 6 threshold and greater than Group 5 threshold.
    ///
    UINT32  MaxRatioLimitGroup6:8;
    ///
    /// [Bits 63:56] Package. Maximum Ratio Limit for Active cores in Group 7
    /// Maximum turbo ratio limit when number of active cores is less or equal
    /// to Group 7 threshold and greater than Group 6 threshold.
    ///
    UINT32  MaxRatioLimitGroup7:8;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_GOLDMONT_TURBO_RATIO_LIMIT_REGISTER;


/**
  Package. Group Size of Active Cores for Turbo Mode Operation (RW) Writes of
  0 threshold is ignored.

  @param  ECX  MSR_GOLDMONT_TURBO_GROUP_CORECNT (0x000001AE)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_TURBO_GROUP_CORECNT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_TURBO_GROUP_CORECNT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_TURBO_GROUP_CORECNT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_TURBO_GROUP_CORECNT);
  AsmWriteMsr64 (MSR_GOLDMONT_TURBO_GROUP_CORECNT, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_TURBO_GROUP_CORECNT is defined as MSR_TURBO_GROUP_CORECNT in SDM.
**/
#define MSR_GOLDMONT_TURBO_GROUP_CORECNT         0x000001AE

/**
  MSR information returned for MSR index #MSR_GOLDMONT_TURBO_GROUP_CORECNT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] Package. Group 0 Core Count Threshold Maximum number of
    /// active cores to operate under Group 0 Max Turbo Ratio limit.
    ///
    UINT32  CoreCountThresholdGroup0:8;
    ///
    /// [Bits 15:8] Package. Group 1 Core Count Threshold Maximum number of
    /// active cores to operate under Group 1 Max Turbo Ratio limit. Must be
    /// greater than Group 0 Core Count.
    ///
    UINT32  CoreCountThresholdGroup1:8;
    ///
    /// [Bits 23:16] Package. Group 2 Core Count Threshold Maximum number of
    /// active cores to operate under Group 2 Max Turbo Ratio limit. Must be
    /// greater than Group 1 Core Count.
    ///
    UINT32  CoreCountThresholdGroup2:8;
    ///
    /// [Bits 31:24] Package. Group 3 Core Count Threshold Maximum number of
    /// active cores to operate under Group 3 Max Turbo Ratio limit. Must be
    /// greater than Group 2 Core Count.
    ///
    UINT32  CoreCountThresholdGroup3:8;
    ///
    /// [Bits 39:32] Package. Group 4 Core Count Threshold Maximum number of
    /// active cores to operate under Group 4 Max Turbo Ratio limit. Must be
    /// greater than Group 3 Core Count.
    ///
    UINT32  CoreCountThresholdGroup4:8;
    ///
    /// [Bits 47:40] Package. Group 5 Core Count Threshold Maximum number of
    /// active cores to operate under Group 5 Max Turbo Ratio limit. Must be
    /// greater than Group 4 Core Count.
    ///
    UINT32  CoreCountThresholdGroup5:8;
    ///
    /// [Bits 55:48] Package. Group 6 Core Count Threshold Maximum number of
    /// active cores to operate under Group 6 Max Turbo Ratio limit. Must be
    /// greater than Group 5 Core Count.
    ///
    UINT32  CoreCountThresholdGroup6:8;
    ///
    /// [Bits 63:56] Package. Group 7 Core Count Threshold Maximum number of
    /// active cores to operate under Group 7 Max Turbo Ratio limit. Must be
    /// greater than Group 6 Core Count and not less than the total number of
    /// processor cores in the package. E.g. specify 255.
    ///
    UINT32  CoreCountThresholdGroup7:8;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_GOLDMONT_TURBO_GROUP_CORECNT_REGISTER;


/**
  Core. Last Branch Record Filtering Select Register (R/W) See Section 17.9.2,
  "Filtering of Last Branch Records.".

  @param  ECX  MSR_GOLDMONT_LBR_SELECT (0x000001C8)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_LBR_SELECT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_LBR_SELECT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_LBR_SELECT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_LBR_SELECT);
  AsmWriteMsr64 (MSR_GOLDMONT_LBR_SELECT, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_LBR_SELECT is defined as MSR_LBR_SELECT in SDM.
**/
#define MSR_GOLDMONT_LBR_SELECT                  0x000001C8

/**
  MSR information returned for MSR index #MSR_GOLDMONT_LBR_SELECT
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
    ///
    /// [Bit 9] EN_CALL_STACK.
    ///
    UINT32  EN_CALL_STACK:1;
    UINT32  Reserved1:22;
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
} MSR_GOLDMONT_LBR_SELECT_REGISTER;


/**
  Core. Last Branch Record Stack TOS (R/W)  Contains an index (bits 0-4) that
  points to the MSR containing the most recent branch record. See
  MSR_LASTBRANCH_0_FROM_IP.

  @param  ECX  MSR_GOLDMONT_LASTBRANCH_TOS (0x000001C9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_LASTBRANCH_TOS);
  AsmWriteMsr64 (MSR_GOLDMONT_LASTBRANCH_TOS, Msr);
  @endcode
  @note MSR_GOLDMONT_LASTBRANCH_TOS is defined as MSR_LASTBRANCH_TOS in SDM.
**/
#define MSR_GOLDMONT_LASTBRANCH_TOS              0x000001C9


/**
  Core. Power Control Register. See http://biosbits.org.

  @param  ECX  MSR_GOLDMONT_POWER_CTL (0x000001FC)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_POWER_CTL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_POWER_CTL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_POWER_CTL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_POWER_CTL);
  AsmWriteMsr64 (MSR_GOLDMONT_POWER_CTL, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_POWER_CTL is defined as MSR_POWER_CTL in SDM.
**/
#define MSR_GOLDMONT_POWER_CTL                   0x000001FC

/**
  MSR information returned for MSR index #MSR_GOLDMONT_POWER_CTL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:1;
    ///
    /// [Bit 1] Package. C1E Enable (R/W)  When set to '1', will enable the
    /// CPU to switch to the Minimum Enhanced Intel SpeedStep Technology
    /// operating point when all execution cores enter MWAIT (C1).
    ///
    UINT32  C1EEnable:1;
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
} MSR_GOLDMONT_POWER_CTL_REGISTER;


/**
  Package. Lower 64 Bit CR_SGXOWNEREPOCH (W) Writes do not update
  CR_SGXOWNEREPOCH if CPUID.(EAX=12H, ECX=0):EAX.SGX1 is 1 on any thread in
  the package. Lower 64 bits of an 128-bit external entropy value for key
  derivation of an enclave.

  @param  ECX  MSR_GOLDMONT_SGXOWNEREPOCH0 (0x00000300)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_SGXOWNEREPOCH0);
  @endcode
  @note MSR_GOLDMONT_SGXOWNEREPOCH0 is defined as MSR_SGXOWNEREPOCH0 in SDM.
**/
#define MSR_GOLDMONT_SGXOWNEREPOCH0                   0x00000300


//
// Define MSR_GOLDMONT_SGXOWNER0 for compatibility due to name change in the SDM.
//
#define MSR_GOLDMONT_SGXOWNER0                        MSR_GOLDMONT_SGXOWNEREPOCH0


/**
  Package. Upper 64 Bit OwnerEpoch Component of SGX Key (RO). Upper 64 bits of
  an 128-bit external entropy value for key derivation of an enclave.

  @param  ECX  MSR_GOLDMONT_SGXOWNEREPOCH1 (0x00000301)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_SGXOWNEREPOCH1);
  @endcode
  @note MSR_GOLDMONT_SGXOWNEREPOCH1 is defined as MSR_SGXOWNEREPOCH1 in SDM.
**/
#define MSR_GOLDMONT_SGXOWNEREPOCH1                   0x00000301


//
// Define MSR_GOLDMONT_SGXOWNER1 for compatibility due to name change in the SDM.
//
#define MSR_GOLDMONT_SGXOWNER1                        MSR_GOLDMONT_SGXOWNEREPOCH1


/**
  Core. See Table 2-2. See Section 18.2.4, "Architectural Performance
  Monitoring Version 4.".

  @param  ECX  MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_RESET (0x00000390)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_RESET_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_RESET_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_RESET_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_RESET);
  AsmWriteMsr64 (MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_RESET, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_RESET is defined as IA32_PERF_GLOBAL_STATUS_RESET in SDM.
**/
#define MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_RESET 0x00000390

/**
  MSR information returned for MSR index
  #MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_RESET
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Set 1 to clear Ovf_PMC0.
    ///
    UINT32  Ovf_PMC0:1;
    ///
    /// [Bit 1] Set 1 to clear Ovf_PMC1.
    ///
    UINT32  Ovf_PMC1:1;
    ///
    /// [Bit 2] Set 1 to clear Ovf_PMC2.
    ///
    UINT32  Ovf_PMC2:1;
    ///
    /// [Bit 3] Set 1 to clear Ovf_PMC3.
    ///
    UINT32  Ovf_PMC3:1;
    UINT32  Reserved1:28;
    ///
    /// [Bit 32] Set 1 to clear Ovf_FixedCtr0.
    ///
    UINT32  Ovf_FixedCtr0:1;
    ///
    /// [Bit 33] Set 1 to clear Ovf_FixedCtr1.
    ///
    UINT32  Ovf_FixedCtr1:1;
    ///
    /// [Bit 34] Set 1 to clear Ovf_FixedCtr2.
    ///
    UINT32  Ovf_FixedCtr2:1;
    UINT32  Reserved2:20;
    ///
    /// [Bit 55] Set 1 to clear Trace_ToPA_PMI.
    ///
    UINT32  Trace_ToPA_PMI:1;
    UINT32  Reserved3:2;
    ///
    /// [Bit 58] Set 1 to clear LBR_Frz.
    ///
    UINT32  LBR_Frz:1;
    ///
    /// [Bit 59] Set 1 to clear CTR_Frz.
    ///
    UINT32  CTR_Frz:1;
    ///
    /// [Bit 60] Set 1 to clear ASCI.
    ///
    UINT32  ASCI:1;
    ///
    /// [Bit 61] Set 1 to clear Ovf_Uncore.
    ///
    UINT32  Ovf_Uncore:1;
    ///
    /// [Bit 62] Set 1 to clear Ovf_BufDSSAVE.
    ///
    UINT32  Ovf_BufDSSAVE:1;
    ///
    /// [Bit 63] Set 1 to clear CondChgd.
    ///
    UINT32  CondChgd:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_RESET_REGISTER;


/**
  Core. See Table 2-2. See Section 18.2.4, "Architectural Performance
  Monitoring Version 4.".

  @param  ECX  MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_SET (0x00000391)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_SET_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_SET_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_SET_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_SET);
  AsmWriteMsr64 (MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_SET, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_SET is defined as IA32_PERF_GLOBAL_STATUS_SET in SDM.
**/
#define MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_SET 0x00000391

/**
  MSR information returned for MSR index
  #MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_SET
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Set 1 to cause Ovf_PMC0 = 1.
    ///
    UINT32  Ovf_PMC0:1;
    ///
    /// [Bit 1] Set 1 to cause Ovf_PMC1 = 1.
    ///
    UINT32  Ovf_PMC1:1;
    ///
    /// [Bit 2] Set 1 to cause Ovf_PMC2 = 1.
    ///
    UINT32  Ovf_PMC2:1;
    ///
    /// [Bit 3] Set 1 to cause Ovf_PMC3 = 1.
    ///
    UINT32  Ovf_PMC3:1;
    UINT32  Reserved1:28;
    ///
    /// [Bit 32] Set 1 to cause Ovf_FixedCtr0 = 1.
    ///
    UINT32  Ovf_FixedCtr0:1;
    ///
    /// [Bit 33] Set 1 to cause Ovf_FixedCtr1 = 1.
    ///
    UINT32  Ovf_FixedCtr1:1;
    ///
    /// [Bit 34] Set 1 to cause Ovf_FixedCtr2 = 1.
    ///
    UINT32  Ovf_FixedCtr2:1;
    UINT32  Reserved2:20;
    ///
    /// [Bit 55] Set 1 to cause Trace_ToPA_PMI = 1.
    ///
    UINT32  Trace_ToPA_PMI:1;
    UINT32  Reserved3:2;
    ///
    /// [Bit 58] Set 1 to cause LBR_Frz = 1.
    ///
    UINT32  LBR_Frz:1;
    ///
    /// [Bit 59] Set 1 to cause CTR_Frz = 1.
    ///
    UINT32  CTR_Frz:1;
    ///
    /// [Bit 60] Set 1 to cause ASCI = 1.
    ///
    UINT32  ASCI:1;
    ///
    /// [Bit 61] Set 1 to cause Ovf_Uncore.
    ///
    UINT32  Ovf_Uncore:1;
    ///
    /// [Bit 62] Set 1 to cause Ovf_BufDSSAVE.
    ///
    UINT32  Ovf_BufDSSAVE:1;
    UINT32  Reserved4:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_GOLDMONT_IA32_PERF_GLOBAL_STATUS_SET_REGISTER;


/**
  Core. See Table 2-2. See Section 18.6.2.4, "Processor Event Based Sampling
  (PEBS).".

  @param  ECX  MSR_GOLDMONT_PEBS_ENABLE (0x000003F1)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_PEBS_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_PEBS_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_PEBS_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_PEBS_ENABLE);
  AsmWriteMsr64 (MSR_GOLDMONT_PEBS_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_PEBS_ENABLE is defined as MSR_PEBS_ENABLE in SDM.
**/
#define MSR_GOLDMONT_PEBS_ENABLE                 0x000003F1

/**
  MSR information returned for MSR index #MSR_GOLDMONT_PEBS_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Enable PEBS trigger and recording for the programmed event
    /// (precise or otherwise) on IA32_PMC0. (R/W).
    ///
    UINT32  Enable:1;
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
} MSR_GOLDMONT_PEBS_ENABLE_REGISTER;


/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. Package C3
  Residency Counter. (R/O) Value since last reset that this package is in
  processor-specific C3 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_GOLDMONT_PKG_C3_RESIDENCY (0x000003F8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_PKG_C3_RESIDENCY);
  AsmWriteMsr64 (MSR_GOLDMONT_PKG_C3_RESIDENCY, Msr);
  @endcode
  @note MSR_GOLDMONT_PKG_C3_RESIDENCY is defined as MSR_PKG_C3_RESIDENCY in SDM.
**/
#define MSR_GOLDMONT_PKG_C3_RESIDENCY            0x000003F8


/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. Package C6
  Residency Counter. (R/O) Value since last reset that this package is in
  processor-specific C6 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_GOLDMONT_PKG_C6_RESIDENCY (0x000003F9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_PKG_C6_RESIDENCY);
  AsmWriteMsr64 (MSR_GOLDMONT_PKG_C6_RESIDENCY, Msr);
  @endcode
  @note MSR_GOLDMONT_PKG_C6_RESIDENCY is defined as MSR_PKG_C6_RESIDENCY in SDM.
**/
#define MSR_GOLDMONT_PKG_C6_RESIDENCY            0x000003F9


/**
  Core. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. CORE C3
  Residency Counter. (R/O) Value since last reset that this core is in
  processor-specific C3 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_GOLDMONT_CORE_C3_RESIDENCY (0x000003FC)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_CORE_C3_RESIDENCY);
  AsmWriteMsr64 (MSR_GOLDMONT_CORE_C3_RESIDENCY, Msr);
  @endcode
  @note MSR_GOLDMONT_CORE_C3_RESIDENCY is defined as MSR_CORE_C3_RESIDENCY in SDM.
**/
#define MSR_GOLDMONT_CORE_C3_RESIDENCY           0x000003FC


/**
  Package. Enhanced SMM Feature Control (SMM-RW) Reports SMM capability
  Enhancement. Accessible only while in SMM.

  @param  ECX  MSR_GOLDMONT_SMM_FEATURE_CONTROL (0x000004E0)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_SMM_FEATURE_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_SMM_FEATURE_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_SMM_FEATURE_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_SMM_FEATURE_CONTROL);
  AsmWriteMsr64 (MSR_GOLDMONT_SMM_FEATURE_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_SMM_FEATURE_CONTROL is defined as MSR_SMM_FEATURE_CONTROL in SDM.
**/
#define MSR_GOLDMONT_SMM_FEATURE_CONTROL         0x000004E0

/**
  MSR information returned for MSR index #MSR_GOLDMONT_SMM_FEATURE_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Lock (SMM-RWO) When set to '1' locks this register from
    /// further changes.
    ///
    UINT32  Lock:1;
    UINT32  Reserved1:1;
    ///
    /// [Bit 2] SMM_Code_Chk_En (SMM-RW) This control bit is available only if
    /// MSR_SMM_MCA_CAP[58] == 1. When set to '0' (default) none of the
    /// logical processors are prevented from executing SMM code outside the
    /// ranges defined by the SMRR. When set to '1' any logical processor in
    /// the package that attempts to execute SMM code not within the ranges
    /// defined by the SMRR will assert an unrecoverable MCE.
    ///
    UINT32  SMM_Code_Chk_En:1;
    UINT32  Reserved2:29;
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
} MSR_GOLDMONT_SMM_FEATURE_CONTROL_REGISTER;


/**
  Package. SMM Delayed (SMM-RO) Reports the interruptible state of all logical
  processors in the package. Available only while in SMM and
  MSR_SMM_MCA_CAP[LONG_FLOW_INDICATION] == 1.

  @param  ECX  MSR_GOLDMONT_SMM_DELAYED (0x000004E2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_SMM_DELAYED_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_SMM_DELAYED_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_SMM_DELAYED_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_SMM_DELAYED);
  AsmWriteMsr64 (MSR_GOLDMONT_SMM_DELAYED, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_SMM_DELAYED is defined as MSR_SMM_DELAYED in SDM.
**/
#define MSR_GOLDMONT_SMM_DELAYED                 0x000004E2


/**
  Package. SMM Blocked (SMM-RO) Reports the blocked state of all logical
  processors in the package. Available only while in SMM.

  @param  ECX  MSR_GOLDMONT_SMM_BLOCKED (0x000004E3)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_SMM_BLOCKED_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_SMM_BLOCKED_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_SMM_BLOCKED_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_SMM_BLOCKED);
  AsmWriteMsr64 (MSR_GOLDMONT_SMM_BLOCKED, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_SMM_BLOCKED is defined as MSR_SMM_BLOCKED in SDM.
**/
#define MSR_GOLDMONT_SMM_BLOCKED                 0x000004E3


/**
  Core. Trace Control Register (R/W).

  @param  ECX  MSR_GOLDMONT_IA32_RTIT_CTL (0x00000570)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_IA32_RTIT_CTL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_IA32_RTIT_CTL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_IA32_RTIT_CTL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_IA32_RTIT_CTL);
  AsmWriteMsr64 (MSR_GOLDMONT_IA32_RTIT_CTL, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_IA32_RTIT_CTL is defined as IA32_RTIT_CTL in SDM.
**/
#define MSR_IA32_RTIT_CTL                        0x00000570

/**
  MSR information returned for MSR index #MSR_IA32_RTIT_CTL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] TraceEn.
    ///
    UINT32  TraceEn:1;
    ///
    /// [Bit 1] CYCEn.
    ///
    UINT32  CYCEn:1;
    ///
    /// [Bit 2] OS.
    ///
    UINT32  OS:1;
    ///
    /// [Bit 3] User.
    ///
    UINT32  User:1;
    UINT32  Reserved1:3;
    ///
    /// [Bit 7] CR3 filter.
    ///
    UINT32  CR3:1;
    ///
    /// [Bit 8] ToPA. Writing 0 will #GP if also setting TraceEn.
    ///
    UINT32  ToPA:1;
    ///
    /// [Bit 9] MTCEn.
    ///
    UINT32  MTCEn:1;
    ///
    /// [Bit 10] TSCEn.
    ///
    UINT32  TSCEn:1;
    ///
    /// [Bit 11] DisRETC.
    ///
    UINT32  DisRETC:1;
    UINT32  Reserved2:1;
    ///
    /// [Bit 13] BranchEn.
    ///
    UINT32  BranchEn:1;
    ///
    /// [Bits 17:14] MTCFreq.
    ///
    UINT32  MTCFreq:4;
    UINT32  Reserved3:1;
    ///
    /// [Bits 22:19] CYCThresh.
    ///
    UINT32  CYCThresh:4;
    UINT32  Reserved4:1;
    ///
    /// [Bits 27:24] PSBFreq.
    ///
    UINT32  PSBFreq:4;
    UINT32  Reserved5:4;
    ///
    /// [Bits 35:32] ADDR0_CFG.
    ///
    UINT32  ADDR0_CFG:4;
    ///
    /// [Bits 39:36] ADDR1_CFG.
    ///
    UINT32  ADDR1_CFG:4;
    UINT32  Reserved6:24;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_GOLDMONT_IA32_RTIT_CTL_REGISTER;


/**
  Package. Unit Multipliers used in RAPL Interfaces (R/O) See Section 14.9.1,
  "RAPL Interfaces.".

  @param  ECX  MSR_GOLDMONT_RAPL_POWER_UNIT (0x00000606)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_RAPL_POWER_UNIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_RAPL_POWER_UNIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_RAPL_POWER_UNIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_RAPL_POWER_UNIT);
  @endcode
  @note MSR_GOLDMONT_RAPL_POWER_UNIT is defined as MSR_RAPL_POWER_UNIT in SDM.
**/
#define MSR_GOLDMONT_RAPL_POWER_UNIT             0x00000606

/**
  MSR information returned for MSR index #MSR_GOLDMONT_RAPL_POWER_UNIT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 3:0] Power Units. Power related information (in Watts) is in
    /// unit of, 1W/2^PU; where PU is an unsigned integer represented by bits
    /// 3:0. Default value is 1000b, indicating power unit is in 3.9
    /// milliWatts increment.
    ///
    UINT32  PowerUnits:4;
    UINT32  Reserved1:4;
    ///
    /// [Bits 12:8] Energy Status Units. Energy related information (in
    /// Joules) is in unit of, 1Joule/ (2^ESU); where ESU is an unsigned
    /// integer represented by bits 12:8. Default value is 01110b, indicating
    /// energy unit is in 61 microJoules.
    ///
    UINT32  EnergyStatusUnits:5;
    UINT32  Reserved2:3;
    ///
    /// [Bits 19:16] Time Unit. Time related information (in seconds) is in
    /// unit of, 1S/2^TU; where TU is an unsigned integer represented by bits
    /// 19:16. Default value is 1010b, indicating power unit is in 0.977
    /// millisecond.
    ///
    UINT32  TimeUnit:4;
    UINT32  Reserved3:12;
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
} MSR_GOLDMONT_RAPL_POWER_UNIT_REGISTER;


/**
  Package. Package C3 Interrupt Response Limit (R/W)  Note: C-state values are
  processor specific C-state code names, unrelated to MWAIT extension C-state
  parameters or ACPI CStates.

  @param  ECX  MSR_GOLDMONT_PKGC3_IRTL (0x0000060A)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_PKGC3_IRTL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_PKGC3_IRTL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_PKGC3_IRTL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_PKGC3_IRTL);
  AsmWriteMsr64 (MSR_GOLDMONT_PKGC3_IRTL, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_PKGC3_IRTL is defined as MSR_PKGC3_IRTL in SDM.
**/
#define MSR_GOLDMONT_PKGC3_IRTL                  0x0000060A

/**
  MSR information returned for MSR index #MSR_GOLDMONT_PKGC3_IRTL
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
    UINT32  InterruptResponseTimeLimit:10;
    ///
    /// [Bits 12:10] Time Unit (R/W) Specifies the encoding value of time unit
    /// of the interrupt response time limit. See Table 2-19 for supported
    /// time unit encodings.
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
} MSR_GOLDMONT_PKGC3_IRTL_REGISTER;


/**
  Package. Package C6/C7S Interrupt Response Limit 1 (R/W)  This MSR defines
  the interrupt response time limit used by the processor to manage transition
  to package C6 or C7S state. Note: C-state values are processor specific
  C-state code names, unrelated to MWAIT extension C-state parameters or ACPI
  CStates.

  @param  ECX  MSR_GOLDMONT_PKGC_IRTL1 (0x0000060B)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_PKGC_IRTL1_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_PKGC_IRTL1_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_PKGC_IRTL1_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_PKGC_IRTL1);
  AsmWriteMsr64 (MSR_GOLDMONT_PKGC_IRTL1, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_PKGC_IRTL1 is defined as MSR_PKGC_IRTL1 in SDM.
**/
#define MSR_GOLDMONT_PKGC_IRTL1                  0x0000060B

/**
  MSR information returned for MSR index #MSR_GOLDMONT_PKGC_IRTL1
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 9:0] Interrupt response time limit (R/W)  Specifies the limit
    /// that should be used to decide if the package should be put into a
    /// package C6 or C7S state.
    ///
    UINT32  InterruptResponseTimeLimit:10;
    ///
    /// [Bits 12:10] Time Unit (R/W) Specifies the encoding value of time unit
    /// of the interrupt response time limit. See Table 2-19 for supported
    /// time unit encodings.
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
} MSR_GOLDMONT_PKGC_IRTL1_REGISTER;


/**
  Package. Package C7 Interrupt Response Limit 2 (R/W)  This MSR defines the
  interrupt response time limit used by the processor to manage transition to
  package C7 state. Note: C-state values are processor specific C-state code
  names, unrelated to MWAIT extension C-state parameters or ACPI CStates.

  @param  ECX  MSR_GOLDMONT_PKGC_IRTL2 (0x0000060C)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_PKGC_IRTL2_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_PKGC_IRTL2_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_PKGC_IRTL2_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_PKGC_IRTL2);
  AsmWriteMsr64 (MSR_GOLDMONT_PKGC_IRTL2, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_PKGC_IRTL2 is defined as MSR_PKGC_IRTL2 in SDM.
**/
#define MSR_GOLDMONT_PKGC_IRTL2                  0x0000060C

/**
  MSR information returned for MSR index #MSR_GOLDMONT_PKGC_IRTL2
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
    UINT32  InterruptResponseTimeLimit:10;
    ///
    /// [Bits 12:10] Time Unit (R/W) Specifies the encoding value of time unit
    /// of the interrupt response time limit. See Table 2-19 for supported
    /// time unit encodings.
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
} MSR_GOLDMONT_PKGC_IRTL2_REGISTER;


/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. Package C2
  Residency Counter. (R/O) Value since last reset that this package is in
  processor-specific C2 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_GOLDMONT_PKG_C2_RESIDENCY (0x0000060D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_PKG_C2_RESIDENCY);
  AsmWriteMsr64 (MSR_GOLDMONT_PKG_C2_RESIDENCY, Msr);
  @endcode
  @note MSR_GOLDMONT_PKG_C2_RESIDENCY is defined as MSR_PKG_C2_RESIDENCY in SDM.
**/
#define MSR_GOLDMONT_PKG_C2_RESIDENCY            0x0000060D


/**
  Package. PKG RAPL Power Limit Control (R/W) See Section 14.9.3, "Package
  RAPL Domain.".

  @param  ECX  MSR_GOLDMONT_PKG_POWER_LIMIT (0x00000610)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_PKG_POWER_LIMIT);
  AsmWriteMsr64 (MSR_GOLDMONT_PKG_POWER_LIMIT, Msr);
  @endcode
  @note MSR_GOLDMONT_PKG_POWER_LIMIT is defined as MSR_PKG_POWER_LIMIT in SDM.
**/
#define MSR_GOLDMONT_PKG_POWER_LIMIT             0x00000610


/**
  Package. PKG Energy Status (R/O) See Section 14.9.3, "Package RAPL Domain.".

  @param  ECX  MSR_GOLDMONT_PKG_ENERGY_STATUS (0x00000611)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_PKG_ENERGY_STATUS);
  @endcode
  @note MSR_GOLDMONT_PKG_ENERGY_STATUS is defined as MSR_PKG_ENERGY_STATUS in SDM.
**/
#define MSR_GOLDMONT_PKG_ENERGY_STATUS           0x00000611


/**
  Package. PKG Perf Status (R/O) See Section 14.9.3, "Package RAPL Domain.".

  @param  ECX  MSR_GOLDMONT_PKG_PERF_STATUS (0x00000613)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_PKG_PERF_STATUS);
  @endcode
  @note MSR_GOLDMONT_PKG_PERF_STATUS is defined as MSR_PKG_PERF_STATUS in SDM.
**/
#define MSR_GOLDMONT_PKG_PERF_STATUS             0x00000613


/**
  Package. PKG RAPL Parameters (R/W).

  @param  ECX  MSR_GOLDMONT_PKG_POWER_INFO (0x00000614)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_PKG_POWER_INFO_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_PKG_POWER_INFO_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_PKG_POWER_INFO_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_PKG_POWER_INFO);
  AsmWriteMsr64 (MSR_GOLDMONT_PKG_POWER_INFO, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_PKG_POWER_INFO is defined as MSR_PKG_POWER_INFO in SDM.
**/
#define MSR_GOLDMONT_PKG_POWER_INFO              0x00000614

/**
  MSR information returned for MSR index #MSR_GOLDMONT_PKG_POWER_INFO
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 14:0] Thermal Spec Power (R/W)  See Section 14.9.3, "Package
    /// RAPL Domain.".
    ///
    UINT32  ThermalSpecPower:15;
    UINT32  Reserved1:1;
    ///
    /// [Bits 30:16] Minimum Power (R/W)  See Section 14.9.3, "Package RAPL
    /// Domain.".
    ///
    UINT32  MinimumPower:15;
    UINT32  Reserved2:1;
    ///
    /// [Bits 46:32] Maximum Power (R/W)  See Section 14.9.3, "Package RAPL
    /// Domain.".
    ///
    UINT32  MaximumPower:15;
    UINT32  Reserved3:1;
    ///
    /// [Bits 54:48] Maximum Time Window (R/W)  Specified by 2^Y * (1.0 +
    /// Z/4.0) * Time_Unit, where "Y" is the unsigned integer value
    /// represented. by bits 52:48, "Z" is an unsigned integer represented by
    /// bits 54:53. "Time_Unit" is specified by the "Time Units" field of
    /// MSR_RAPL_POWER_UNIT.
    ///
    UINT32  MaximumTimeWindow:7;
    UINT32  Reserved4:9;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_GOLDMONT_PKG_POWER_INFO_REGISTER;


/**
  Package. DRAM RAPL Power Limit Control (R/W)  See Section 14.9.5, "DRAM RAPL
  Domain.".

  @param  ECX  MSR_GOLDMONT_DRAM_POWER_LIMIT (0x00000618)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_DRAM_POWER_LIMIT);
  AsmWriteMsr64 (MSR_GOLDMONT_DRAM_POWER_LIMIT, Msr);
  @endcode
  @note MSR_GOLDMONT_DRAM_POWER_LIMIT is defined as MSR_DRAM_POWER_LIMIT in SDM.
**/
#define MSR_GOLDMONT_DRAM_POWER_LIMIT            0x00000618


/**
  Package. DRAM Energy Status (R/O)  See Section 14.9.5, "DRAM RAPL Domain.".

  @param  ECX  MSR_GOLDMONT_DRAM_ENERGY_STATUS (0x00000619)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_DRAM_ENERGY_STATUS);
  @endcode
  @note MSR_GOLDMONT_DRAM_ENERGY_STATUS is defined as MSR_DRAM_ENERGY_STATUS in SDM.
**/
#define MSR_GOLDMONT_DRAM_ENERGY_STATUS          0x00000619


/**
  Package. DRAM Performance Throttling Status (R/O) See Section 14.9.5, "DRAM
  RAPL Domain.".

  @param  ECX  MSR_GOLDMONT_DRAM_PERF_STATUS (0x0000061B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_DRAM_PERF_STATUS);
  @endcode
  @note MSR_GOLDMONT_DRAM_PERF_STATUS is defined as MSR_DRAM_PERF_STATUS in SDM.
**/
#define MSR_GOLDMONT_DRAM_PERF_STATUS            0x0000061B


/**
  Package. DRAM RAPL Parameters (R/W) See Section 14.9.5, "DRAM RAPL Domain.".

  @param  ECX  MSR_GOLDMONT_DRAM_POWER_INFO (0x0000061C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_DRAM_POWER_INFO);
  AsmWriteMsr64 (MSR_GOLDMONT_DRAM_POWER_INFO, Msr);
  @endcode
  @note MSR_GOLDMONT_DRAM_POWER_INFO is defined as MSR_DRAM_POWER_INFO in SDM.
**/
#define MSR_GOLDMONT_DRAM_POWER_INFO             0x0000061C


/**
  Package. Note: C-state values are processor specific C-state code names,.
  Package C10 Residency Counter. (R/O) Value since last reset that the entire
  SOC is in an S0i3 state. Count at the same frequency as the TSC.

  @param  ECX  MSR_GOLDMONT_PKG_C10_RESIDENCY (0x00000632)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_PKG_C10_RESIDENCY);
  AsmWriteMsr64 (MSR_GOLDMONT_PKG_C10_RESIDENCY, Msr);
  @endcode
  @note MSR_GOLDMONT_PKG_C10_RESIDENCY is defined as MSR_PKG_C10_RESIDENCY in SDM.
**/
#define MSR_GOLDMONT_PKG_C10_RESIDENCY           0x00000632


/**
  Package. PP0 Energy Status (R/O)  See Section 14.9.4, "PP0/PP1 RAPL
  Domains.".

  @param  ECX  MSR_GOLDMONT_PP0_ENERGY_STATUS (0x00000639)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_PP0_ENERGY_STATUS);
  @endcode
  @note MSR_GOLDMONT_PP0_ENERGY_STATUS is defined as MSR_PP0_ENERGY_STATUS in SDM.
**/
#define MSR_GOLDMONT_PP0_ENERGY_STATUS           0x00000639


/**
  Package. PP1 Energy Status (R/O)  See Section 14.9.4, "PP0/PP1 RAPL
  Domains.".

  @param  ECX  MSR_GOLDMONT_PP1_ENERGY_STATUS (0x00000641)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_PP1_ENERGY_STATUS);
  @endcode
  @note MSR_GOLDMONT_PP1_ENERGY_STATUS is defined as MSR_PP1_ENERGY_STATUS in SDM.
**/
#define MSR_GOLDMONT_PP1_ENERGY_STATUS           0x00000641


/**
  Package. ConfigTDP Control (R/W).

  @param  ECX  MSR_GOLDMONT_TURBO_ACTIVATION_RATIO (0x0000064C)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_TURBO_ACTIVATION_RATIO_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_TURBO_ACTIVATION_RATIO_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_TURBO_ACTIVATION_RATIO_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_TURBO_ACTIVATION_RATIO);
  AsmWriteMsr64 (MSR_GOLDMONT_TURBO_ACTIVATION_RATIO, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_TURBO_ACTIVATION_RATIO is defined as MSR_TURBO_ACTIVATION_RATIO in SDM.
**/
#define MSR_GOLDMONT_TURBO_ACTIVATION_RATIO      0x0000064C

/**
  MSR information returned for MSR index #MSR_GOLDMONT_TURBO_ACTIVATION_RATIO
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] MAX_NON_TURBO_RATIO (RW/L) System BIOS can program this
    /// field.
    ///
    UINT32  MAX_NON_TURBO_RATIO:8;
    UINT32  Reserved1:23;
    ///
    /// [Bit 31] TURBO_ACTIVATION_RATIO_Lock (RW/L) When this bit is set, the
    /// content of this register is locked until a reset.
    ///
    UINT32  TURBO_ACTIVATION_RATIO_Lock:1;
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
} MSR_GOLDMONT_TURBO_ACTIVATION_RATIO_REGISTER;


/**
  Package. Indicator of Frequency Clipping in Processor Cores (R/W) (frequency
  refers to processor core frequency).

  @param  ECX  MSR_GOLDMONT_CORE_PERF_LIMIT_REASONS (0x0000064F)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_CORE_PERF_LIMIT_REASONS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_CORE_PERF_LIMIT_REASONS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_CORE_PERF_LIMIT_REASONS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_CORE_PERF_LIMIT_REASONS);
  AsmWriteMsr64 (MSR_GOLDMONT_CORE_PERF_LIMIT_REASONS, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_CORE_PERF_LIMIT_REASONS is defined as MSR_CORE_PERF_LIMIT_REASONS in SDM.
**/
#define MSR_GOLDMONT_CORE_PERF_LIMIT_REASONS     0x0000064F

/**
  MSR information returned for MSR index #MSR_GOLDMONT_CORE_PERF_LIMIT_REASONS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] PROCHOT Status (R0) When set, processor core frequency is
    /// reduced below the operating system request due to assertion of
    /// external PROCHOT.
    ///
    UINT32  PROCHOTStatus:1;
    ///
    /// [Bit 1] Thermal Status (R0) When set, frequency is reduced below the
    /// operating system request due to a thermal event.
    ///
    UINT32  ThermalStatus:1;
    ///
    /// [Bit 2] Package-Level Power Limiting PL1 Status (R0) When set,
    /// frequency is reduced below the operating system request due to
    /// package-level power limiting PL1.
    ///
    UINT32  PL1Status:1;
    ///
    /// [Bit 3] Package-Level PL2 Power Limiting Status (R0) When set,
    /// frequency is reduced below the operating system request due to
    /// package-level power limiting PL2.
    ///
    UINT32  PL2Status:1;
    UINT32  Reserved1:5;
    ///
    /// [Bit 9] Core Power Limiting Status (R0) When set, frequency is reduced
    /// below the operating system request due to domain-level power limiting.
    ///
    UINT32  PowerLimitingStatus:1;
    ///
    /// [Bit 10] VR Therm Alert Status (R0) When set, frequency is reduced
    /// below the operating system request due to a thermal alert from the
    /// Voltage Regulator.
    ///
    UINT32  VRThermAlertStatus:1;
    ///
    /// [Bit 11] Max Turbo Limit Status (R0) When set, frequency is reduced
    /// below the operating system request due to multi-core turbo limits.
    ///
    UINT32  MaxTurboLimitStatus:1;
    ///
    /// [Bit 12] Electrical Design Point Status (R0) When set, frequency is
    /// reduced below the operating system request due to electrical design
    /// point constraints (e.g. maximum electrical current consumption).
    ///
    UINT32  ElectricalDesignPointStatus:1;
    ///
    /// [Bit 13] Turbo Transition Attenuation Status (R0) When set, frequency
    /// is reduced below the operating system request due to Turbo transition
    /// attenuation. This prevents performance degradation due to frequent
    /// operating ratio changes.
    ///
    UINT32  TurboTransitionAttenuationStatus:1;
    ///
    /// [Bit 14] Maximum Efficiency Frequency Status (R0) When set, frequency
    /// is reduced below the maximum efficiency frequency.
    ///
    UINT32  MaximumEfficiencyFrequencyStatus:1;
    UINT32  Reserved2:1;
    ///
    /// [Bit 16] PROCHOT Log  When set, indicates that the PROCHOT Status bit
    /// has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  PROCHOT:1;
    ///
    /// [Bit 17] Thermal Log  When set, indicates that the Thermal Status bit
    /// has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  ThermalLog:1;
    ///
    /// [Bit 18] Package-Level PL1 Power Limiting Log  When set, indicates
    /// that the Package Level PL1 Power Limiting Status bit has asserted
    /// since the log bit was last cleared. This log bit will remain set until
    /// cleared by software writing 0.
    ///
    UINT32  PL1Log:1;
    ///
    /// [Bit 19] Package-Level PL2 Power Limiting Log When set, indicates that
    /// the Package Level PL2 Power Limiting Status bit has asserted since the
    /// log bit was last cleared. This log bit will remain set until cleared
    /// by software writing 0.
    ///
    UINT32  PL2Log:1;
    UINT32  Reserved3:5;
    ///
    /// [Bit 25] Core Power Limiting Log  When set, indicates that the Core
    /// Power Limiting Status bit has asserted since the log bit was last
    /// cleared. This log bit will remain set until cleared by software
    /// writing 0.
    ///
    UINT32  CorePowerLimitingLog:1;
    ///
    /// [Bit 26] VR Therm Alert Log  When set, indicates that the VR Therm
    /// Alert Status bit has asserted since the log bit was last cleared. This
    /// log bit will remain set until cleared by software writing 0.
    ///
    UINT32  VRThermAlertLog:1;
    ///
    /// [Bit 27] Max Turbo Limit Log When set, indicates that the Max Turbo
    /// Limit Status bit has asserted since the log bit was last cleared. This
    /// log bit will remain set until cleared by software writing 0.
    ///
    UINT32  MaxTurboLimitLog:1;
    ///
    /// [Bit 28] Electrical Design Point Log  When set, indicates that the EDP
    /// Status bit has asserted since the log bit was last cleared. This log
    /// bit will remain set until cleared by software writing 0.
    ///
    UINT32  ElectricalDesignPointLog:1;
    ///
    /// [Bit 29] Turbo Transition Attenuation Log When set, indicates that the
    /// Turbo Transition Attenuation Status bit has asserted since the log bit
    /// was last cleared. This log bit will remain set until cleared by
    /// software writing 0.
    ///
    UINT32  TurboTransitionAttenuationLog:1;
    ///
    /// [Bit 30] Maximum Efficiency Frequency Log  When set, indicates that
    /// the Maximum Efficiency Frequency Status bit has asserted since the log
    /// bit was last cleared. This log bit will remain set until cleared by
    /// software writing 0.
    ///
    UINT32  MaximumEfficiencyFrequencyLog:1;
    UINT32  Reserved4:1;
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
} MSR_GOLDMONT_CORE_PERF_LIMIT_REASONS_REGISTER;


/**
  Core. Last Branch Record n From IP (R/W) One of 32 pairs of last branch
  record registers on the last branch record stack. The From_IP part of the
  stack contains pointers to the source instruction . See also: -  Last Branch
  Record Stack TOS at 1C9H -  Section 17.6 and record format in Section
  17.4.8.1.

  @param  ECX  MSR_GOLDMONT_LASTBRANCH_n_FROM_IP
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_LASTBRANCH_FROM_IP_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_LASTBRANCH_FROM_IP_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_LASTBRANCH_FROM_IP_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_LASTBRANCH_n_FROM_IP);
  AsmWriteMsr64 (MSR_GOLDMONT_LASTBRANCH_n_FROM_IP, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_LASTBRANCH_0_FROM_IP  is defined as MSR_LASTBRANCH_0_FROM_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_1_FROM_IP  is defined as MSR_LASTBRANCH_1_FROM_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_2_FROM_IP  is defined as MSR_LASTBRANCH_2_FROM_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_3_FROM_IP  is defined as MSR_LASTBRANCH_3_FROM_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_4_FROM_IP  is defined as MSR_LASTBRANCH_4_FROM_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_5_FROM_IP  is defined as MSR_LASTBRANCH_5_FROM_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_6_FROM_IP  is defined as MSR_LASTBRANCH_6_FROM_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_7_FROM_IP  is defined as MSR_LASTBRANCH_7_FROM_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_8_FROM_IP  is defined as MSR_LASTBRANCH_8_FROM_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_9_FROM_IP  is defined as MSR_LASTBRANCH_9_FROM_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_10_FROM_IP is defined as MSR_LASTBRANCH_10_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_11_FROM_IP is defined as MSR_LASTBRANCH_11_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_12_FROM_IP is defined as MSR_LASTBRANCH_12_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_13_FROM_IP is defined as MSR_LASTBRANCH_13_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_14_FROM_IP is defined as MSR_LASTBRANCH_14_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_15_FROM_IP is defined as MSR_LASTBRANCH_15_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_16_FROM_IP is defined as MSR_LASTBRANCH_16_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_17_FROM_IP is defined as MSR_LASTBRANCH_17_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_18_FROM_IP is defined as MSR_LASTBRANCH_18_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_19_FROM_IP is defined as MSR_LASTBRANCH_19_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_20_FROM_IP is defined as MSR_LASTBRANCH_20_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_21_FROM_IP is defined as MSR_LASTBRANCH_21_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_22_FROM_IP is defined as MSR_LASTBRANCH_22_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_23_FROM_IP is defined as MSR_LASTBRANCH_23_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_24_FROM_IP is defined as MSR_LASTBRANCH_24_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_25_FROM_IP is defined as MSR_LASTBRANCH_25_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_26_FROM_IP is defined as MSR_LASTBRANCH_26_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_27_FROM_IP is defined as MSR_LASTBRANCH_27_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_28_FROM_IP is defined as MSR_LASTBRANCH_28_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_29_FROM_IP is defined as MSR_LASTBRANCH_29_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_30_FROM_IP is defined as MSR_LASTBRANCH_30_FROM_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_31_FROM_IP is defined as MSR_LASTBRANCH_31_FROM_IP in SDM.
  @{
**/
#define MSR_GOLDMONT_LASTBRANCH_0_FROM_IP        0x00000680
#define MSR_GOLDMONT_LASTBRANCH_1_FROM_IP        0x00000681
#define MSR_GOLDMONT_LASTBRANCH_2_FROM_IP        0x00000682
#define MSR_GOLDMONT_LASTBRANCH_3_FROM_IP        0x00000683
#define MSR_GOLDMONT_LASTBRANCH_4_FROM_IP        0x00000684
#define MSR_GOLDMONT_LASTBRANCH_5_FROM_IP        0x00000685
#define MSR_GOLDMONT_LASTBRANCH_6_FROM_IP        0x00000686
#define MSR_GOLDMONT_LASTBRANCH_7_FROM_IP        0x00000687
#define MSR_GOLDMONT_LASTBRANCH_8_FROM_IP        0x00000688
#define MSR_GOLDMONT_LASTBRANCH_9_FROM_IP        0x00000689
#define MSR_GOLDMONT_LASTBRANCH_10_FROM_IP       0x0000068A
#define MSR_GOLDMONT_LASTBRANCH_11_FROM_IP       0x0000068B
#define MSR_GOLDMONT_LASTBRANCH_12_FROM_IP       0x0000068C
#define MSR_GOLDMONT_LASTBRANCH_13_FROM_IP       0x0000068D
#define MSR_GOLDMONT_LASTBRANCH_14_FROM_IP       0x0000068E
#define MSR_GOLDMONT_LASTBRANCH_15_FROM_IP       0x0000068F
#define MSR_GOLDMONT_LASTBRANCH_16_FROM_IP       0x00000690
#define MSR_GOLDMONT_LASTBRANCH_17_FROM_IP       0x00000691
#define MSR_GOLDMONT_LASTBRANCH_18_FROM_IP       0x00000692
#define MSR_GOLDMONT_LASTBRANCH_19_FROM_IP       0x00000693
#define MSR_GOLDMONT_LASTBRANCH_20_FROM_IP       0x00000694
#define MSR_GOLDMONT_LASTBRANCH_21_FROM_IP       0x00000695
#define MSR_GOLDMONT_LASTBRANCH_22_FROM_IP       0x00000696
#define MSR_GOLDMONT_LASTBRANCH_23_FROM_IP       0x00000697
#define MSR_GOLDMONT_LASTBRANCH_24_FROM_IP       0x00000698
#define MSR_GOLDMONT_LASTBRANCH_25_FROM_IP       0x00000699
#define MSR_GOLDMONT_LASTBRANCH_26_FROM_IP       0x0000069A
#define MSR_GOLDMONT_LASTBRANCH_27_FROM_IP       0x0000069B
#define MSR_GOLDMONT_LASTBRANCH_28_FROM_IP       0x0000069C
#define MSR_GOLDMONT_LASTBRANCH_29_FROM_IP       0x0000069D
#define MSR_GOLDMONT_LASTBRANCH_30_FROM_IP       0x0000069E
#define MSR_GOLDMONT_LASTBRANCH_31_FROM_IP       0x0000069F
/// @}

/**
  MSR information returned for MSR indexes #MSR_GOLDMONT_LASTBRANCH_0_FROM_IP
  to #MSR_GOLDMONT_LASTBRANCH_31_FROM_IP.
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 31:0] From Linear Address (R/W).
    ///
    UINT32  FromLinearAddress:32;
    ///
    /// [Bit 47:32] From Linear Address (R/W).
    ///
    UINT32  FromLinearAddressHi:16;
    ///
    /// [Bits 62:48] Signed extension of bits 47:0.
    ///
    UINT32  SignedExtension:15;
    ///
    /// [Bit 63] Mispred.
    ///
    UINT32  Mispred:1;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_GOLDMONT_LASTBRANCH_FROM_IP_REGISTER;


/**
  Core. Last Branch Record n To IP (R/W) One of 32 pairs of last branch record
  registers on the last branch record stack. The To_IP part of the stack
  contains pointers to the Destination instruction and elapsed cycles from
  last LBR update. See also: - Section 17.6.

  @param  ECX  MSR_GOLDMONT_LASTBRANCH_n_TO_IP
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_LASTBRANCH_TO_IP_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_LASTBRANCH_TO_IP_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_LASTBRANCH_TO_IP_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_LASTBRANCH_0_TO_IP);
  AsmWriteMsr64 (MSR_GOLDMONT_LASTBRANCH_0_TO_IP, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_LASTBRANCH_0_TO_IP  is defined as MSR_LASTBRANCH_0_TO_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_1_TO_IP  is defined as MSR_LASTBRANCH_1_TO_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_2_TO_IP  is defined as MSR_LASTBRANCH_2_TO_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_3_TO_IP  is defined as MSR_LASTBRANCH_3_TO_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_4_TO_IP  is defined as MSR_LASTBRANCH_4_TO_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_5_TO_IP  is defined as MSR_LASTBRANCH_5_TO_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_6_TO_IP  is defined as MSR_LASTBRANCH_6_TO_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_7_TO_IP  is defined as MSR_LASTBRANCH_7_TO_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_8_TO_IP  is defined as MSR_LASTBRANCH_8_TO_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_9_TO_IP  is defined as MSR_LASTBRANCH_9_TO_IP  in SDM.
        MSR_GOLDMONT_LASTBRANCH_10_TO_IP is defined as MSR_LASTBRANCH_10_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_11_TO_IP is defined as MSR_LASTBRANCH_11_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_12_TO_IP is defined as MSR_LASTBRANCH_12_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_13_TO_IP is defined as MSR_LASTBRANCH_13_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_14_TO_IP is defined as MSR_LASTBRANCH_14_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_15_TO_IP is defined as MSR_LASTBRANCH_15_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_16_TO_IP is defined as MSR_LASTBRANCH_16_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_17_TO_IP is defined as MSR_LASTBRANCH_17_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_18_TO_IP is defined as MSR_LASTBRANCH_18_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_19_TO_IP is defined as MSR_LASTBRANCH_19_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_20_TO_IP is defined as MSR_LASTBRANCH_20_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_21_TO_IP is defined as MSR_LASTBRANCH_21_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_22_TO_IP is defined as MSR_LASTBRANCH_22_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_23_TO_IP is defined as MSR_LASTBRANCH_23_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_24_TO_IP is defined as MSR_LASTBRANCH_24_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_25_TO_IP is defined as MSR_LASTBRANCH_25_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_26_TO_IP is defined as MSR_LASTBRANCH_26_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_27_TO_IP is defined as MSR_LASTBRANCH_27_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_28_TO_IP is defined as MSR_LASTBRANCH_28_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_29_TO_IP is defined as MSR_LASTBRANCH_29_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_30_TO_IP is defined as MSR_LASTBRANCH_30_TO_IP in SDM.
        MSR_GOLDMONT_LASTBRANCH_31_TO_IP is defined as MSR_LASTBRANCH_31_TO_IP in SDM.
  @{
**/
#define MSR_GOLDMONT_LASTBRANCH_0_TO_IP          0x000006C0
#define MSR_GOLDMONT_LASTBRANCH_1_TO_IP          0x000006C1
#define MSR_GOLDMONT_LASTBRANCH_2_TO_IP          0x000006C2
#define MSR_GOLDMONT_LASTBRANCH_3_TO_IP          0x000006C3
#define MSR_GOLDMONT_LASTBRANCH_4_TO_IP          0x000006C4
#define MSR_GOLDMONT_LASTBRANCH_5_TO_IP          0x000006C5
#define MSR_GOLDMONT_LASTBRANCH_6_TO_IP          0x000006C6
#define MSR_GOLDMONT_LASTBRANCH_7_TO_IP          0x000006C7
#define MSR_GOLDMONT_LASTBRANCH_8_TO_IP          0x000006C8
#define MSR_GOLDMONT_LASTBRANCH_9_TO_IP          0x000006C9
#define MSR_GOLDMONT_LASTBRANCH_10_TO_IP         0x000006CA
#define MSR_GOLDMONT_LASTBRANCH_11_TO_IP         0x000006CB
#define MSR_GOLDMONT_LASTBRANCH_12_TO_IP         0x000006CC
#define MSR_GOLDMONT_LASTBRANCH_13_TO_IP         0x000006CD
#define MSR_GOLDMONT_LASTBRANCH_14_TO_IP         0x000006CE
#define MSR_GOLDMONT_LASTBRANCH_15_TO_IP         0x000006CF
#define MSR_GOLDMONT_LASTBRANCH_16_TO_IP         0x000006D0
#define MSR_GOLDMONT_LASTBRANCH_17_TO_IP         0x000006D1
#define MSR_GOLDMONT_LASTBRANCH_18_TO_IP         0x000006D2
#define MSR_GOLDMONT_LASTBRANCH_19_TO_IP         0x000006D3
#define MSR_GOLDMONT_LASTBRANCH_20_TO_IP         0x000006D4
#define MSR_GOLDMONT_LASTBRANCH_21_TO_IP         0x000006D5
#define MSR_GOLDMONT_LASTBRANCH_22_TO_IP         0x000006D6
#define MSR_GOLDMONT_LASTBRANCH_23_TO_IP         0x000006D7
#define MSR_GOLDMONT_LASTBRANCH_24_TO_IP         0x000006D8
#define MSR_GOLDMONT_LASTBRANCH_25_TO_IP         0x000006D9
#define MSR_GOLDMONT_LASTBRANCH_26_TO_IP         0x000006DA
#define MSR_GOLDMONT_LASTBRANCH_27_TO_IP         0x000006DB
#define MSR_GOLDMONT_LASTBRANCH_28_TO_IP         0x000006DC
#define MSR_GOLDMONT_LASTBRANCH_29_TO_IP         0x000006DD
#define MSR_GOLDMONT_LASTBRANCH_30_TO_IP         0x000006DE
#define MSR_GOLDMONT_LASTBRANCH_31_TO_IP         0x000006DF
/// @}

/**
  MSR information returned for MSR indexes #MSR_GOLDMONT_LASTBRANCH_0_TO_IP to
  #MSR_GOLDMONT_LASTBRANCH_31_TO_IP.
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 31:0] Target Linear Address (R/W).
    ///
    UINT32  TargetLinearAddress:32;
    ///
    /// [Bit 47:32] Target Linear Address (R/W).
    ///
    UINT32  TargetLinearAddressHi:16;
    ///
    /// [Bits 63:48] Elapsed cycles from last update to the LBR.
    ///
    UINT32  ElapsedCycles:16;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_GOLDMONT_LASTBRANCH_TO_IP_REGISTER;


/**
  Core. Resource Association Register (R/W).

  @param  ECX  MSR_GOLDMONT_IA32_PQR_ASSOC (0x00000C8F)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_IA32_PQR_ASSOC_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_IA32_PQR_ASSOC_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_IA32_PQR_ASSOC_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_IA32_PQR_ASSOC);
  AsmWriteMsr64 (MSR_GOLDMONT_IA32_PQR_ASSOC, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_IA32_PQR_ASSOC is defined as IA32_PQR_ASSOC in SDM.
**/
#define MSR_GOLDMONT_IA32_PQR_ASSOC              0x00000C8F

/**
  MSR information returned for MSR index #MSR_GOLDMONT_IA32_PQR_ASSOC
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:32;
    ///
    /// [Bits 33:32] COS (R/W).
    ///
    UINT32  COS:2;
    UINT32  Reserved2:30;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_GOLDMONT_IA32_PQR_ASSOC_REGISTER;


/**
  Module. L2 Class Of Service Mask - COS n (R/W) if CPUID.(EAX=10H,
  ECX=1):EDX.COS_MAX[15:0] >=n.

  @param  ECX  MSR_GOLDMONT_IA32_L2_QOS_MASK_n
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_IA32_L2_QOS_MASK_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_IA32_L2_QOS_MASK_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_IA32_L2_QOS_MASK_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_IA32_L2_QOS_MASK_n);
  AsmWriteMsr64 (MSR_GOLDMONT_IA32_L2_QOS_MASK_n, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_IA32_L2_QOS_MASK_0 is defined as IA32_L2_QOS_MASK_0 in SDM.
        MSR_GOLDMONT_IA32_L2_QOS_MASK_1 is defined as IA32_L2_QOS_MASK_1 in SDM.
        MSR_GOLDMONT_IA32_L2_QOS_MASK_2 is defined as IA32_L2_QOS_MASK_2 in SDM.
  @{
**/
#define MSR_GOLDMONT_IA32_L2_QOS_MASK_0          0x00000D10
#define MSR_GOLDMONT_IA32_L2_QOS_MASK_1          0x00000D11
#define MSR_GOLDMONT_IA32_L2_QOS_MASK_2          0x00000D12
/// @}

/**
  MSR information returned for MSR indexes #MSR_GOLDMONT_IA32_L2_QOS_MASK_0 to
  #MSR_GOLDMONT_IA32_L2_QOS_MASK_2.
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] CBM: Bit vector of available L2 ways for COS 0 enforcement
    ///
    UINT32  CBM:8;
    UINT32  Reserved1:24;
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
} MSR_GOLDMONT_IA32_L2_QOS_MASK_REGISTER;


/**
  Package. L2 Class Of Service Mask - COS 3 (R/W) if CPUID.(EAX=10H,
  ECX=1):EDX.COS_MAX[15:0] >=3.

  @param  ECX  MSR_GOLDMONT_IA32_L2_QOS_MASK_3
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_IA32_L2_QOS_MASK_3_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_IA32_L2_QOS_MASK_3_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_IA32_L2_QOS_MASK_3_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_IA32_L2_QOS_MASK_3);
  AsmWriteMsr64 (MSR_GOLDMONT_IA32_L2_QOS_MASK_3, Msr.Uint64);
  @endcode
  @note MSR_GOLDMONT_IA32_L2_QOS_MASK_3 is defined as IA32_L2_QOS_MASK_3 in SDM.
**/
#define MSR_GOLDMONT_IA32_L2_QOS_MASK_3          0x00000D13

/**
  MSR information returned for MSR index #MSR_GOLDMONT_IA32_L2_QOS_MASK_3.
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 19:0] CBM: Bit vector of available L2 ways for COS 0 enforcement
    ///
    UINT32  CBM:20;
    UINT32  Reserved1:12;
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
} MSR_GOLDMONT_IA32_L2_QOS_MASK_3_REGISTER;


#endif
