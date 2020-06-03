/** @file
  MSR Definitions for Intel processors based on the Ivy Bridge microarchitecture.

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

#ifndef __IVY_BRIDGE_MSR_H__
#define __IVY_BRIDGE_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>

/**
  Is Intel processors based on the Ivy Bridge microarchitecture?

  @param   DisplayFamily  Display Family ID
  @param   DisplayModel   Display Model ID

  @retval  TRUE   Yes, it is.
  @retval  FALSE  No, it isn't.
**/
#define IS_IVY_BRIDGE_PROCESSOR(DisplayFamily, DisplayModel) \
  (DisplayFamily == 0x06 && \
   (                        \
    DisplayModel == 0x3A || \
    DisplayModel == 0x3E    \
    )                       \
   )

/**
  Package. See http://biosbits.org.

  @param  ECX  MSR_IVY_BRIDGE_PLATFORM_INFO (0x000000CE)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_PLATFORM_INFO_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_PLATFORM_INFO_REGISTER.

  <b>Example usage</b>
  @code
  MSR_IVY_BRIDGE_PLATFORM_INFO_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_IVY_BRIDGE_PLATFORM_INFO);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_PLATFORM_INFO, Msr.Uint64);
  @endcode
  @note MSR_IVY_BRIDGE_PLATFORM_INFO is defined as MSR_PLATFORM_INFO in SDM.
**/
#define MSR_IVY_BRIDGE_PLATFORM_INFO             0x000000CE

/**
  MSR information returned for MSR index #MSR_IVY_BRIDGE_PLATFORM_INFO
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
    ///
    /// [Bit 32] Package. Low Power Mode Support (LPM) (R/O)  When set to 1,
    /// indicates that LPM is supported, and when set to 0, indicates LPM is
    /// not supported.
    ///
    UINT32  LowPowerModeSupport:1;
    ///
    /// [Bits 34:33] Package. Number of ConfigTDP Levels (R/O) 00: Only Base
    /// TDP level available. 01: One additional TDP level available. 02: Two
    /// additional TDP level available. 11: Reserved.
    ///
    UINT32  ConfigTDPLevels:2;
    UINT32  Reserved4:5;
    ///
    /// [Bits 47:40] Package. Maximum Efficiency Ratio (R/O)  The is the
    /// minimum ratio (maximum efficiency) that the processor can operates, in
    /// units of 100MHz.
    ///
    UINT32  MaximumEfficiencyRatio:8;
    ///
    /// [Bits 55:48] Package. Minimum Operating Ratio (R/O) Contains the
    /// minimum supported operating ratio in units of 100 MHz.
    ///
    UINT32  MinimumOperatingRatio:8;
    UINT32  Reserved5:8;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_IVY_BRIDGE_PLATFORM_INFO_REGISTER;


/**
  Core. C-State Configuration Control (R/W)  Note: C-state values are
  processor specific C-state code names, unrelated to MWAIT extension C-state
  parameters or ACPI C-States. See http://biosbits.org.

  @param  ECX  MSR_IVY_BRIDGE_PKG_CST_CONFIG_CONTROL (0x000000E2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_PKG_CST_CONFIG_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_PKG_CST_CONFIG_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_IVY_BRIDGE_PKG_CST_CONFIG_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_IVY_BRIDGE_PKG_CST_CONFIG_CONTROL);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_PKG_CST_CONFIG_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_IVY_BRIDGE_PKG_CST_CONFIG_CONTROL is defined as MSR_PKG_CST_CONFIG_CONTROL in SDM.
**/
#define MSR_IVY_BRIDGE_PKG_CST_CONFIG_CONTROL    0x000000E2

/**
  MSR information returned for MSR index #MSR_IVY_BRIDGE_PKG_CST_CONFIG_CONTROL
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
} MSR_IVY_BRIDGE_PKG_CST_CONFIG_CONTROL_REGISTER;


/**
  Package. PP0 Energy Status (R/O)  See Section 14.9.4, "PP0/PP1 RAPL
  Domains.".

  @param  ECX  MSR_IVY_BRIDGE_PP0_ENERGY_STATUS (0x00000639)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_PP0_ENERGY_STATUS);
  @endcode
  @note MSR_IVY_BRIDGE_PP0_ENERGY_STATUS is defined as MSR_PP0_ENERGY_STATUS in SDM.
**/
#define MSR_IVY_BRIDGE_PP0_ENERGY_STATUS         0x00000639


/**
  Package. Base TDP Ratio (R/O).

  @param  ECX  MSR_IVY_BRIDGE_CONFIG_TDP_NOMINAL (0x00000648)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_CONFIG_TDP_NOMINAL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_CONFIG_TDP_NOMINAL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_IVY_BRIDGE_CONFIG_TDP_NOMINAL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_IVY_BRIDGE_CONFIG_TDP_NOMINAL);
  @endcode
  @note MSR_IVY_BRIDGE_CONFIG_TDP_NOMINAL is defined as MSR_CONFIG_TDP_NOMINAL in SDM.
**/
#define MSR_IVY_BRIDGE_CONFIG_TDP_NOMINAL        0x00000648

/**
  MSR information returned for MSR index #MSR_IVY_BRIDGE_CONFIG_TDP_NOMINAL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] Config_TDP_Base Base TDP level ratio to be used for this
    /// specific processor (in units of 100 MHz).
    ///
    UINT32  Config_TDP_Base:8;
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
} MSR_IVY_BRIDGE_CONFIG_TDP_NOMINAL_REGISTER;


/**
  Package. ConfigTDP Level 1 ratio and power level (R/O).

  @param  ECX  MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL1 (0x00000649)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL1_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL1_REGISTER.

  <b>Example usage</b>
  @code
  MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL1_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL1);
  @endcode
  @note MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL1 is defined as MSR_CONFIG_TDP_LEVEL1 in SDM.
**/
#define MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL1         0x00000649

/**
  MSR information returned for MSR index #MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL1
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 14:0] PKG_TDP_LVL1. Power setting for ConfigTDP Level 1.
    ///
    UINT32  PKG_TDP_LVL1:15;
    UINT32  Reserved1:1;
    ///
    /// [Bits 23:16] Config_TDP_LVL1_Ratio. ConfigTDP level 1 ratio to be used
    /// for this specific processor.
    ///
    UINT32  Config_TDP_LVL1_Ratio:8;
    UINT32  Reserved2:8;
    ///
    /// [Bits 46:32] PKG_MAX_PWR_LVL1. Max Power setting allowed for ConfigTDP
    /// Level 1.
    ///
    UINT32  PKG_MAX_PWR_LVL1:15;
    UINT32  Reserved3:1;
    ///
    /// [Bits 62:48] PKG_MIN_PWR_LVL1. MIN Power setting allowed for ConfigTDP
    /// Level 1.
    ///
    UINT32  PKG_MIN_PWR_LVL1:15;
    UINT32  Reserved4:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL1_REGISTER;


/**
  Package. ConfigTDP Level 2 ratio and power level (R/O).

  @param  ECX  MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL2 (0x0000064A)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL2_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL2_REGISTER.

  <b>Example usage</b>
  @code
  MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL2_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL2);
  @endcode
  @note MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL2 is defined as MSR_CONFIG_TDP_LEVEL2 in SDM.
**/
#define MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL2         0x0000064A

/**
  MSR information returned for MSR index #MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL2
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 14:0] PKG_TDP_LVL2. Power setting for ConfigTDP Level 2.
    ///
    UINT32  PKG_TDP_LVL2:15;
    UINT32  Reserved1:1;
    ///
    /// [Bits 23:16] Config_TDP_LVL2_Ratio. ConfigTDP level 2 ratio to be used
    /// for this specific processor.
    ///
    UINT32  Config_TDP_LVL2_Ratio:8;
    UINT32  Reserved2:8;
    ///
    /// [Bits 46:32] PKG_MAX_PWR_LVL2. Max Power setting allowed for ConfigTDP
    /// Level 2.
    ///
    UINT32  PKG_MAX_PWR_LVL2:15;
    UINT32  Reserved3:1;
    ///
    /// [Bits 62:48] PKG_MIN_PWR_LVL2. MIN Power setting allowed for ConfigTDP
    /// Level 2.
    ///
    UINT32  PKG_MIN_PWR_LVL2:15;
    UINT32  Reserved4:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_IVY_BRIDGE_CONFIG_TDP_LEVEL2_REGISTER;


/**
  Package. ConfigTDP Control (R/W).

  @param  ECX  MSR_IVY_BRIDGE_CONFIG_TDP_CONTROL (0x0000064B)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_CONFIG_TDP_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_CONFIG_TDP_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_IVY_BRIDGE_CONFIG_TDP_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_IVY_BRIDGE_CONFIG_TDP_CONTROL);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_CONFIG_TDP_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_IVY_BRIDGE_CONFIG_TDP_CONTROL is defined as MSR_CONFIG_TDP_CONTROL in SDM.
**/
#define MSR_IVY_BRIDGE_CONFIG_TDP_CONTROL        0x0000064B

/**
  MSR information returned for MSR index #MSR_IVY_BRIDGE_CONFIG_TDP_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 1:0] TDP_LEVEL (RW/L) System BIOS can program this field.
    ///
    UINT32  TDP_LEVEL:2;
    UINT32  Reserved1:29;
    ///
    /// [Bit 31] Config_TDP_Lock (RW/L) When this bit is set, the content of
    /// this register is locked until a reset.
    ///
    UINT32  Config_TDP_Lock:1;
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
} MSR_IVY_BRIDGE_CONFIG_TDP_CONTROL_REGISTER;


/**
  Package. ConfigTDP Control (R/W).

  @param  ECX  MSR_IVY_BRIDGE_TURBO_ACTIVATION_RATIO (0x0000064C)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_TURBO_ACTIVATION_RATIO_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_TURBO_ACTIVATION_RATIO_REGISTER.

  <b>Example usage</b>
  @code
  MSR_IVY_BRIDGE_TURBO_ACTIVATION_RATIO_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_IVY_BRIDGE_TURBO_ACTIVATION_RATIO);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_TURBO_ACTIVATION_RATIO, Msr.Uint64);
  @endcode
  @note MSR_IVY_BRIDGE_TURBO_ACTIVATION_RATIO is defined as MSR_TURBO_ACTIVATION_RATIO in SDM.
**/
#define MSR_IVY_BRIDGE_TURBO_ACTIVATION_RATIO    0x0000064C

/**
  MSR information returned for MSR index #MSR_IVY_BRIDGE_TURBO_ACTIVATION_RATIO
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
} MSR_IVY_BRIDGE_TURBO_ACTIVATION_RATIO_REGISTER;


/**
  Package. Protected Processor Inventory Number Enable Control (R/W).

  @param  ECX  MSR_IVY_BRIDGE_PPIN_CTL (0x0000004E)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_PPIN_CTL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_PPIN_CTL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_IVY_BRIDGE_PPIN_CTL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_IVY_BRIDGE_PPIN_CTL);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_PPIN_CTL, Msr.Uint64);
  @endcode
  @note MSR_IVY_BRIDGE_PPIN_CTL is defined as MSR_PPIN_CTL in SDM.
**/
#define MSR_IVY_BRIDGE_PPIN_CTL                  0x0000004E

/**
  MSR information returned for MSR index #MSR_IVY_BRIDGE_PPIN_CTL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] LockOut (R/WO) Set 1to prevent further writes to MSR_PPIN_CTL.
    /// Writing 1 to MSR_PPINCTL[bit 0] is permitted only if MSR_PPIN_CTL[bit
    /// 1] is clear, Default is 0. BIOS should provide an opt-in menu to
    /// enable the user to turn on MSR_PPIN_CTL[bit 1] for privileged
    /// inventory initialization agent to access MSR_PPIN. After reading
    /// MSR_PPIN, the privileged inventory initialization agent should write
    /// '01b' to MSR_PPIN_CTL to disable further access to MSR_PPIN and
    /// prevent unauthorized modification to MSR_PPIN_CTL.
    ///
    UINT32  LockOut:1;
    ///
    /// [Bit 1] Enable_PPIN (R/W) If 1, enables MSR_PPIN to be accessible
    /// using RDMSR. Once set, attempt to write 1 to MSR_PPIN_CTL[bit 0] will
    /// cause #GP. If 0, an attempt to read MSR_PPIN will cause #GP. Default
    /// is 0.
    ///
    UINT32  Enable_PPIN:1;
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
} MSR_IVY_BRIDGE_PPIN_CTL_REGISTER;


/**
  Package. Protected Processor Inventory Number (R/O). Protected Processor
  Inventory Number (R/O) A unique value within a given CPUID
  family/model/stepping signature that a privileged inventory initialization
  agent can access to identify each physical processor, when access to
  MSR_PPIN is enabled. Access to MSR_PPIN is permitted only if
  MSR_PPIN_CTL[bits 1:0] = '10b'.

  @param  ECX  MSR_IVY_BRIDGE_PPIN (0x0000004F)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_PPIN);
  @endcode
  @note MSR_IVY_BRIDGE_PPIN is defined as MSR_PPIN in SDM.
**/
#define MSR_IVY_BRIDGE_PPIN                      0x0000004F


/**
  Package. See http://biosbits.org.

  @param  ECX  MSR_IVY_BRIDGE_PLATFORM_INFO_1 (0x000000CE)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_PLATFORM_INFO_1_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_PLATFORM_INFO_1_REGISTER.

  <b>Example usage</b>
  @code
  MSR_IVY_BRIDGE_PLATFORM_INFO_1_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_IVY_BRIDGE_PLATFORM_INFO_1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_PLATFORM_INFO_1, Msr.Uint64);
  @endcode
  @note MSR_IVY_BRIDGE_PLATFORM_INFO_1 is defined as MSR_PLATFORM_INFO_1 in SDM.
**/
#define MSR_IVY_BRIDGE_PLATFORM_INFO_1           0x000000CE

/**
  MSR information returned for MSR index #MSR_IVY_BRIDGE_PLATFORM_INFO_1
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
    UINT32  Reserved2:7;
    ///
    /// [Bit 23] Package. PPIN_CAP (R/O) When set to 1, indicates that
    /// Protected Processor Inventory Number (PPIN) capability can be enabled
    /// for privileged system inventory agent to read PPIN from MSR_PPIN. When
    /// set to 0, PPIN capability is not supported. An attempt to access
    /// MSR_PPIN_CTL or MSR_PPIN will cause #GP.
    ///
    UINT32  PPIN_CAP:1;
    UINT32  Reserved3:4;
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
    UINT32  Reserved4:1;
    UINT32  Reserved5:8;
    ///
    /// [Bits 47:40] Package. Maximum Efficiency Ratio (R/O)  The is the
    /// minimum ratio (maximum efficiency) that the processor can operates, in
    /// units of 100MHz.
    ///
    UINT32  MaximumEfficiencyRatio:8;
    UINT32  Reserved6:16;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_IVY_BRIDGE_PLATFORM_INFO_1_REGISTER;


/**
  Package. MC Bank Error Configuration (R/W).

  @param  ECX  MSR_IVY_BRIDGE_ERROR_CONTROL (0x0000017F)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_ERROR_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_ERROR_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_IVY_BRIDGE_ERROR_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_IVY_BRIDGE_ERROR_CONTROL);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_ERROR_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_IVY_BRIDGE_ERROR_CONTROL is defined as MSR_ERROR_CONTROL in SDM.
**/
#define MSR_IVY_BRIDGE_ERROR_CONTROL             0x0000017F

/**
  MSR information returned for MSR index #MSR_IVY_BRIDGE_ERROR_CONTROL
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
} MSR_IVY_BRIDGE_ERROR_CONTROL_REGISTER;


/**
  Package.

  @param  ECX  MSR_IVY_BRIDGE_TEMPERATURE_TARGET (0x000001A2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_TEMPERATURE_TARGET_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_TEMPERATURE_TARGET_REGISTER.

  <b>Example usage</b>
  @code
  MSR_IVY_BRIDGE_TEMPERATURE_TARGET_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_IVY_BRIDGE_TEMPERATURE_TARGET);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_TEMPERATURE_TARGET, Msr.Uint64);
  @endcode
  @note MSR_IVY_BRIDGE_TEMPERATURE_TARGET is defined as MSR_TEMPERATURE_TARGET in SDM.
**/
#define MSR_IVY_BRIDGE_TEMPERATURE_TARGET        0x000001A2

/**
  MSR information returned for MSR index #MSR_IVY_BRIDGE_TEMPERATURE_TARGET
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:16;
    ///
    /// [Bits 23:16] Temperature Target (RO)  The minimum temperature at which
    /// PROCHOT# will be asserted. The value is degree C.
    ///
    UINT32  TemperatureTarget:8;
    ///
    /// [Bits 27:24] TCC Activation Offset (R/W)  Specifies a temperature
    /// offset in degrees C from the temperature target (bits 23:16). PROCHOT#
    /// will assert at the offset target temperature. Write is permitted only
    /// MSR_PLATFORM_INFO.[30] is set.
    ///
    UINT32  TCCActivationOffset:4;
    UINT32  Reserved2:4;
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
} MSR_IVY_BRIDGE_TEMPERATURE_TARGET_REGISTER;


/**
  Package. Maximum Ratio Limit of Turbo Mode RO if MSR_PLATFORM_INFO.[28] = 0,
  RW if MSR_PLATFORM_INFO.[28] = 1.

  @param  ECX  MSR_IVY_BRIDGE_TURBO_RATIO_LIMIT1 (0x000001AE)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_TURBO_RATIO_LIMIT1_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_TURBO_RATIO_LIMIT1_REGISTER.

  <b>Example usage</b>
  @code
  MSR_IVY_BRIDGE_TURBO_RATIO_LIMIT1_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_IVY_BRIDGE_TURBO_RATIO_LIMIT1);
  @endcode
  @note MSR_IVY_BRIDGE_TURBO_RATIO_LIMIT1 is defined as MSR_TURBO_RATIO_LIMIT1 in SDM.
**/
#define MSR_IVY_BRIDGE_TURBO_RATIO_LIMIT1        0x000001AE

/**
  MSR information returned for MSR index #MSR_IVY_BRIDGE_TURBO_RATIO_LIMIT1
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] Package. Maximum Ratio Limit for 9C Maximum turbo ratio
    /// limit of 9 core active.
    ///
    UINT32  Maximum9C:8;
    ///
    /// [Bits 15:8] Package. Maximum Ratio Limit for 10C Maximum turbo ratio
    /// limit of 10core active.
    ///
    UINT32  Maximum10C:8;
    ///
    /// [Bits 23:16] Package. Maximum Ratio Limit for 11C Maximum turbo ratio
    /// limit of 11 core active.
    ///
    UINT32  Maximum11C:8;
    ///
    /// [Bits 31:24] Package. Maximum Ratio Limit for 12C Maximum turbo ratio
    /// limit of 12 core active.
    ///
    UINT32  Maximum12C:8;
    ///
    /// [Bits 39:32] Package. Maximum Ratio Limit for 13C Maximum turbo ratio
    /// limit of 13 core active.
    ///
    UINT32  Maximum13C:8;
    ///
    /// [Bits 47:40] Package. Maximum Ratio Limit for 14C Maximum turbo ratio
    /// limit of 14 core active.
    ///
    UINT32  Maximum14C:8;
    ///
    /// [Bits 55:48] Package. Maximum Ratio Limit for 15C Maximum turbo ratio
    /// limit of 15 core active.
    ///
    UINT32  Maximum15C:8;
    UINT32  Reserved:7;
    ///
    /// [Bit 63] Package. Semaphore for Turbo Ratio Limit Configuration If 1,
    /// the processor uses override configuration specified in
    /// MSR_TURBO_RATIO_LIMIT and MSR_TURBO_RATIO_LIMIT1. If 0, the processor
    /// uses factory-set configuration (Default).
    ///
    UINT32  TurboRatioLimitConfigurationSemaphore:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_IVY_BRIDGE_TURBO_RATIO_LIMIT1_REGISTER;


/**
  Package. Misc MAC information of Integrated I/O. (R/O) see Section 15.3.2.4.

  @param  ECX  MSR_IVY_BRIDGE_IA32_MC6_MISC (0x0000041B)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_IA32_MC6_MISC_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_IA32_MC6_MISC_REGISTER.

  <b>Example usage</b>
  @code
  MSR_IVY_BRIDGE_IA32_MC6_MISC_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_IVY_BRIDGE_IA32_MC6_MISC);
  @endcode
  @note MSR_IVY_BRIDGE_IA32_MC6_MISC is defined as IA32_MC6_MISC in SDM.
**/
#define MSR_IVY_BRIDGE_IA32_MC6_MISC             0x0000041B

/**
  MSR information returned for MSR index #MSR_IVY_BRIDGE_IA32_MC6_MISC
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 5:0] Recoverable Address LSB.
    ///
    UINT32  RecoverableAddressLSB:6;
    ///
    /// [Bits 8:6] Address Mode.
    ///
    UINT32  AddressMode:3;
    UINT32  Reserved1:7;
    ///
    /// [Bits 31:16] PCI Express Requestor ID.
    ///
    UINT32  PCIExpressRequestorID:16;
    ///
    /// [Bits 39:32] PCI Express Segment Number.
    ///
    UINT32  PCIExpressSegmentNumber:8;
    UINT32  Reserved2:24;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_IVY_BRIDGE_IA32_MC6_MISC_REGISTER;


/**
  Package. See Section 15.3.2.1, "IA32_MCi_CTL MSRs." through Section
  15.3.2.4, "IA32_MCi_MISC MSRs.".

  Bank MC29 through MC31 reports MC error from a specific CBo (core broadcast)
  and its corresponding slice of L3.

  @param  ECX  MSR_IVY_BRIDGE_IA32_MCi_CTL
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_IA32_MC29_CTL);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_IA32_MC29_CTL, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_IA32_MC29_CTL is defined as IA32_MC29_CTL in SDM.
        MSR_IVY_BRIDGE_IA32_MC30_CTL is defined as IA32_MC30_CTL in SDM.
        MSR_IVY_BRIDGE_IA32_MC31_CTL is defined as IA32_MC31_CTL in SDM.
  @{
**/
#define MSR_IVY_BRIDGE_IA32_MC29_CTL             0x00000474
#define MSR_IVY_BRIDGE_IA32_MC30_CTL             0x00000478
#define MSR_IVY_BRIDGE_IA32_MC31_CTL             0x0000047C
/// @}


/**
  Package. See Section 15.3.2.1, "IA32_MCi_CTL MSRs." through Section
  15.3.2.4, "IA32_MCi_MISC MSRs.".

  Bank MC29 through MC31 reports MC error from a specific CBo (core broadcast)
  and its corresponding slice of L3.

  @param  ECX  MSR_IVY_BRIDGE_IA32_MCi_STATUS
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_IA32_MC29_STATUS);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_IA32_MC29_STATUS, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_IA32_MC29_STATUS is defined as IA32_MC29_STATUS in SDM.
        MSR_IVY_BRIDGE_IA32_MC30_STATUS is defined as IA32_MC30_STATUS in SDM.
        MSR_IVY_BRIDGE_IA32_MC31_STATUS is defined as IA32_MC31_STATUS in SDM.
  @{
**/
#define MSR_IVY_BRIDGE_IA32_MC29_STATUS          0x00000475
#define MSR_IVY_BRIDGE_IA32_MC30_STATUS          0x00000479
#define MSR_IVY_BRIDGE_IA32_MC31_STATUS          0x0000047D
/// @}


/**
  Package. See Section 15.3.2.1, "IA32_MCi_CTL MSRs." through Section
  15.3.2.4, "IA32_MCi_MISC MSRs.".

  Bank MC29 through MC31 reports MC error from a specific CBo (core broadcast)
  and its corresponding slice of L3.

  @param  ECX  MSR_IVY_BRIDGE_IA32_MCi_ADDR
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_IA32_MC29_ADDR);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_IA32_MC29_ADDR, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_IA32_MC29_ADDR is defined as IA32_MC29_ADDR in SDM.
        MSR_IVY_BRIDGE_IA32_MC30_ADDR is defined as IA32_MC30_ADDR in SDM.
        MSR_IVY_BRIDGE_IA32_MC31_ADDR is defined as IA32_MC31_ADDR in SDM.
  @{
**/
#define MSR_IVY_BRIDGE_IA32_MC29_ADDR            0x00000476
#define MSR_IVY_BRIDGE_IA32_MC30_ADDR            0x0000047A
#define MSR_IVY_BRIDGE_IA32_MC31_ADDR            0x0000047E
/// @}


/**
  Package. See Section 15.3.2.1, "IA32_MCi_CTL MSRs." through Section
  15.3.2.4, "IA32_MCi_MISC MSRs.".

  Bank MC29 through MC31 reports MC error from a specific CBo (core broadcast)
  and its corresponding slice of L3.

  @param  ECX  MSR_IVY_BRIDGE_IA32_MCi_MISC
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_IA32_MC29_MISC);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_IA32_MC29_MISC, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_IA32_MC29_MISC is defined as IA32_MC29_MISC in SDM.
        MSR_IVY_BRIDGE_IA32_MC30_MISC is defined as IA32_MC30_MISC in SDM.
        MSR_IVY_BRIDGE_IA32_MC31_MISC is defined as IA32_MC31_MISC in SDM.
  @{
**/
#define MSR_IVY_BRIDGE_IA32_MC29_MISC            0x00000477
#define MSR_IVY_BRIDGE_IA32_MC30_MISC            0x0000047B
#define MSR_IVY_BRIDGE_IA32_MC31_MISC            0x0000047F
/// @}


/**
  Package. Package RAPL Perf Status (R/O).

  @param  ECX  MSR_IVY_BRIDGE_PKG_PERF_STATUS (0x00000613)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_PKG_PERF_STATUS);
  @endcode
  @note MSR_IVY_BRIDGE_PKG_PERF_STATUS is defined as MSR_PKG_PERF_STATUS in SDM.
**/
#define MSR_IVY_BRIDGE_PKG_PERF_STATUS           0x00000613


/**
  Package. DRAM RAPL Power Limit Control (R/W)  See Section 14.9.5, "DRAM RAPL
  Domain.".

  @param  ECX  MSR_IVY_BRIDGE_DRAM_POWER_LIMIT (0x00000618)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_DRAM_POWER_LIMIT);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_DRAM_POWER_LIMIT, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_DRAM_POWER_LIMIT is defined as MSR_DRAM_POWER_LIMIT in SDM.
**/
#define MSR_IVY_BRIDGE_DRAM_POWER_LIMIT          0x00000618


/**
  Package. DRAM Energy Status (R/O)  See Section 14.9.5, "DRAM RAPL Domain.".

  @param  ECX  MSR_IVY_BRIDGE_DRAM_ENERGY_STATUS (0x00000619)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_DRAM_ENERGY_STATUS);
  @endcode
  @note MSR_IVY_BRIDGE_DRAM_ENERGY_STATUS is defined as MSR_DRAM_ENERGY_STATUS in SDM.
**/
#define MSR_IVY_BRIDGE_DRAM_ENERGY_STATUS        0x00000619


/**
  Package. DRAM Performance Throttling Status (R/O) See Section 14.9.5, "DRAM
  RAPL Domain.".

  @param  ECX  MSR_IVY_BRIDGE_DRAM_PERF_STATUS (0x0000061B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_DRAM_PERF_STATUS);
  @endcode
  @note MSR_IVY_BRIDGE_DRAM_PERF_STATUS is defined as MSR_DRAM_PERF_STATUS in SDM.
**/
#define MSR_IVY_BRIDGE_DRAM_PERF_STATUS          0x0000061B


/**
  Package. DRAM RAPL Parameters (R/W) See Section 14.9.5, "DRAM RAPL Domain.".

  @param  ECX  MSR_IVY_BRIDGE_DRAM_POWER_INFO (0x0000061C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_DRAM_POWER_INFO);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_DRAM_POWER_INFO, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_DRAM_POWER_INFO is defined as MSR_DRAM_POWER_INFO in SDM.
**/
#define MSR_IVY_BRIDGE_DRAM_POWER_INFO           0x0000061C


/**
  Thread. See Section 18.3.1.1.1, "Processor Event Based Sampling (PEBS).".

  @param  ECX  MSR_IVY_BRIDGE_PEBS_ENABLE (0x000003F1)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_PEBS_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_IVY_BRIDGE_PEBS_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_IVY_BRIDGE_PEBS_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_IVY_BRIDGE_PEBS_ENABLE);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_PEBS_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_IVY_BRIDGE_PEBS_ENABLE is defined as MSR_PEBS_ENABLE in SDM.
**/
#define MSR_IVY_BRIDGE_PEBS_ENABLE               0x000003F1

/**
  MSR information returned for MSR index #MSR_IVY_BRIDGE_PEBS_ENABLE
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
    UINT32  Reserved2:28;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_IVY_BRIDGE_PEBS_ENABLE_REGISTER;


/**
  Package. Uncore perfmon per-socket global control.

  @param  ECX  MSR_IVY_BRIDGE_PMON_GLOBAL_CTL (0x00000C00)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_PMON_GLOBAL_CTL);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_PMON_GLOBAL_CTL, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_PMON_GLOBAL_CTL is defined as MSR_PMON_GLOBAL_CTL in SDM.
**/
#define MSR_IVY_BRIDGE_PMON_GLOBAL_CTL           0x00000C00


/**
  Package. Uncore perfmon per-socket global status.

  @param  ECX  MSR_IVY_BRIDGE_PMON_GLOBAL_STATUS (0x00000C01)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_PMON_GLOBAL_STATUS);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_PMON_GLOBAL_STATUS, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_PMON_GLOBAL_STATUS is defined as MSR_PMON_GLOBAL_STATUS in SDM.
**/
#define MSR_IVY_BRIDGE_PMON_GLOBAL_STATUS        0x00000C01


/**
  Package. Uncore perfmon per-socket global configuration.

  @param  ECX  MSR_IVY_BRIDGE_PMON_GLOBAL_CONFIG (0x00000C06)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_PMON_GLOBAL_CONFIG);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_PMON_GLOBAL_CONFIG, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_PMON_GLOBAL_CONFIG is defined as MSR_PMON_GLOBAL_CONFIG in SDM.
**/
#define MSR_IVY_BRIDGE_PMON_GLOBAL_CONFIG        0x00000C06


/**
  Package. Uncore U-box perfmon U-box wide status.

  @param  ECX  MSR_IVY_BRIDGE_U_PMON_BOX_STATUS (0x00000C15)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_U_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_U_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_U_PMON_BOX_STATUS is defined as MSR_U_PMON_BOX_STATUS in SDM.
**/
#define MSR_IVY_BRIDGE_U_PMON_BOX_STATUS         0x00000C15


/**
  Package. Uncore PCU perfmon box wide status.

  @param  ECX  MSR_IVY_BRIDGE_PCU_PMON_BOX_STATUS (0x00000C35)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_PCU_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_PCU_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_PCU_PMON_BOX_STATUS is defined as MSR_PCU_PMON_BOX_STATUS in SDM.
**/
#define MSR_IVY_BRIDGE_PCU_PMON_BOX_STATUS       0x00000C35


/**
  Package. Uncore C-box 0 perfmon box wide filter1.

  @param  ECX  MSR_IVY_BRIDGE_C0_PMON_BOX_FILTER1 (0x00000D1A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C0_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C0_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C0_PMON_BOX_FILTER1 is defined as MSR_C0_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_IVY_BRIDGE_C0_PMON_BOX_FILTER1       0x00000D1A


/**
  Package. Uncore C-box 1 perfmon box wide filter1.

  @param  ECX  MSR_IVY_BRIDGE_C1_PMON_BOX_FILTER1 (0x00000D3A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C1_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C1_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C1_PMON_BOX_FILTER1 is defined as MSR_C1_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_IVY_BRIDGE_C1_PMON_BOX_FILTER1       0x00000D3A


/**
  Package. Uncore C-box 2 perfmon box wide filter1.

  @param  ECX  MSR_IVY_BRIDGE_C2_PMON_BOX_FILTER1 (0x00000D5A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C2_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C2_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C2_PMON_BOX_FILTER1 is defined as MSR_C2_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_IVY_BRIDGE_C2_PMON_BOX_FILTER1       0x00000D5A


/**
  Package. Uncore C-box 3 perfmon box wide filter1.

  @param  ECX  MSR_IVY_BRIDGE_C3_PMON_BOX_FILTER1 (0x00000D7A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C3_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C3_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C3_PMON_BOX_FILTER1 is defined as MSR_C3_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_IVY_BRIDGE_C3_PMON_BOX_FILTER1       0x00000D7A


/**
  Package. Uncore C-box 4 perfmon box wide filter1.

  @param  ECX  MSR_IVY_BRIDGE_C4_PMON_BOX_FILTER1 (0x00000D9A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C4_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C4_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C4_PMON_BOX_FILTER1 is defined as MSR_C4_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_IVY_BRIDGE_C4_PMON_BOX_FILTER1       0x00000D9A


/**
  Package. Uncore C-box 5 perfmon box wide filter1.

  @param  ECX  MSR_IVY_BRIDGE_C5_PMON_BOX_FILTER1 (0x00000DBA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C5_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C5_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C5_PMON_BOX_FILTER1 is defined as MSR_C5_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_IVY_BRIDGE_C5_PMON_BOX_FILTER1       0x00000DBA


/**
  Package. Uncore C-box 6 perfmon box wide filter1.

  @param  ECX  MSR_IVY_BRIDGE_C6_PMON_BOX_FILTER1 (0x00000DDA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C6_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C6_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C6_PMON_BOX_FILTER1 is defined as MSR_C6_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_IVY_BRIDGE_C6_PMON_BOX_FILTER1       0x00000DDA


/**
  Package. Uncore C-box 7 perfmon box wide filter1.

  @param  ECX  MSR_IVY_BRIDGE_C7_PMON_BOX_FILTER1 (0x00000DFA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C7_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C7_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C7_PMON_BOX_FILTER1 is defined as MSR_C7_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_IVY_BRIDGE_C7_PMON_BOX_FILTER1       0x00000DFA


/**
  Package. Uncore C-box 8 perfmon local box wide control.

  @param  ECX  MSR_IVY_BRIDGE_C8_PMON_BOX_CTL (0x00000E04)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C8_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C8_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C8_PMON_BOX_CTL is defined as MSR_C8_PMON_BOX_CTL in SDM.
**/
#define MSR_IVY_BRIDGE_C8_PMON_BOX_CTL           0x00000E04


/**
  Package. Uncore C-box 8 perfmon event select for C-box 8 counter 0.

  @param  ECX  MSR_IVY_BRIDGE_C8_PMON_EVNTSEL0 (0x00000E10)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C8_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C8_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C8_PMON_EVNTSEL0 is defined as MSR_C8_PMON_EVNTSEL0 in SDM.
**/
#define MSR_IVY_BRIDGE_C8_PMON_EVNTSEL0          0x00000E10


/**
  Package. Uncore C-box 8 perfmon event select for C-box 8 counter 1.

  @param  ECX  MSR_IVY_BRIDGE_C8_PMON_EVNTSEL1 (0x00000E11)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C8_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C8_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C8_PMON_EVNTSEL1 is defined as MSR_C8_PMON_EVNTSEL1 in SDM.
**/
#define MSR_IVY_BRIDGE_C8_PMON_EVNTSEL1          0x00000E11


/**
  Package. Uncore C-box 8 perfmon event select for C-box 8 counter 2.

  @param  ECX  MSR_IVY_BRIDGE_C8_PMON_EVNTSEL2 (0x00000E12)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C8_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C8_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C8_PMON_EVNTSEL2 is defined as MSR_C8_PMON_EVNTSEL2 in SDM.
**/
#define MSR_IVY_BRIDGE_C8_PMON_EVNTSEL2          0x00000E12


/**
  Package. Uncore C-box 8 perfmon event select for C-box 8 counter 3.

  @param  ECX  MSR_IVY_BRIDGE_C8_PMON_EVNTSEL3 (0x00000E13)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C8_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C8_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C8_PMON_EVNTSEL3 is defined as MSR_C8_PMON_EVNTSEL3 in SDM.
**/
#define MSR_IVY_BRIDGE_C8_PMON_EVNTSEL3          0x00000E13


/**
  Package. Uncore C-box 8 perfmon box wide filter.

  @param  ECX  MSR_IVY_BRIDGE_C8_PMON_BOX_FILTER (0x00000E14)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C8_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C8_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C8_PMON_BOX_FILTER is defined as MSR_C8_PMON_BOX_FILTER in SDM.
**/
#define MSR_IVY_BRIDGE_C8_PMON_BOX_FILTER        0x00000E14


/**
  Package. Uncore C-box 8 perfmon counter 0.

  @param  ECX  MSR_IVY_BRIDGE_C8_PMON_CTR0 (0x00000E16)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C8_PMON_CTR0);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C8_PMON_CTR0, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C8_PMON_CTR0 is defined as MSR_C8_PMON_CTR0 in SDM.
**/
#define MSR_IVY_BRIDGE_C8_PMON_CTR0              0x00000E16


/**
  Package. Uncore C-box 8 perfmon counter 1.

  @param  ECX  MSR_IVY_BRIDGE_C8_PMON_CTR1 (0x00000E17)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C8_PMON_CTR1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C8_PMON_CTR1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C8_PMON_CTR1 is defined as MSR_C8_PMON_CTR1 in SDM.
**/
#define MSR_IVY_BRIDGE_C8_PMON_CTR1              0x00000E17


/**
  Package. Uncore C-box 8 perfmon counter 2.

  @param  ECX  MSR_IVY_BRIDGE_C8_PMON_CTR2 (0x00000E18)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C8_PMON_CTR2);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C8_PMON_CTR2, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C8_PMON_CTR2 is defined as MSR_C8_PMON_CTR2 in SDM.
**/
#define MSR_IVY_BRIDGE_C8_PMON_CTR2              0x00000E18


/**
  Package. Uncore C-box 8 perfmon counter 3.

  @param  ECX  MSR_IVY_BRIDGE_C8_PMON_CTR3 (0x00000E19)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C8_PMON_CTR3);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C8_PMON_CTR3, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C8_PMON_CTR3 is defined as MSR_C8_PMON_CTR3 in SDM.
**/
#define MSR_IVY_BRIDGE_C8_PMON_CTR3              0x00000E19


/**
  Package. Uncore C-box 8 perfmon box wide filter1.

  @param  ECX  MSR_IVY_BRIDGE_C8_PMON_BOX_FILTER1 (0x00000E1A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C8_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C8_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C8_PMON_BOX_FILTER1 is defined as MSR_C8_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_IVY_BRIDGE_C8_PMON_BOX_FILTER1       0x00000E1A


/**
  Package. Uncore C-box 9 perfmon local box wide control.

  @param  ECX  MSR_IVY_BRIDGE_C9_PMON_BOX_CTL (0x00000E24)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C9_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C9_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C9_PMON_BOX_CTL is defined as MSR_C9_PMON_BOX_CTL in SDM.
**/
#define MSR_IVY_BRIDGE_C9_PMON_BOX_CTL           0x00000E24


/**
  Package. Uncore C-box 9 perfmon event select for C-box 9 counter 0.

  @param  ECX  MSR_IVY_BRIDGE_C9_PMON_EVNTSEL0 (0x00000E30)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C9_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C9_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C9_PMON_EVNTSEL0 is defined as MSR_C9_PMON_EVNTSEL0 in SDM.
**/
#define MSR_IVY_BRIDGE_C9_PMON_EVNTSEL0          0x00000E30


/**
  Package. Uncore C-box 9 perfmon event select for C-box 9 counter 1.

  @param  ECX  MSR_IVY_BRIDGE_C9_PMON_EVNTSEL1 (0x00000E31)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C9_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C9_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C9_PMON_EVNTSEL1 is defined as MSR_C9_PMON_EVNTSEL1 in SDM.
**/
#define MSR_IVY_BRIDGE_C9_PMON_EVNTSEL1          0x00000E31


/**
  Package. Uncore C-box 9 perfmon event select for C-box 9 counter 2.

  @param  ECX  MSR_IVY_BRIDGE_C9_PMON_EVNTSEL2 (0x00000E32)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C9_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C9_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C9_PMON_EVNTSEL2 is defined as MSR_C9_PMON_EVNTSEL2 in SDM.
**/
#define MSR_IVY_BRIDGE_C9_PMON_EVNTSEL2          0x00000E32


/**
  Package. Uncore C-box 9 perfmon event select for C-box 9 counter 3.

  @param  ECX  MSR_IVY_BRIDGE_C9_PMON_EVNTSEL3 (0x00000E33)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C9_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C9_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C9_PMON_EVNTSEL3 is defined as MSR_C9_PMON_EVNTSEL3 in SDM.
**/
#define MSR_IVY_BRIDGE_C9_PMON_EVNTSEL3          0x00000E33


/**
  Package. Uncore C-box 9 perfmon box wide filter.

  @param  ECX  MSR_IVY_BRIDGE_C9_PMON_BOX_FILTER (0x00000E34)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C9_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C9_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C9_PMON_BOX_FILTER is defined as MSR_C9_PMON_BOX_FILTER in SDM.
**/
#define MSR_IVY_BRIDGE_C9_PMON_BOX_FILTER        0x00000E34


/**
  Package. Uncore C-box 9 perfmon counter 0.

  @param  ECX  MSR_IVY_BRIDGE_C9_PMON_CTR0 (0x00000E36)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C9_PMON_CTR0);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C9_PMON_CTR0, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C9_PMON_CTR0 is defined as MSR_C9_PMON_CTR0 in SDM.
**/
#define MSR_IVY_BRIDGE_C9_PMON_CTR0              0x00000E36


/**
  Package. Uncore C-box 9 perfmon counter 1.

  @param  ECX  MSR_IVY_BRIDGE_C9_PMON_CTR1 (0x00000E37)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C9_PMON_CTR1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C9_PMON_CTR1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C9_PMON_CTR1 is defined as MSR_C9_PMON_CTR1 in SDM.
**/
#define MSR_IVY_BRIDGE_C9_PMON_CTR1              0x00000E37


/**
  Package. Uncore C-box 9 perfmon counter 2.

  @param  ECX  MSR_IVY_BRIDGE_C9_PMON_CTR2 (0x00000E38)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C9_PMON_CTR2);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C9_PMON_CTR2, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C9_PMON_CTR2 is defined as MSR_C9_PMON_CTR2 in SDM.
**/
#define MSR_IVY_BRIDGE_C9_PMON_CTR2              0x00000E38


/**
  Package. Uncore C-box 9 perfmon counter 3.

  @param  ECX  MSR_IVY_BRIDGE_C9_PMON_CTR3 (0x00000E39)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C9_PMON_CTR3);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C9_PMON_CTR3, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C9_PMON_CTR3 is defined as MSR_C9_PMON_CTR3 in SDM.
**/
#define MSR_IVY_BRIDGE_C9_PMON_CTR3              0x00000E39


/**
  Package. Uncore C-box 9 perfmon box wide filter1.

  @param  ECX  MSR_IVY_BRIDGE_C9_PMON_BOX_FILTER1 (0x00000E3A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C9_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C9_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C9_PMON_BOX_FILTER1 is defined as MSR_C9_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_IVY_BRIDGE_C9_PMON_BOX_FILTER1       0x00000E3A


/**
  Package. Uncore C-box 10 perfmon local box wide control.

  @param  ECX  MSR_IVY_BRIDGE_C10_PMON_BOX_CTL (0x00000E44)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C10_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C10_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C10_PMON_BOX_CTL is defined as MSR_C10_PMON_BOX_CTL in SDM.
**/
#define MSR_IVY_BRIDGE_C10_PMON_BOX_CTL          0x00000E44


/**
  Package. Uncore C-box 10 perfmon event select for C-box 10 counter 0.

  @param  ECX  MSR_IVY_BRIDGE_C10_PMON_EVNTSEL0 (0x00000E50)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C10_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C10_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C10_PMON_EVNTSEL0 is defined as MSR_C10_PMON_EVNTSEL0 in SDM.
**/
#define MSR_IVY_BRIDGE_C10_PMON_EVNTSEL0         0x00000E50


/**
  Package. Uncore C-box 10 perfmon event select for C-box 10 counter 1.

  @param  ECX  MSR_IVY_BRIDGE_C10_PMON_EVNTSEL1 (0x00000E51)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C10_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C10_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C10_PMON_EVNTSEL1 is defined as MSR_C10_PMON_EVNTSEL1 in SDM.
**/
#define MSR_IVY_BRIDGE_C10_PMON_EVNTSEL1         0x00000E51


/**
  Package. Uncore C-box 10 perfmon event select for C-box 10 counter 2.

  @param  ECX  MSR_IVY_BRIDGE_C10_PMON_EVNTSEL2 (0x00000E52)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C10_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C10_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C10_PMON_EVNTSEL2 is defined as MSR_C10_PMON_EVNTSEL2 in SDM.
**/
#define MSR_IVY_BRIDGE_C10_PMON_EVNTSEL2         0x00000E52


/**
  Package. Uncore C-box 10 perfmon event select for C-box 10 counter 3.

  @param  ECX  MSR_IVY_BRIDGE_C10_PMON_EVNTSEL3 (0x00000E53)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C10_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C10_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C10_PMON_EVNTSEL3 is defined as MSR_C10_PMON_EVNTSEL3 in SDM.
**/
#define MSR_IVY_BRIDGE_C10_PMON_EVNTSEL3         0x00000E53


/**
  Package. Uncore C-box 10 perfmon box wide filter.

  @param  ECX  MSR_IVY_BRIDGE_C10_PMON_BOX_FILTER (0x00000E54)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C10_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C10_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C10_PMON_BOX_FILTER is defined as MSR_C10_PMON_BOX_FILTER in SDM.
**/
#define MSR_IVY_BRIDGE_C10_PMON_BOX_FILTER       0x00000E54


/**
  Package. Uncore C-box 10 perfmon counter 0.

  @param  ECX  MSR_IVY_BRIDGE_C10_PMON_CTR0 (0x00000E56)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C10_PMON_CTR0);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C10_PMON_CTR0, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C10_PMON_CTR0 is defined as MSR_C10_PMON_CTR0 in SDM.
**/
#define MSR_IVY_BRIDGE_C10_PMON_CTR0             0x00000E56


/**
  Package. Uncore C-box 10 perfmon counter 1.

  @param  ECX  MSR_IVY_BRIDGE_C10_PMON_CTR1 (0x00000E57)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C10_PMON_CTR1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C10_PMON_CTR1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C10_PMON_CTR1 is defined as MSR_C10_PMON_CTR1 in SDM.
**/
#define MSR_IVY_BRIDGE_C10_PMON_CTR1             0x00000E57


/**
  Package. Uncore C-box 10 perfmon counter 2.

  @param  ECX  MSR_IVY_BRIDGE_C10_PMON_CTR2 (0x00000E58)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C10_PMON_CTR2);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C10_PMON_CTR2, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C10_PMON_CTR2 is defined as MSR_C10_PMON_CTR2 in SDM.
**/
#define MSR_IVY_BRIDGE_C10_PMON_CTR2             0x00000E58


/**
  Package. Uncore C-box 10 perfmon counter 3.

  @param  ECX  MSR_IVY_BRIDGE_C10_PMON_CTR3 (0x00000E59)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C10_PMON_CTR3);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C10_PMON_CTR3, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C10_PMON_CTR3 is defined as MSR_C10_PMON_CTR3 in SDM.
**/
#define MSR_IVY_BRIDGE_C10_PMON_CTR3             0x00000E59


/**
  Package. Uncore C-box 10 perfmon box wide filter1.

  @param  ECX  MSR_IVY_BRIDGE_C10_PMON_BOX_FILTER1 (0x00000E5A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C10_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C10_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C10_PMON_BOX_FILTER1 is defined as MSR_C10_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_IVY_BRIDGE_C10_PMON_BOX_FILTER1      0x00000E5A


/**
  Package. Uncore C-box 11 perfmon local box wide control.

  @param  ECX  MSR_IVY_BRIDGE_C11_PMON_BOX_CTL (0x00000E64)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C11_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C11_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C11_PMON_BOX_CTL is defined as MSR_C11_PMON_BOX_CTL in SDM.
**/
#define MSR_IVY_BRIDGE_C11_PMON_BOX_CTL          0x00000E64


/**
  Package. Uncore C-box 11 perfmon event select for C-box 11 counter 0.

  @param  ECX  MSR_IVY_BRIDGE_C11_PMON_EVNTSEL0 (0x00000E70)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C11_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C11_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C11_PMON_EVNTSEL0 is defined as MSR_C11_PMON_EVNTSEL0 in SDM.
**/
#define MSR_IVY_BRIDGE_C11_PMON_EVNTSEL0         0x00000E70


/**
  Package. Uncore C-box 11 perfmon event select for C-box 11 counter 1.

  @param  ECX  MSR_IVY_BRIDGE_C11_PMON_EVNTSEL1 (0x00000E71)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C11_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C11_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C11_PMON_EVNTSEL1 is defined as MSR_C11_PMON_EVNTSEL1 in SDM.
**/
#define MSR_IVY_BRIDGE_C11_PMON_EVNTSEL1         0x00000E71


/**
  Package. Uncore C-box 11 perfmon event select for C-box 11 counter 2.

  @param  ECX  MSR_IVY_BRIDGE_C11_PMON_EVNTSEL2 (0x00000E72)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C11_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C11_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C11_PMON_EVNTSEL2 is defined as MSR_C11_PMON_EVNTSEL2 in SDM.
**/
#define MSR_IVY_BRIDGE_C11_PMON_EVNTSEL2         0x00000E72


/**
  Package. Uncore C-box 11 perfmon event select for C-box 11 counter 3.

  @param  ECX  MSR_IVY_BRIDGE_C11_PMON_EVNTSEL3 (0x00000E73)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C11_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C11_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C11_PMON_EVNTSEL3 is defined as MSR_C11_PMON_EVNTSEL3 in SDM.
**/
#define MSR_IVY_BRIDGE_C11_PMON_EVNTSEL3         0x00000E73


/**
  Package. Uncore C-box 11 perfmon box wide filter.

  @param  ECX  MSR_IVY_BRIDGE_C11_PMON_BOX_FILTER (0x00000E74)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C11_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C11_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C11_PMON_BOX_FILTER is defined as MSR_C11_PMON_BOX_FILTER in SDM.
**/
#define MSR_IVY_BRIDGE_C11_PMON_BOX_FILTER       0x00000E74


/**
  Package. Uncore C-box 11 perfmon counter 0.

  @param  ECX  MSR_IVY_BRIDGE_C11_PMON_CTR0 (0x00000E76)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C11_PMON_CTR0);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C11_PMON_CTR0, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C11_PMON_CTR0 is defined as MSR_C11_PMON_CTR0 in SDM.
**/
#define MSR_IVY_BRIDGE_C11_PMON_CTR0             0x00000E76


/**
  Package. Uncore C-box 11 perfmon counter 1.

  @param  ECX  MSR_IVY_BRIDGE_C11_PMON_CTR1 (0x00000E77)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C11_PMON_CTR1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C11_PMON_CTR1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C11_PMON_CTR1 is defined as MSR_C11_PMON_CTR1 in SDM.
**/
#define MSR_IVY_BRIDGE_C11_PMON_CTR1             0x00000E77


/**
  Package. Uncore C-box 11 perfmon counter 2.

  @param  ECX  MSR_IVY_BRIDGE_C11_PMON_CTR2 (0x00000E78)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C11_PMON_CTR2);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C11_PMON_CTR2, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C11_PMON_CTR2 is defined as MSR_C11_PMON_CTR2 in SDM.
**/
#define MSR_IVY_BRIDGE_C11_PMON_CTR2             0x00000E78


/**
  Package. Uncore C-box 11 perfmon counter 3.

  @param  ECX  MSR_IVY_BRIDGE_C11_PMON_CTR3 (0x00000E79)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C11_PMON_CTR3);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C11_PMON_CTR3, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C11_PMON_CTR3 is defined as MSR_C11_PMON_CTR3 in SDM.
**/
#define MSR_IVY_BRIDGE_C11_PMON_CTR3             0x00000E79


/**
  Package. Uncore C-box 11 perfmon box wide filter1.

  @param  ECX  MSR_IVY_BRIDGE_C11_PMON_BOX_FILTER1 (0x00000E7A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C11_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C11_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C11_PMON_BOX_FILTER1 is defined as MSR_C11_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_IVY_BRIDGE_C11_PMON_BOX_FILTER1      0x00000E7A


/**
  Package. Uncore C-box 12 perfmon local box wide control.

  @param  ECX  MSR_IVY_BRIDGE_C12_PMON_BOX_CTL (0x00000E84)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C12_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C12_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C12_PMON_BOX_CTL is defined as MSR_C12_PMON_BOX_CTL in SDM.
**/
#define MSR_IVY_BRIDGE_C12_PMON_BOX_CTL          0x00000E84


/**
  Package. Uncore C-box 12 perfmon event select for C-box 12 counter 0.

  @param  ECX  MSR_IVY_BRIDGE_C12_PMON_EVNTSEL0 (0x00000E90)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C12_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C12_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C12_PMON_EVNTSEL0 is defined as MSR_C12_PMON_EVNTSEL0 in SDM.
**/
#define MSR_IVY_BRIDGE_C12_PMON_EVNTSEL0         0x00000E90


/**
  Package. Uncore C-box 12 perfmon event select for C-box 12 counter 1.

  @param  ECX  MSR_IVY_BRIDGE_C12_PMON_EVNTSEL1 (0x00000E91)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C12_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C12_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C12_PMON_EVNTSEL1 is defined as MSR_C12_PMON_EVNTSEL1 in SDM.
**/
#define MSR_IVY_BRIDGE_C12_PMON_EVNTSEL1         0x00000E91


/**
  Package. Uncore C-box 12 perfmon event select for C-box 12 counter 2.

  @param  ECX  MSR_IVY_BRIDGE_C12_PMON_EVNTSEL2 (0x00000E92)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C12_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C12_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C12_PMON_EVNTSEL2 is defined as MSR_C12_PMON_EVNTSEL2 in SDM.
**/
#define MSR_IVY_BRIDGE_C12_PMON_EVNTSEL2         0x00000E92


/**
  Package. Uncore C-box 12 perfmon event select for C-box 12 counter 3.

  @param  ECX  MSR_IVY_BRIDGE_C12_PMON_EVNTSEL3 (0x00000E93)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C12_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C12_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C12_PMON_EVNTSEL3 is defined as MSR_C12_PMON_EVNTSEL3 in SDM.
**/
#define MSR_IVY_BRIDGE_C12_PMON_EVNTSEL3         0x00000E93


/**
  Package. Uncore C-box 12 perfmon box wide filter.

  @param  ECX  MSR_IVY_BRIDGE_C12_PMON_BOX_FILTER (0x00000E94)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C12_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C12_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C12_PMON_BOX_FILTER is defined as MSR_C12_PMON_BOX_FILTER in SDM.
**/
#define MSR_IVY_BRIDGE_C12_PMON_BOX_FILTER       0x00000E94


/**
  Package. Uncore C-box 12 perfmon counter 0.

  @param  ECX  MSR_IVY_BRIDGE_C12_PMON_CTR0 (0x00000E96)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C12_PMON_CTR0);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C12_PMON_CTR0, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C12_PMON_CTR0 is defined as MSR_C12_PMON_CTR0 in SDM.
**/
#define MSR_IVY_BRIDGE_C12_PMON_CTR0             0x00000E96


/**
  Package. Uncore C-box 12 perfmon counter 1.

  @param  ECX  MSR_IVY_BRIDGE_C12_PMON_CTR1 (0x00000E97)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C12_PMON_CTR1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C12_PMON_CTR1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C12_PMON_CTR1 is defined as MSR_C12_PMON_CTR1 in SDM.
**/
#define MSR_IVY_BRIDGE_C12_PMON_CTR1             0x00000E97


/**
  Package. Uncore C-box 12 perfmon counter 2.

  @param  ECX  MSR_IVY_BRIDGE_C12_PMON_CTR2 (0x00000E98)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C12_PMON_CTR2);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C12_PMON_CTR2, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C12_PMON_CTR2 is defined as MSR_C12_PMON_CTR2 in SDM.
**/
#define MSR_IVY_BRIDGE_C12_PMON_CTR2             0x00000E98


/**
  Package. Uncore C-box 12 perfmon counter 3.

  @param  ECX  MSR_IVY_BRIDGE_C12_PMON_CTR3 (0x00000E99)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C12_PMON_CTR3);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C12_PMON_CTR3, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C12_PMON_CTR3 is defined as MSR_C12_PMON_CTR3 in SDM.
**/
#define MSR_IVY_BRIDGE_C12_PMON_CTR3             0x00000E99


/**
  Package. Uncore C-box 12 perfmon box wide filter1.

  @param  ECX  MSR_IVY_BRIDGE_C12_PMON_BOX_FILTER1 (0x00000E9A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C12_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C12_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C12_PMON_BOX_FILTER1 is defined as MSR_C12_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_IVY_BRIDGE_C12_PMON_BOX_FILTER1      0x00000E9A


/**
  Package. Uncore C-box 13 perfmon local box wide control.

  @param  ECX  MSR_IVY_BRIDGE_C13_PMON_BOX_CTL (0x00000EA4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C13_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C13_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C13_PMON_BOX_CTL is defined as MSR_C13_PMON_BOX_CTL in SDM.
**/
#define MSR_IVY_BRIDGE_C13_PMON_BOX_CTL          0x00000EA4


/**
  Package. Uncore C-box 13 perfmon event select for C-box 13 counter 0.

  @param  ECX  MSR_IVY_BRIDGE_C13_PMON_EVNTSEL0 (0x00000EB0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C13_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C13_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C13_PMON_EVNTSEL0 is defined as MSR_C13_PMON_EVNTSEL0 in SDM.
**/
#define MSR_IVY_BRIDGE_C13_PMON_EVNTSEL0         0x00000EB0


/**
  Package. Uncore C-box 13 perfmon event select for C-box 13 counter 1.

  @param  ECX  MSR_IVY_BRIDGE_C13_PMON_EVNTSEL1 (0x00000EB1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C13_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C13_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C13_PMON_EVNTSEL1 is defined as MSR_C13_PMON_EVNTSEL1 in SDM.
**/
#define MSR_IVY_BRIDGE_C13_PMON_EVNTSEL1         0x00000EB1


/**
  Package. Uncore C-box 13 perfmon event select for C-box 13 counter 2.

  @param  ECX  MSR_IVY_BRIDGE_C13_PMON_EVNTSEL2 (0x00000EB2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C13_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C13_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C13_PMON_EVNTSEL2 is defined as MSR_C13_PMON_EVNTSEL2 in SDM.
**/
#define MSR_IVY_BRIDGE_C13_PMON_EVNTSEL2         0x00000EB2


/**
  Package. Uncore C-box 13 perfmon event select for C-box 13 counter 3.

  @param  ECX  MSR_IVY_BRIDGE_C13_PMON_EVNTSEL3 (0x00000EB3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C13_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C13_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C13_PMON_EVNTSEL3 is defined as MSR_C13_PMON_EVNTSEL3 in SDM.
**/
#define MSR_IVY_BRIDGE_C13_PMON_EVNTSEL3         0x00000EB3


/**
  Package. Uncore C-box 13 perfmon box wide filter.

  @param  ECX  MSR_IVY_BRIDGE_C13_PMON_BOX_FILTER (0x00000EB4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C13_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C13_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C13_PMON_BOX_FILTER is defined as MSR_C13_PMON_BOX_FILTER in SDM.
**/
#define MSR_IVY_BRIDGE_C13_PMON_BOX_FILTER       0x00000EB4


/**
  Package. Uncore C-box 13 perfmon counter 0.

  @param  ECX  MSR_IVY_BRIDGE_C13_PMON_CTR0 (0x00000EB6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C13_PMON_CTR0);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C13_PMON_CTR0, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C13_PMON_CTR0 is defined as MSR_C13_PMON_CTR0 in SDM.
**/
#define MSR_IVY_BRIDGE_C13_PMON_CTR0             0x00000EB6


/**
  Package. Uncore C-box 13 perfmon counter 1.

  @param  ECX  MSR_IVY_BRIDGE_C13_PMON_CTR1 (0x00000EB7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C13_PMON_CTR1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C13_PMON_CTR1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C13_PMON_CTR1 is defined as MSR_C13_PMON_CTR1 in SDM.
**/
#define MSR_IVY_BRIDGE_C13_PMON_CTR1             0x00000EB7


/**
  Package. Uncore C-box 13 perfmon counter 2.

  @param  ECX  MSR_IVY_BRIDGE_C13_PMON_CTR2 (0x00000EB8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C13_PMON_CTR2);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C13_PMON_CTR2, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C13_PMON_CTR2 is defined as MSR_C13_PMON_CTR2 in SDM.
**/
#define MSR_IVY_BRIDGE_C13_PMON_CTR2             0x00000EB8


/**
  Package. Uncore C-box 13 perfmon counter 3.

  @param  ECX  MSR_IVY_BRIDGE_C13_PMON_CTR3 (0x00000EB9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C13_PMON_CTR3);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C13_PMON_CTR3, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C13_PMON_CTR3 is defined as MSR_C13_PMON_CTR3 in SDM.
**/
#define MSR_IVY_BRIDGE_C13_PMON_CTR3             0x00000EB9


/**
  Package. Uncore C-box 13 perfmon box wide filter1.

  @param  ECX  MSR_IVY_BRIDGE_C13_PMON_BOX_FILTER1 (0x00000EBA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C13_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C13_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C13_PMON_BOX_FILTER1 is defined as MSR_C13_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_IVY_BRIDGE_C13_PMON_BOX_FILTER1      0x00000EBA


/**
  Package. Uncore C-box 14 perfmon local box wide control.

  @param  ECX  MSR_IVY_BRIDGE_C14_PMON_BOX_CTL (0x00000EC4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C14_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C14_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C14_PMON_BOX_CTL is defined as MSR_C14_PMON_BOX_CTL in SDM.
**/
#define MSR_IVY_BRIDGE_C14_PMON_BOX_CTL          0x00000EC4


/**
  Package. Uncore C-box 14 perfmon event select for C-box 14 counter 0.

  @param  ECX  MSR_IVY_BRIDGE_C14_PMON_EVNTSEL0 (0x00000ED0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C14_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C14_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C14_PMON_EVNTSEL0 is defined as MSR_C14_PMON_EVNTSEL0 in SDM.
**/
#define MSR_IVY_BRIDGE_C14_PMON_EVNTSEL0         0x00000ED0


/**
  Package. Uncore C-box 14 perfmon event select for C-box 14 counter 1.

  @param  ECX  MSR_IVY_BRIDGE_C14_PMON_EVNTSEL1 (0x00000ED1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C14_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C14_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C14_PMON_EVNTSEL1 is defined as MSR_C14_PMON_EVNTSEL1 in SDM.
**/
#define MSR_IVY_BRIDGE_C14_PMON_EVNTSEL1         0x00000ED1


/**
  Package. Uncore C-box 14 perfmon event select for C-box 14 counter 2.

  @param  ECX  MSR_IVY_BRIDGE_C14_PMON_EVNTSEL2 (0x00000ED2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C14_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C14_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C14_PMON_EVNTSEL2 is defined as MSR_C14_PMON_EVNTSEL2 in SDM.
**/
#define MSR_IVY_BRIDGE_C14_PMON_EVNTSEL2         0x00000ED2


/**
  Package. Uncore C-box 14 perfmon event select for C-box 14 counter 3.

  @param  ECX  MSR_IVY_BRIDGE_C14_PMON_EVNTSEL3 (0x00000ED3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C14_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C14_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C14_PMON_EVNTSEL3 is defined as MSR_C14_PMON_EVNTSEL3 in SDM.
**/
#define MSR_IVY_BRIDGE_C14_PMON_EVNTSEL3         0x00000ED3


/**
  Package. Uncore C-box 14 perfmon box wide filter.

  @param  ECX  MSR_IVY_BRIDGE_C14_PMON_BOX_FILTER (0x00000ED4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C14_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C14_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C14_PMON_BOX_FILTER is defined as MSR_C14_PMON_BOX_FILTER in SDM.
**/
#define MSR_IVY_BRIDGE_C14_PMON_BOX_FILTER       0x00000ED4


/**
  Package. Uncore C-box 14 perfmon counter 0.

  @param  ECX  MSR_IVY_BRIDGE_C14_PMON_CTR0 (0x00000ED6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C14_PMON_CTR0);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C14_PMON_CTR0, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C14_PMON_CTR0 is defined as MSR_C14_PMON_CTR0 in SDM.
**/
#define MSR_IVY_BRIDGE_C14_PMON_CTR0             0x00000ED6


/**
  Package. Uncore C-box 14 perfmon counter 1.

  @param  ECX  MSR_IVY_BRIDGE_C14_PMON_CTR1 (0x00000ED7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C14_PMON_CTR1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C14_PMON_CTR1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C14_PMON_CTR1 is defined as MSR_C14_PMON_CTR1 in SDM.
**/
#define MSR_IVY_BRIDGE_C14_PMON_CTR1             0x00000ED7


/**
  Package. Uncore C-box 14 perfmon counter 2.

  @param  ECX  MSR_IVY_BRIDGE_C14_PMON_CTR2 (0x00000ED8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C14_PMON_CTR2);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C14_PMON_CTR2, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C14_PMON_CTR2 is defined as MSR_C14_PMON_CTR2 in SDM.
**/
#define MSR_IVY_BRIDGE_C14_PMON_CTR2             0x00000ED8


/**
  Package. Uncore C-box 14 perfmon counter 3.

  @param  ECX  MSR_IVY_BRIDGE_C14_PMON_CTR3 (0x00000ED9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C14_PMON_CTR3);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C14_PMON_CTR3, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C14_PMON_CTR3 is defined as MSR_C14_PMON_CTR3 in SDM.
**/
#define MSR_IVY_BRIDGE_C14_PMON_CTR3             0x00000ED9


/**
  Package. Uncore C-box 14 perfmon box wide filter1.

  @param  ECX  MSR_IVY_BRIDGE_C14_PMON_BOX_FILTER1 (0x00000EDA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_IVY_BRIDGE_C14_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_IVY_BRIDGE_C14_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_IVY_BRIDGE_C14_PMON_BOX_FILTER1 is defined as MSR_C14_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_IVY_BRIDGE_C14_PMON_BOX_FILTER1      0x00000EDA

#endif
