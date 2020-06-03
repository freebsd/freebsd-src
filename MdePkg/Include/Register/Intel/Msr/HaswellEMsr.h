/** @file
  MSR Definitions for Intel processors based on the Haswell-E microarchitecture.

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

#ifndef __HASWELL_E_MSR_H__
#define __HASWELL_E_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>

/**
  Is Intel processors based on the Haswell-E microarchitecture?

  @param   DisplayFamily  Display Family ID
  @param   DisplayModel   Display Model ID

  @retval  TRUE   Yes, it is.
  @retval  FALSE  No, it isn't.
**/
#define IS_HASWELL_E_PROCESSOR(DisplayFamily, DisplayModel) \
  (DisplayFamily == 0x06 && \
   (                        \
    DisplayModel == 0x3F    \
    )                       \
   )

/**
  Package. Configured State of Enabled Processor Core Count and Logical
  Processor Count (RO) -  After a Power-On RESET, enumerates factory
  configuration of the number of processor cores and logical processors in the
  physical package. -  Following the sequence of (i) BIOS modified a
  Configuration Mask which selects a subset of processor cores to be active
  post RESET and (ii) a RESET event after the modification, enumerates the
  current configuration of enabled processor core count and logical processor
  count in the physical package.

  @param  ECX  MSR_HASWELL_E_CORE_THREAD_COUNT (0x00000035)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_CORE_THREAD_COUNT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_CORE_THREAD_COUNT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_CORE_THREAD_COUNT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_CORE_THREAD_COUNT);
  @endcode
  @note MSR_HASWELL_E_CORE_THREAD_COUNT is defined as MSR_CORE_THREAD_COUNT in SDM.
**/
#define MSR_HASWELL_E_CORE_THREAD_COUNT          0x00000035

/**
  MSR information returned for MSR index #MSR_HASWELL_E_CORE_THREAD_COUNT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 15:0] Core_COUNT (RO) The number of processor cores that are
    /// currently enabled (by either factory configuration or BIOS
    /// configuration) in the physical package.
    ///
    UINT32  Core_Count:16;
    ///
    /// [Bits 31:16] THREAD_COUNT (RO) The number of logical processors that
    /// are currently enabled (by either factory configuration or BIOS
    /// configuration) in the physical package.
    ///
    UINT32  Thread_Count:16;
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
} MSR_HASWELL_E_CORE_THREAD_COUNT_REGISTER;


/**
  Thread. A Hardware Assigned ID for the Logical Processor (RO).

  @param  ECX  MSR_HASWELL_E_THREAD_ID_INFO (0x00000053)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_THREAD_ID_INFO_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_THREAD_ID_INFO_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_THREAD_ID_INFO_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_THREAD_ID_INFO);
  @endcode
  @note MSR_HASWELL_E_THREAD_ID_INFO is defined as MSR_THREAD_ID_INFO in SDM.
**/
#define MSR_HASWELL_E_THREAD_ID_INFO             0x00000053

/**
  MSR information returned for MSR index #MSR_HASWELL_E_THREAD_ID_INFO
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] Logical_Processor_ID (RO) An implementation-specific
    /// numerical. value physically assigned to each logical processor. This
    /// ID is not related to Initial APIC ID or x2APIC ID, it is unique within
    /// a physical package.
    ///
    UINT32  Logical_Processor_ID:8;
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
} MSR_HASWELL_E_THREAD_ID_INFO_REGISTER;


/**
  Core. C-State Configuration Control (R/W) Note: C-state values are processor
  specific C-state code names, unrelated to MWAIT extension C-state parameters
  or ACPI C-states. `See http://biosbits.org. <http://biosbits.org>`__.

  @param  ECX  MSR_HASWELL_E_PKG_CST_CONFIG_CONTROL (0x000000E2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_PKG_CST_CONFIG_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_PKG_CST_CONFIG_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_PKG_CST_CONFIG_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_PKG_CST_CONFIG_CONTROL);
  AsmWriteMsr64 (MSR_HASWELL_E_PKG_CST_CONFIG_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_E_PKG_CST_CONFIG_CONTROL is defined as MSR_PKG_CST_CONFIG_CONTROL in SDM.
**/
#define MSR_HASWELL_E_PKG_CST_CONFIG_CONTROL     0x000000E2

/**
  MSR information returned for MSR index #MSR_HASWELL_E_PKG_CST_CONFIG_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 2:0] Package C-State Limit (R/W) Specifies the lowest
    /// processor-specific C-state code name (consuming the least power) for
    /// the package. The default is set as factory-configured package C-state
    /// limit. The following C-state code name encodings are supported: 000b:
    /// C0/C1 (no package C-state support) 001b: C2 010b: C6 (non-retention)
    /// 011b: C6 (retention) 111b: No Package C state limits. All C states
    /// supported by the processor are available.
    ///
    UINT32  Limit:3;
    UINT32  Reserved1:7;
    ///
    /// [Bit 10] I/O MWAIT Redirection Enable (R/W).
    ///
    UINT32  IO_MWAIT:1;
    UINT32  Reserved2:4;
    ///
    /// [Bit 15] CFG Lock (R/WO).
    ///
    UINT32  CFGLock:1;
    UINT32  Reserved3:9;
    ///
    /// [Bit 25] C3 State Auto Demotion Enable (R/W).
    ///
    UINT32  C3AutoDemotion:1;
    ///
    /// [Bit 26] C1 State Auto Demotion Enable (R/W).
    ///
    UINT32  C1AutoDemotion:1;
    ///
    /// [Bit 27] Enable C3 Undemotion (R/W).
    ///
    UINT32  C3Undemotion:1;
    ///
    /// [Bit 28] Enable C1 Undemotion (R/W).
    ///
    UINT32  C1Undemotion:1;
    ///
    /// [Bit 29] Package C State Demotion Enable (R/W).
    ///
    UINT32  CStateDemotion:1;
    ///
    /// [Bit 30] Package C State UnDemotion Enable (R/W).
    ///
    UINT32  CStateUndemotion:1;
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
} MSR_HASWELL_E_PKG_CST_CONFIG_CONTROL_REGISTER;


/**
  Thread. Global Machine Check Capability (R/O).

  @param  ECX  MSR_HASWELL_E_IA32_MCG_CAP (0x00000179)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_IA32_MCG_CAP_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_IA32_MCG_CAP_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_IA32_MCG_CAP_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_IA32_MCG_CAP);
  @endcode
  @note MSR_HASWELL_E_IA32_MCG_CAP is defined as IA32_MCG_CAP in SDM.
**/
#define MSR_HASWELL_E_IA32_MCG_CAP               0x00000179

/**
  MSR information returned for MSR index #MSR_HASWELL_E_IA32_MCG_CAP
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] Count.
    ///
    UINT32  Count:8;
    ///
    /// [Bit 8] MCG_CTL_P.
    ///
    UINT32  MCG_CTL_P:1;
    ///
    /// [Bit 9] MCG_EXT_P.
    ///
    UINT32  MCG_EXT_P:1;
    ///
    /// [Bit 10] MCP_CMCI_P.
    ///
    UINT32  MCP_CMCI_P:1;
    ///
    /// [Bit 11] MCG_TES_P.
    ///
    UINT32  MCG_TES_P:1;
    UINT32  Reserved1:4;
    ///
    /// [Bits 23:16] MCG_EXT_CNT.
    ///
    UINT32  MCG_EXT_CNT:8;
    ///
    /// [Bit 24] MCG_SER_P.
    ///
    UINT32  MCG_SER_P:1;
    ///
    /// [Bit 25] MCG_EM_P.
    ///
    UINT32  MCG_EM_P:1;
    ///
    /// [Bit 26] MCG_ELOG_P.
    ///
    UINT32  MCG_ELOG_P:1;
    UINT32  Reserved2:5;
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
} MSR_HASWELL_E_IA32_MCG_CAP_REGISTER;


/**
  THREAD. Enhanced SMM Capabilities (SMM-RO) Reports SMM capability
  Enhancement. Accessible only while in SMM.

  @param  ECX  MSR_HASWELL_E_SMM_MCA_CAP (0x0000017D)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_SMM_MCA_CAP_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_SMM_MCA_CAP_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_SMM_MCA_CAP_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_SMM_MCA_CAP);
  AsmWriteMsr64 (MSR_HASWELL_E_SMM_MCA_CAP, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_E_SMM_MCA_CAP is defined as MSR_SMM_MCA_CAP in SDM.
**/
#define MSR_HASWELL_E_SMM_MCA_CAP                0x0000017D

/**
  MSR information returned for MSR index #MSR_HASWELL_E_SMM_MCA_CAP
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
    /// SMM code access restriction is supported and a host-space interface
    /// available to SMM handler.
    ///
    UINT32  SMM_Code_Access_Chk:1;
    ///
    /// [Bit 59] Long_Flow_Indication (SMM-RO) If set to 1 indicates that the
    /// SMM long flow indicator is supported and a host-space interface
    /// available to SMM handler.
    ///
    UINT32  Long_Flow_Indication:1;
    UINT32  Reserved3:4;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_E_SMM_MCA_CAP_REGISTER;


/**
  Package. MC Bank Error Configuration (R/W).

  @param  ECX  MSR_HASWELL_E_ERROR_CONTROL (0x0000017F)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_ERROR_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_ERROR_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_ERROR_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_ERROR_CONTROL);
  AsmWriteMsr64 (MSR_HASWELL_E_ERROR_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_E_ERROR_CONTROL is defined as MSR_ERROR_CONTROL in SDM.
**/
#define MSR_HASWELL_E_ERROR_CONTROL              0x0000017F

/**
  MSR information returned for MSR index #MSR_HASWELL_E_ERROR_CONTROL
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
} MSR_HASWELL_E_ERROR_CONTROL_REGISTER;


/**
  Package. Maximum Ratio Limit of Turbo Mode RO if MSR_PLATFORM_INFO.[28] = 0,
  RW if MSR_PLATFORM_INFO.[28] = 1.

  @param  ECX  MSR_HASWELL_E_TURBO_RATIO_LIMIT (0x000001AD)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_TURBO_RATIO_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_TURBO_RATIO_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_TURBO_RATIO_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_TURBO_RATIO_LIMIT);
  @endcode
  @note MSR_HASWELL_E_TURBO_RATIO_LIMIT is defined as MSR_TURBO_RATIO_LIMIT in SDM.
**/
#define MSR_HASWELL_E_TURBO_RATIO_LIMIT          0x000001AD

/**
  MSR information returned for MSR index #MSR_HASWELL_E_TURBO_RATIO_LIMIT
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
} MSR_HASWELL_E_TURBO_RATIO_LIMIT_REGISTER;


/**
  Package. Maximum Ratio Limit of Turbo Mode RO if MSR_PLATFORM_INFO.[28] = 0,
  RW if MSR_PLATFORM_INFO.[28] = 1.

  @param  ECX  MSR_HASWELL_E_TURBO_RATIO_LIMIT1 (0x000001AE)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_TURBO_RATIO_LIMIT1_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_TURBO_RATIO_LIMIT1_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_TURBO_RATIO_LIMIT1_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_TURBO_RATIO_LIMIT1);
  @endcode
  @note MSR_HASWELL_E_TURBO_RATIO_LIMIT1 is defined as MSR_TURBO_RATIO_LIMIT1 in SDM.
**/
#define MSR_HASWELL_E_TURBO_RATIO_LIMIT1         0x000001AE

/**
  MSR information returned for MSR index #MSR_HASWELL_E_TURBO_RATIO_LIMIT1
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
    /// limit of 10 core active.
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
    ///
    /// [Bits 63:56] Package. Maximum Ratio Limit for16C Maximum turbo ratio
    /// limit of 16 core active.
    ///
    UINT32  Maximum16C:8;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_E_TURBO_RATIO_LIMIT1_REGISTER;


/**
  Package. Maximum Ratio Limit of Turbo Mode RO if MSR_PLATFORM_INFO.[28] = 0,
  RW if MSR_PLATFORM_INFO.[28] = 1.

  @param  ECX  MSR_HASWELL_E_TURBO_RATIO_LIMIT2 (0x000001AF)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_TURBO_RATIO_LIMIT2_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_TURBO_RATIO_LIMIT2_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_TURBO_RATIO_LIMIT2_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_TURBO_RATIO_LIMIT2);
  @endcode
  @note MSR_HASWELL_E_TURBO_RATIO_LIMIT2 is defined as MSR_TURBO_RATIO_LIMIT2 in SDM.
**/
#define MSR_HASWELL_E_TURBO_RATIO_LIMIT2         0x000001AF

/**
  MSR information returned for MSR index #MSR_HASWELL_E_TURBO_RATIO_LIMIT2
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] Package. Maximum Ratio Limit for 17C Maximum turbo ratio
    /// limit of 17 core active.
    ///
    UINT32  Maximum17C:8;
    ///
    /// [Bits 15:8] Package. Maximum Ratio Limit for 18C Maximum turbo ratio
    /// limit of 18 core active.
    ///
    UINT32  Maximum18C:8;
    UINT32  Reserved1:16;
    UINT32  Reserved2:31;
    ///
    /// [Bit 63] Package. Semaphore for Turbo Ratio Limit Configuration If 1,
    /// the processor uses override configuration specified in
    /// MSR_TURBO_RATIO_LIMIT, MSR_TURBO_RATIO_LIMIT1 and
    /// MSR_TURBO_RATIO_LIMIT2. If 0, the processor uses factory-set
    /// configuration (Default).
    ///
    UINT32  TurboRatioLimitConfigurationSemaphore:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_E_TURBO_RATIO_LIMIT2_REGISTER;


/**
  Package. Unit Multipliers used in RAPL Interfaces (R/O).

  @param  ECX  MSR_HASWELL_E_RAPL_POWER_UNIT (0x00000606)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_RAPL_POWER_UNIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_RAPL_POWER_UNIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_RAPL_POWER_UNIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_RAPL_POWER_UNIT);
  @endcode
  @note MSR_HASWELL_E_RAPL_POWER_UNIT is defined as MSR_RAPL_POWER_UNIT in SDM.
**/
#define MSR_HASWELL_E_RAPL_POWER_UNIT            0x00000606

/**
  MSR information returned for MSR index #MSR_HASWELL_E_RAPL_POWER_UNIT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 3:0] Package. Power Units See Section 14.9.1, "RAPL Interfaces.".
    ///
    UINT32  PowerUnits:4;
    UINT32  Reserved1:4;
    ///
    /// [Bits 12:8] Package. Energy Status Units Energy related information
    /// (in Joules) is based on the multiplier, 1/2^ESU; where ESU is an
    /// unsigned integer represented by bits 12:8. Default value is 0EH (or 61
    /// micro-joules).
    ///
    UINT32  EnergyStatusUnits:5;
    UINT32  Reserved2:3;
    ///
    /// [Bits 19:16] Package. Time Units See Section 14.9.1, "RAPL
    /// Interfaces.".
    ///
    UINT32  TimeUnits:4;
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
} MSR_HASWELL_E_RAPL_POWER_UNIT_REGISTER;


/**
  Package. DRAM RAPL Power Limit Control (R/W)  See Section 14.9.5, "DRAM RAPL
  Domain.".

  @param  ECX  MSR_HASWELL_E_DRAM_POWER_LIMIT (0x00000618)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_DRAM_POWER_LIMIT);
  AsmWriteMsr64 (MSR_HASWELL_E_DRAM_POWER_LIMIT, Msr);
  @endcode
  @note MSR_HASWELL_E_DRAM_POWER_LIMIT is defined as MSR_DRAM_POWER_LIMIT in SDM.
**/
#define MSR_HASWELL_E_DRAM_POWER_LIMIT           0x00000618


/**
  Package. DRAM Energy Status (R/O)  Energy Consumed by DRAM devices.

  @param  ECX  MSR_HASWELL_E_DRAM_ENERGY_STATUS (0x00000619)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_DRAM_ENERGY_STATUS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_DRAM_ENERGY_STATUS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_DRAM_ENERGY_STATUS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_DRAM_ENERGY_STATUS);
  @endcode
  @note MSR_HASWELL_E_DRAM_ENERGY_STATUS is defined as MSR_DRAM_ENERGY_STATUS in SDM.
**/
#define MSR_HASWELL_E_DRAM_ENERGY_STATUS         0x00000619

/**
  MSR information returned for MSR index #MSR_HASWELL_E_DRAM_ENERGY_STATUS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 31:0] Energy in 15.3 micro-joules. Requires BIOS configuration
    /// to enable DRAM RAPL mode 0 (Direct VR).
    ///
    UINT32  Energy:32;
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
} MSR_HASWELL_E_DRAM_ENERGY_STATUS_REGISTER;


/**
  Package. DRAM Performance Throttling Status (R/O) See Section 14.9.5, "DRAM
  RAPL Domain.".

  @param  ECX  MSR_HASWELL_E_DRAM_PERF_STATUS (0x0000061B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_DRAM_PERF_STATUS);
  @endcode
  @note MSR_HASWELL_E_DRAM_PERF_STATUS is defined as MSR_DRAM_PERF_STATUS in SDM.
**/
#define MSR_HASWELL_E_DRAM_PERF_STATUS           0x0000061B


/**
  Package. DRAM RAPL Parameters (R/W) See Section 14.9.5, "DRAM RAPL Domain.".

  @param  ECX  MSR_HASWELL_E_DRAM_POWER_INFO (0x0000061C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_DRAM_POWER_INFO);
  AsmWriteMsr64 (MSR_HASWELL_E_DRAM_POWER_INFO, Msr);
  @endcode
  @note MSR_HASWELL_E_DRAM_POWER_INFO is defined as MSR_DRAM_POWER_INFO in SDM.
**/
#define MSR_HASWELL_E_DRAM_POWER_INFO            0x0000061C


/**
  Package. Configuration of PCIE PLL Relative to BCLK(R/W).

  @param  ECX  MSR_HASWELL_E_PCIE_PLL_RATIO (0x0000061E)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_PCIE_PLL_RATIO_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_PCIE_PLL_RATIO_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_PCIE_PLL_RATIO_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_PCIE_PLL_RATIO);
  AsmWriteMsr64 (MSR_HASWELL_E_PCIE_PLL_RATIO, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_E_PCIE_PLL_RATIO is defined as MSR_PCIE_PLL_RATIO in SDM.
**/
#define MSR_HASWELL_E_PCIE_PLL_RATIO             0x0000061E

/**
  MSR information returned for MSR index #MSR_HASWELL_E_PCIE_PLL_RATIO
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 1:0] Package. PCIE Ratio (R/W) 00b: Use 5:5 mapping for100MHz
    /// operation (default) 01b: Use 5:4 mapping for125MHz operation 10b: Use
    /// 5:3 mapping for166MHz operation 11b: Use 5:2 mapping for250MHz
    /// operation.
    ///
    UINT32  PCIERatio:2;
    ///
    /// [Bit 2] Package. LPLL Select (R/W) if 1, use configured setting of
    /// PCIE Ratio.
    ///
    UINT32  LPLLSelect:1;
    ///
    /// [Bit 3] Package. LONG RESET (R/W) if 1, wait additional time-out
    /// before re-locking Gen2/Gen3 PLLs.
    ///
    UINT32  LONGRESET:1;
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
} MSR_HASWELL_E_PCIE_PLL_RATIO_REGISTER;


/**
  Package. Uncore Ratio Limit (R/W) Out of reset, the min_ratio and max_ratio
  fields represent the widest possible range of uncore frequencies. Writing to
  these fields allows software to control the minimum and the maximum
  frequency that hardware will select.

  @param  ECX  MSR_HASWELL_E_MSRUNCORE_RATIO_LIMIT (0x00000620)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_MSRUNCORE_RATIO_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_MSRUNCORE_RATIO_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_MSRUNCORE_RATIO_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_MSRUNCORE_RATIO_LIMIT);
  AsmWriteMsr64 (MSR_HASWELL_E_MSRUNCORE_RATIO_LIMIT, Msr.Uint64);
  @endcode
**/
#define MSR_HASWELL_E_MSRUNCORE_RATIO_LIMIT      0x00000620

/**
  MSR information returned for MSR index #MSR_HASWELL_E_MSRUNCORE_RATIO_LIMIT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 6:0] MAX_RATIO This field is used to limit the max ratio of the
    /// LLC/Ring.
    ///
    UINT32  MAX_RATIO:7;
    UINT32  Reserved1:1;
    ///
    /// [Bits 14:8] MIN_RATIO Writing to this field controls the minimum
    /// possible ratio of the LLC/Ring.
    ///
    UINT32  MIN_RATIO:7;
    UINT32  Reserved2:17;
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
} MSR_HASWELL_E_MSRUNCORE_RATIO_LIMIT_REGISTER;

/**
  Package. Reserved (R/O) Reads return 0.

  @param  ECX  MSR_HASWELL_E_PP0_ENERGY_STATUS (0x00000639)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_PP0_ENERGY_STATUS);
  @endcode
  @note MSR_HASWELL_E_PP0_ENERGY_STATUS is defined as MSR_PP0_ENERGY_STATUS in SDM.
**/
#define MSR_HASWELL_E_PP0_ENERGY_STATUS          0x00000639


/**
  Package. Indicator of Frequency Clipping in Processor Cores (R/W) (frequency
  refers to processor core frequency).

  @param  ECX  MSR_HASWELL_E_CORE_PERF_LIMIT_REASONS (0x00000690)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_CORE_PERF_LIMIT_REASONS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_CORE_PERF_LIMIT_REASONS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_CORE_PERF_LIMIT_REASONS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_CORE_PERF_LIMIT_REASONS);
  AsmWriteMsr64 (MSR_HASWELL_E_CORE_PERF_LIMIT_REASONS, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_E_CORE_PERF_LIMIT_REASONS is defined as MSR_CORE_PERF_LIMIT_REASONS in SDM.
**/
#define MSR_HASWELL_E_CORE_PERF_LIMIT_REASONS    0x00000690

/**
  MSR information returned for MSR index #MSR_HASWELL_E_CORE_PERF_LIMIT_REASONS
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
    UINT32  PROCHOT_Status:1;
    ///
    /// [Bit 1] Thermal Status (R0) When set, frequency is reduced below the
    /// operating system request due to a thermal event.
    ///
    UINT32  ThermalStatus:1;
    ///
    /// [Bit 2] Power Budget Management Status (R0) When set, frequency is
    /// reduced below the operating system request due to PBM limit.
    ///
    UINT32  PowerBudgetManagementStatus:1;
    ///
    /// [Bit 3] Platform Configuration Services Status (R0) When set,
    /// frequency is reduced below the operating system request due to PCS
    /// limit.
    ///
    UINT32  PlatformConfigurationServicesStatus:1;
    UINT32  Reserved1:1;
    ///
    /// [Bit 5] Autonomous Utilization-Based Frequency Control Status (R0)
    /// When set, frequency is reduced below the operating system request
    /// because the processor has detected that utilization is low.
    ///
    UINT32  AutonomousUtilizationBasedFrequencyControlStatus:1;
    ///
    /// [Bit 6] VR Therm Alert Status (R0) When set, frequency is reduced
    /// below the operating system request due to a thermal alert from the
    /// Voltage Regulator.
    ///
    UINT32  VRThermAlertStatus:1;
    UINT32  Reserved2:1;
    ///
    /// [Bit 8] Electrical Design Point Status (R0) When set, frequency is
    /// reduced below the operating system request due to electrical design
    /// point constraints (e.g. maximum electrical current consumption).
    ///
    UINT32  ElectricalDesignPointStatus:1;
    UINT32  Reserved3:1;
    ///
    /// [Bit 10] Multi-Core Turbo Status (R0) When set, frequency is reduced
    /// below the operating system request due to Multi-Core Turbo limits.
    ///
    UINT32  MultiCoreTurboStatus:1;
    UINT32  Reserved4:2;
    ///
    /// [Bit 13] Core Frequency P1 Status (R0) When set, frequency is reduced
    /// below max non-turbo P1.
    ///
    UINT32  FrequencyP1Status:1;
    ///
    /// [Bit 14] Core Max n-core Turbo Frequency Limiting Status (R0) When
    /// set, frequency is reduced below max n-core turbo frequency.
    ///
    UINT32  TurboFrequencyLimitingStatus:1;
    ///
    /// [Bit 15] Core Frequency Limiting Status (R0) When set, frequency is
    /// reduced below the operating system request.
    ///
    UINT32  FrequencyLimitingStatus:1;
    ///
    /// [Bit 16] PROCHOT Log  When set, indicates that the PROCHOT Status bit
    /// has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  PROCHOT_Log:1;
    ///
    /// [Bit 17] Thermal Log  When set, indicates that the Thermal Status bit
    /// has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  ThermalLog:1;
    ///
    /// [Bit 18] Power Budget Management Log  When set, indicates that the PBM
    /// Status bit has asserted since the log bit was last cleared. This log
    /// bit will remain set until cleared by software writing 0.
    ///
    UINT32  PowerBudgetManagementLog:1;
    ///
    /// [Bit 19] Platform Configuration Services Log  When set, indicates that
    /// the PCS Status bit has asserted since the log bit was last cleared.
    /// This log bit will remain set until cleared by software writing 0.
    ///
    UINT32  PlatformConfigurationServicesLog:1;
    UINT32  Reserved5:1;
    ///
    /// [Bit 21] Autonomous Utilization-Based Frequency Control Log  When set,
    /// indicates that the AUBFC Status bit has asserted since the log bit was
    /// last cleared. This log bit will remain set until cleared by software
    /// writing 0.
    ///
    UINT32  AutonomousUtilizationBasedFrequencyControlLog:1;
    ///
    /// [Bit 22] VR Therm Alert Log  When set, indicates that the VR Therm
    /// Alert Status bit has asserted since the log bit was last cleared. This
    /// log bit will remain set until cleared by software writing 0.
    ///
    UINT32  VRThermAlertLog:1;
    UINT32  Reserved6:1;
    ///
    /// [Bit 24] Electrical Design Point Log  When set, indicates that the EDP
    /// Status bit has asserted since the log bit was last cleared. This log
    /// bit will remain set until cleared by software writing 0.
    ///
    UINT32  ElectricalDesignPointLog:1;
    UINT32  Reserved7:1;
    ///
    /// [Bit 26] Multi-Core Turbo Log  When set, indicates that the Multi-Core
    /// Turbo Status bit has asserted since the log bit was last cleared. This
    /// log bit will remain set until cleared by software writing 0.
    ///
    UINT32  MultiCoreTurboLog:1;
    UINT32  Reserved8:2;
    ///
    /// [Bit 29] Core Frequency P1 Log When set, indicates that the Core
    /// Frequency P1 Status bit has asserted since the log bit was last
    /// cleared. This log bit will remain set until cleared by software
    /// writing 0.
    ///
    UINT32  CoreFrequencyP1Log:1;
    ///
    /// [Bit 30] Core Max n-core Turbo Frequency Limiting Log When set,
    /// indicates that the Core Max n-core Turbo Frequency Limiting Status bit
    /// has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  TurboFrequencyLimitingLog:1;
    ///
    /// [Bit 31] Core Frequency Limiting Log When set, indicates that the Core
    /// Frequency Limiting Status bit has asserted since the log bit was last
    /// cleared. This log bit will remain set until cleared by software
    /// writing 0.
    ///
    UINT32  CoreFrequencyLimitingLog:1;
    UINT32  Reserved9:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_E_CORE_PERF_LIMIT_REASONS_REGISTER;


/**
  THREAD. Monitoring Event Select Register (R/W). if CPUID.(EAX=07H,
  ECX=0):EBX.RDT-M[bit 12] = 1.

  @param  ECX  MSR_HASWELL_E_IA32_QM_EVTSEL (0x00000C8D)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_IA32_QM_EVTSEL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_IA32_QM_EVTSEL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_IA32_QM_EVTSEL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_IA32_QM_EVTSEL);
  AsmWriteMsr64 (MSR_HASWELL_E_IA32_QM_EVTSEL, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_E_IA32_QM_EVTSEL is defined as IA32_QM_EVTSEL in SDM.
**/
#define MSR_HASWELL_E_IA32_QM_EVTSEL             0x00000C8D

/**
  MSR information returned for MSR index #MSR_HASWELL_E_IA32_QM_EVTSEL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] EventID (RW) Event encoding: 0x0: no monitoring 0x1: L3
    /// occupancy monitoring all other encoding reserved..
    ///
    UINT32  EventID:8;
    UINT32  Reserved1:24;
    ///
    /// [Bits 41:32] RMID (RW).
    ///
    UINT32  RMID:10;
    UINT32  Reserved2:22;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_E_IA32_QM_EVTSEL_REGISTER;


/**
  THREAD. Resource Association Register (R/W)..

  @param  ECX  MSR_HASWELL_E_IA32_PQR_ASSOC (0x00000C8F)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_IA32_PQR_ASSOC_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_E_IA32_PQR_ASSOC_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_E_IA32_PQR_ASSOC_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_E_IA32_PQR_ASSOC);
  AsmWriteMsr64 (MSR_HASWELL_E_IA32_PQR_ASSOC, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_E_IA32_PQR_ASSOC is defined as IA32_PQR_ASSOC in SDM.
**/
#define MSR_HASWELL_E_IA32_PQR_ASSOC             0x00000C8F

/**
  MSR information returned for MSR index #MSR_HASWELL_E_IA32_PQR_ASSOC
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 9:0] RMID.
    ///
    UINT32  RMID:10;
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
} MSR_HASWELL_E_IA32_PQR_ASSOC_REGISTER;


/**
  Package. Uncore perfmon per-socket global control.

  @param  ECX  MSR_HASWELL_E_PMON_GLOBAL_CTL (0x00000700)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_PMON_GLOBAL_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_PMON_GLOBAL_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_PMON_GLOBAL_CTL is defined as MSR_PMON_GLOBAL_CTL in SDM.
**/
#define MSR_HASWELL_E_PMON_GLOBAL_CTL            0x00000700


/**
  Package. Uncore perfmon per-socket global status.

  @param  ECX  MSR_HASWELL_E_PMON_GLOBAL_STATUS (0x00000701)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_PMON_GLOBAL_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_PMON_GLOBAL_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_PMON_GLOBAL_STATUS is defined as MSR_PMON_GLOBAL_STATUS in SDM.
**/
#define MSR_HASWELL_E_PMON_GLOBAL_STATUS         0x00000701


/**
  Package. Uncore perfmon per-socket global configuration.

  @param  ECX  MSR_HASWELL_E_PMON_GLOBAL_CONFIG (0x00000702)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_PMON_GLOBAL_CONFIG);
  AsmWriteMsr64 (MSR_HASWELL_E_PMON_GLOBAL_CONFIG, Msr);
  @endcode
  @note MSR_HASWELL_E_PMON_GLOBAL_CONFIG is defined as MSR_PMON_GLOBAL_CONFIG in SDM.
**/
#define MSR_HASWELL_E_PMON_GLOBAL_CONFIG         0x00000702


/**
  Package. Uncore U-box UCLK fixed counter control.

  @param  ECX  MSR_HASWELL_E_U_PMON_UCLK_FIXED_CTL (0x00000703)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_U_PMON_UCLK_FIXED_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_U_PMON_UCLK_FIXED_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_U_PMON_UCLK_FIXED_CTL is defined as MSR_U_PMON_UCLK_FIXED_CTL in SDM.
**/
#define MSR_HASWELL_E_U_PMON_UCLK_FIXED_CTL      0x00000703


/**
  Package. Uncore U-box UCLK fixed counter.

  @param  ECX  MSR_HASWELL_E_U_PMON_UCLK_FIXED_CTR (0x00000704)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_U_PMON_UCLK_FIXED_CTR);
  AsmWriteMsr64 (MSR_HASWELL_E_U_PMON_UCLK_FIXED_CTR, Msr);
  @endcode
  @note MSR_HASWELL_E_U_PMON_UCLK_FIXED_CTR is defined as MSR_U_PMON_UCLK_FIXED_CTR in SDM.
**/
#define MSR_HASWELL_E_U_PMON_UCLK_FIXED_CTR      0x00000704


/**
  Package. Uncore U-box perfmon event select for U-box counter 0.

  @param  ECX  MSR_HASWELL_E_U_PMON_EVNTSEL0 (0x00000705)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_U_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_U_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_U_PMON_EVNTSEL0 is defined as MSR_U_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_U_PMON_EVNTSEL0            0x00000705


/**
  Package. Uncore U-box perfmon event select for U-box counter 1.

  @param  ECX  MSR_HASWELL_E_U_PMON_EVNTSEL1 (0x00000706)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_U_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_U_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_U_PMON_EVNTSEL1 is defined as MSR_U_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_U_PMON_EVNTSEL1            0x00000706


/**
  Package. Uncore U-box perfmon U-box wide status.

  @param  ECX  MSR_HASWELL_E_U_PMON_BOX_STATUS (0x00000708)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_U_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_U_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_U_PMON_BOX_STATUS is defined as MSR_U_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_U_PMON_BOX_STATUS          0x00000708


/**
  Package. Uncore U-box perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_U_PMON_CTR0 (0x00000709)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_U_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_U_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_U_PMON_CTR0 is defined as MSR_U_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_U_PMON_CTR0                0x00000709


/**
  Package. Uncore U-box perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_U_PMON_CTR1 (0x0000070A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_U_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_U_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_U_PMON_CTR1 is defined as MSR_U_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_U_PMON_CTR1                0x0000070A


/**
  Package. Uncore PCU perfmon for PCU-box-wide control.

  @param  ECX  MSR_HASWELL_E_PCU_PMON_BOX_CTL (0x00000710)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_PCU_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_PCU_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_PCU_PMON_BOX_CTL is defined as MSR_PCU_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_PCU_PMON_BOX_CTL           0x00000710


/**
  Package. Uncore PCU perfmon event select for PCU counter 0.

  @param  ECX  MSR_HASWELL_E_PCU_PMON_EVNTSEL0 (0x00000711)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_PCU_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_PCU_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_PCU_PMON_EVNTSEL0 is defined as MSR_PCU_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_PCU_PMON_EVNTSEL0          0x00000711


/**
  Package. Uncore PCU perfmon event select for PCU counter 1.

  @param  ECX  MSR_HASWELL_E_PCU_PMON_EVNTSEL1 (0x00000712)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_PCU_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_PCU_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_PCU_PMON_EVNTSEL1 is defined as MSR_PCU_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_PCU_PMON_EVNTSEL1          0x00000712


/**
  Package. Uncore PCU perfmon event select for PCU counter 2.

  @param  ECX  MSR_HASWELL_E_PCU_PMON_EVNTSEL2 (0x00000713)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_PCU_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_PCU_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_PCU_PMON_EVNTSEL2 is defined as MSR_PCU_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_PCU_PMON_EVNTSEL2          0x00000713


/**
  Package. Uncore PCU perfmon event select for PCU counter 3.

  @param  ECX  MSR_HASWELL_E_PCU_PMON_EVNTSEL3 (0x00000714)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_PCU_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_PCU_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_PCU_PMON_EVNTSEL3 is defined as MSR_PCU_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_PCU_PMON_EVNTSEL3          0x00000714


/**
  Package. Uncore PCU perfmon box-wide filter.

  @param  ECX  MSR_HASWELL_E_PCU_PMON_BOX_FILTER (0x00000715)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_PCU_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_HASWELL_E_PCU_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_HASWELL_E_PCU_PMON_BOX_FILTER is defined as MSR_PCU_PMON_BOX_FILTER in SDM.
**/
#define MSR_HASWELL_E_PCU_PMON_BOX_FILTER        0x00000715


/**
  Package. Uncore PCU perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_PCU_PMON_BOX_STATUS (0x00000716)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_PCU_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_PCU_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_PCU_PMON_BOX_STATUS is defined as MSR_PCU_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_PCU_PMON_BOX_STATUS        0x00000716


/**
  Package. Uncore PCU perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_PCU_PMON_CTR0 (0x00000717)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_PCU_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_PCU_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_PCU_PMON_CTR0 is defined as MSR_PCU_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_PCU_PMON_CTR0              0x00000717


/**
  Package. Uncore PCU perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_PCU_PMON_CTR1 (0x00000718)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_PCU_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_PCU_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_PCU_PMON_CTR1 is defined as MSR_PCU_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_PCU_PMON_CTR1              0x00000718


/**
  Package. Uncore PCU perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_PCU_PMON_CTR2 (0x00000719)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_PCU_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_PCU_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_PCU_PMON_CTR2 is defined as MSR_PCU_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_PCU_PMON_CTR2              0x00000719


/**
  Package. Uncore PCU perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_PCU_PMON_CTR3 (0x0000071A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_PCU_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_PCU_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_PCU_PMON_CTR3 is defined as MSR_PCU_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_PCU_PMON_CTR3              0x0000071A


/**
  Package. Uncore SBo 0 perfmon for SBo 0 box-wide control.

  @param  ECX  MSR_HASWELL_E_S0_PMON_BOX_CTL (0x00000720)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S0_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_S0_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_S0_PMON_BOX_CTL is defined as MSR_S0_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_S0_PMON_BOX_CTL            0x00000720


/**
  Package. Uncore SBo 0 perfmon event select for SBo 0 counter 0.

  @param  ECX  MSR_HASWELL_E_S0_PMON_EVNTSEL0 (0x00000721)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S0_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_S0_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_S0_PMON_EVNTSEL0 is defined as MSR_S0_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_S0_PMON_EVNTSEL0           0x00000721


/**
  Package. Uncore SBo 0 perfmon event select for SBo 0 counter 1.

  @param  ECX  MSR_HASWELL_E_S0_PMON_EVNTSEL1 (0x00000722)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S0_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_S0_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_S0_PMON_EVNTSEL1 is defined as MSR_S0_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_S0_PMON_EVNTSEL1           0x00000722


/**
  Package. Uncore SBo 0 perfmon event select for SBo 0 counter 2.

  @param  ECX  MSR_HASWELL_E_S0_PMON_EVNTSEL2 (0x00000723)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S0_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_S0_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_S0_PMON_EVNTSEL2 is defined as MSR_S0_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_S0_PMON_EVNTSEL2           0x00000723


/**
  Package. Uncore SBo 0 perfmon event select for SBo 0 counter 3.

  @param  ECX  MSR_HASWELL_E_S0_PMON_EVNTSEL3 (0x00000724)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S0_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_S0_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_S0_PMON_EVNTSEL3 is defined as MSR_S0_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_S0_PMON_EVNTSEL3           0x00000724


/**
  Package. Uncore SBo 0 perfmon box-wide filter.

  @param  ECX  MSR_HASWELL_E_S0_PMON_BOX_FILTER (0x00000725)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S0_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_HASWELL_E_S0_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_HASWELL_E_S0_PMON_BOX_FILTER is defined as MSR_S0_PMON_BOX_FILTER in SDM.
**/
#define MSR_HASWELL_E_S0_PMON_BOX_FILTER         0x00000725


/**
  Package. Uncore SBo 0 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_S0_PMON_CTR0 (0x00000726)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S0_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_S0_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_S0_PMON_CTR0 is defined as MSR_S0_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_S0_PMON_CTR0               0x00000726


/**
  Package. Uncore SBo 0 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_S0_PMON_CTR1 (0x00000727)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S0_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_S0_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_S0_PMON_CTR1 is defined as MSR_S0_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_S0_PMON_CTR1               0x00000727


/**
  Package. Uncore SBo 0 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_S0_PMON_CTR2 (0x00000728)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S0_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_S0_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_S0_PMON_CTR2 is defined as MSR_S0_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_S0_PMON_CTR2               0x00000728


/**
  Package. Uncore SBo 0 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_S0_PMON_CTR3 (0x00000729)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S0_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_S0_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_S0_PMON_CTR3 is defined as MSR_S0_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_S0_PMON_CTR3               0x00000729


/**
  Package. Uncore SBo 1 perfmon for SBo 1 box-wide control.

  @param  ECX  MSR_HASWELL_E_S1_PMON_BOX_CTL (0x0000072A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S1_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_S1_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_S1_PMON_BOX_CTL is defined as MSR_S1_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_S1_PMON_BOX_CTL            0x0000072A


/**
  Package. Uncore SBo 1 perfmon event select for SBo 1 counter 0.

  @param  ECX  MSR_HASWELL_E_S1_PMON_EVNTSEL0 (0x0000072B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S1_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_S1_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_S1_PMON_EVNTSEL0 is defined as MSR_S1_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_S1_PMON_EVNTSEL0           0x0000072B


/**
  Package. Uncore SBo 1 perfmon event select for SBo 1 counter 1.

  @param  ECX  MSR_HASWELL_E_S1_PMON_EVNTSEL1 (0x0000072C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S1_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_S1_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_S1_PMON_EVNTSEL1 is defined as MSR_S1_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_S1_PMON_EVNTSEL1           0x0000072C


/**
  Package. Uncore SBo 1 perfmon event select for SBo 1 counter 2.

  @param  ECX  MSR_HASWELL_E_S1_PMON_EVNTSEL2 (0x0000072D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S1_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_S1_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_S1_PMON_EVNTSEL2 is defined as MSR_S1_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_S1_PMON_EVNTSEL2           0x0000072D


/**
  Package. Uncore SBo 1 perfmon event select for SBo 1 counter 3.

  @param  ECX  MSR_HASWELL_E_S1_PMON_EVNTSEL3 (0x0000072E)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S1_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_S1_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_S1_PMON_EVNTSEL3 is defined as MSR_S1_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_S1_PMON_EVNTSEL3           0x0000072E


/**
  Package. Uncore SBo 1 perfmon box-wide filter.

  @param  ECX  MSR_HASWELL_E_S1_PMON_BOX_FILTER (0x0000072F)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S1_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_HASWELL_E_S1_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_HASWELL_E_S1_PMON_BOX_FILTER is defined as MSR_S1_PMON_BOX_FILTER in SDM.
**/
#define MSR_HASWELL_E_S1_PMON_BOX_FILTER         0x0000072F


/**
  Package. Uncore SBo 1 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_S1_PMON_CTR0 (0x00000730)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S1_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_S1_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_S1_PMON_CTR0 is defined as MSR_S1_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_S1_PMON_CTR0               0x00000730


/**
  Package. Uncore SBo 1 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_S1_PMON_CTR1 (0x00000731)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S1_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_S1_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_S1_PMON_CTR1 is defined as MSR_S1_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_S1_PMON_CTR1               0x00000731


/**
  Package. Uncore SBo 1 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_S1_PMON_CTR2 (0x00000732)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S1_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_S1_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_S1_PMON_CTR2 is defined as MSR_S1_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_S1_PMON_CTR2               0x00000732


/**
  Package. Uncore SBo 1 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_S1_PMON_CTR3 (0x00000733)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S1_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_S1_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_S1_PMON_CTR3 is defined as MSR_S1_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_S1_PMON_CTR3               0x00000733


/**
  Package. Uncore SBo 2 perfmon for SBo 2 box-wide control.

  @param  ECX  MSR_HASWELL_E_S2_PMON_BOX_CTL (0x00000734)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S2_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_S2_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_S2_PMON_BOX_CTL is defined as MSR_S2_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_S2_PMON_BOX_CTL            0x00000734


/**
  Package. Uncore SBo 2 perfmon event select for SBo 2 counter 0.

  @param  ECX  MSR_HASWELL_E_S2_PMON_EVNTSEL0 (0x00000735)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S2_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_S2_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_S2_PMON_EVNTSEL0 is defined as MSR_S2_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_S2_PMON_EVNTSEL0           0x00000735


/**
  Package. Uncore SBo 2 perfmon event select for SBo 2 counter 1.

  @param  ECX  MSR_HASWELL_E_S2_PMON_EVNTSEL1 (0x00000736)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S2_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_S2_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_S2_PMON_EVNTSEL1 is defined as MSR_S2_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_S2_PMON_EVNTSEL1           0x00000736


/**
  Package. Uncore SBo 2 perfmon event select for SBo 2 counter 2.

  @param  ECX  MSR_HASWELL_E_S2_PMON_EVNTSEL2 (0x00000737)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S2_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_S2_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_S2_PMON_EVNTSEL2 is defined as MSR_S2_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_S2_PMON_EVNTSEL2           0x00000737


/**
  Package. Uncore SBo 2 perfmon event select for SBo 2 counter 3.

  @param  ECX  MSR_HASWELL_E_S2_PMON_EVNTSEL3 (0x00000738)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S2_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_S2_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_S2_PMON_EVNTSEL3 is defined as MSR_S2_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_S2_PMON_EVNTSEL3           0x00000738


/**
  Package. Uncore SBo 2 perfmon box-wide filter.

  @param  ECX  MSR_HASWELL_E_S2_PMON_BOX_FILTER (0x00000739)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S2_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_HASWELL_E_S2_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_HASWELL_E_S2_PMON_BOX_FILTER is defined as MSR_S2_PMON_BOX_FILTER in SDM.
**/
#define MSR_HASWELL_E_S2_PMON_BOX_FILTER         0x00000739


/**
  Package. Uncore SBo 2 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_S2_PMON_CTR0 (0x0000073A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S2_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_S2_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_S2_PMON_CTR0 is defined as MSR_S2_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_S2_PMON_CTR0               0x0000073A


/**
  Package. Uncore SBo 2 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_S2_PMON_CTR1 (0x0000073B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S2_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_S2_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_S2_PMON_CTR1 is defined as MSR_S2_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_S2_PMON_CTR1               0x0000073B


/**
  Package. Uncore SBo 2 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_S2_PMON_CTR2 (0x0000073C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S2_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_S2_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_S2_PMON_CTR2 is defined as MSR_S2_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_S2_PMON_CTR2               0x0000073C


/**
  Package. Uncore SBo 2 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_S2_PMON_CTR3 (0x0000073D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S2_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_S2_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_S2_PMON_CTR3 is defined as MSR_S2_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_S2_PMON_CTR3               0x0000073D


/**
  Package. Uncore SBo 3 perfmon for SBo 3 box-wide control.

  @param  ECX  MSR_HASWELL_E_S3_PMON_BOX_CTL (0x0000073E)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S3_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_S3_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_S3_PMON_BOX_CTL is defined as MSR_S3_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_S3_PMON_BOX_CTL            0x0000073E


/**
  Package. Uncore SBo 3 perfmon event select for SBo 3 counter 0.

  @param  ECX  MSR_HASWELL_E_S3_PMON_EVNTSEL0 (0x0000073F)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S3_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_S3_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_S3_PMON_EVNTSEL0 is defined as MSR_S3_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_S3_PMON_EVNTSEL0           0x0000073F


/**
  Package. Uncore SBo 3 perfmon event select for SBo 3 counter 1.

  @param  ECX  MSR_HASWELL_E_S3_PMON_EVNTSEL1 (0x00000740)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S3_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_S3_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_S3_PMON_EVNTSEL1 is defined as MSR_S3_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_S3_PMON_EVNTSEL1           0x00000740


/**
  Package. Uncore SBo 3 perfmon event select for SBo 3 counter 2.

  @param  ECX  MSR_HASWELL_E_S3_PMON_EVNTSEL2 (0x00000741)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S3_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_S3_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_S3_PMON_EVNTSEL2 is defined as MSR_S3_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_S3_PMON_EVNTSEL2           0x00000741


/**
  Package. Uncore SBo 3 perfmon event select for SBo 3 counter 3.

  @param  ECX  MSR_HASWELL_E_S3_PMON_EVNTSEL3 (0x00000742)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S3_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_S3_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_S3_PMON_EVNTSEL3 is defined as MSR_S3_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_S3_PMON_EVNTSEL3           0x00000742


/**
  Package. Uncore SBo 3 perfmon box-wide filter.

  @param  ECX  MSR_HASWELL_E_S3_PMON_BOX_FILTER (0x00000743)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S3_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_HASWELL_E_S3_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_HASWELL_E_S3_PMON_BOX_FILTER is defined as MSR_S3_PMON_BOX_FILTER in SDM.
**/
#define MSR_HASWELL_E_S3_PMON_BOX_FILTER         0x00000743


/**
  Package. Uncore SBo 3 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_S3_PMON_CTR0 (0x00000744)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S3_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_S3_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_S3_PMON_CTR0 is defined as MSR_S3_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_S3_PMON_CTR0               0x00000744


/**
  Package. Uncore SBo 3 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_S3_PMON_CTR1 (0x00000745)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S3_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_S3_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_S3_PMON_CTR1 is defined as MSR_S3_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_S3_PMON_CTR1               0x00000745


/**
  Package. Uncore SBo 3 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_S3_PMON_CTR2 (0x00000746)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S3_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_S3_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_S3_PMON_CTR2 is defined as MSR_S3_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_S3_PMON_CTR2               0x00000746


/**
  Package. Uncore SBo 3 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_S3_PMON_CTR3 (0x00000747)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_S3_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_S3_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_S3_PMON_CTR3 is defined as MSR_S3_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_S3_PMON_CTR3               0x00000747


/**
  Package. Uncore C-box 0 perfmon for box-wide control.

  @param  ECX  MSR_HASWELL_E_C0_PMON_BOX_CTL (0x00000E00)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C0_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C0_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C0_PMON_BOX_CTL is defined as MSR_C0_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C0_PMON_BOX_CTL            0x00000E00


/**
  Package. Uncore C-box 0 perfmon event select for C-box 0 counter 0.

  @param  ECX  MSR_HASWELL_E_C0_PMON_EVNTSEL0 (0x00000E01)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C0_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C0_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C0_PMON_EVNTSEL0 is defined as MSR_C0_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C0_PMON_EVNTSEL0           0x00000E01


/**
  Package. Uncore C-box 0 perfmon event select for C-box 0 counter 1.

  @param  ECX  MSR_HASWELL_E_C0_PMON_EVNTSEL1 (0x00000E02)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C0_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C0_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C0_PMON_EVNTSEL1 is defined as MSR_C0_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C0_PMON_EVNTSEL1           0x00000E02


/**
  Package. Uncore C-box 0 perfmon event select for C-box 0 counter 2.

  @param  ECX  MSR_HASWELL_E_C0_PMON_EVNTSEL2 (0x00000E03)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C0_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C0_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C0_PMON_EVNTSEL2 is defined as MSR_C0_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C0_PMON_EVNTSEL2           0x00000E03


/**
  Package. Uncore C-box 0 perfmon event select for C-box 0 counter 3.

  @param  ECX  MSR_HASWELL_E_C0_PMON_EVNTSEL3 (0x00000E04)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C0_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C0_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C0_PMON_EVNTSEL3 is defined as MSR_C0_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C0_PMON_EVNTSEL3           0x00000E04


/**
  Package. Uncore C-box 0 perfmon box wide filter 0.

  @param  ECX  MSR_HASWELL_E_C0_PMON_BOX_FILTER0 (0x00000E05)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C0_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C0_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C0_PMON_BOX_FILTER0 is defined as MSR_C0_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C0_PMON_BOX_FILTER0        0x00000E05


/**
  Package. Uncore C-box 0 perfmon box wide filter 1.

  @param  ECX  MSR_HASWELL_E_C0_PMON_BOX_FILTER1 (0x00000E06)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C0_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C0_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C0_PMON_BOX_FILTER1 is defined as MSR_C0_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C0_PMON_BOX_FILTER1        0x00000E06


/**
  Package. Uncore C-box 0 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C0_PMON_BOX_STATUS (0x00000E07)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C0_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C0_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C0_PMON_BOX_STATUS is defined as MSR_C0_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C0_PMON_BOX_STATUS         0x00000E07


/**
  Package. Uncore C-box 0 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C0_PMON_CTR0 (0x00000E08)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C0_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C0_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C0_PMON_CTR0 is defined as MSR_C0_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C0_PMON_CTR0               0x00000E08


/**
  Package. Uncore C-box 0 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C0_PMON_CTR1 (0x00000E09)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C0_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C0_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C0_PMON_CTR1 is defined as MSR_C0_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C0_PMON_CTR1               0x00000E09


/**
  Package. Uncore C-box 0 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C0_PMON_CTR2 (0x00000E0A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C0_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C0_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C0_PMON_CTR2 is defined as MSR_C0_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C0_PMON_CTR2               0x00000E0A


/**
  Package. Uncore C-box 0 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C0_PMON_CTR3 (0x00000E0B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C0_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C0_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C0_PMON_CTR3 is defined as MSR_C0_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C0_PMON_CTR3               0x00000E0B


/**
  Package. Uncore C-box 1 perfmon for box-wide control.

  @param  ECX  MSR_HASWELL_E_C1_PMON_BOX_CTL (0x00000E10)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C1_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C1_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C1_PMON_BOX_CTL is defined as MSR_C1_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C1_PMON_BOX_CTL            0x00000E10


/**
  Package. Uncore C-box 1 perfmon event select for C-box 1 counter 0.

  @param  ECX  MSR_HASWELL_E_C1_PMON_EVNTSEL0 (0x00000E11)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C1_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C1_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C1_PMON_EVNTSEL0 is defined as MSR_C1_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C1_PMON_EVNTSEL0           0x00000E11


/**
  Package. Uncore C-box 1 perfmon event select for C-box 1 counter 1.

  @param  ECX  MSR_HASWELL_E_C1_PMON_EVNTSEL1 (0x00000E12)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C1_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C1_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C1_PMON_EVNTSEL1 is defined as MSR_C1_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C1_PMON_EVNTSEL1           0x00000E12


/**
  Package. Uncore C-box 1 perfmon event select for C-box 1 counter 2.

  @param  ECX  MSR_HASWELL_E_C1_PMON_EVNTSEL2 (0x00000E13)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C1_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C1_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C1_PMON_EVNTSEL2 is defined as MSR_C1_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C1_PMON_EVNTSEL2           0x00000E13


/**
  Package. Uncore C-box 1 perfmon event select for C-box 1 counter 3.

  @param  ECX  MSR_HASWELL_E_C1_PMON_EVNTSEL3 (0x00000E14)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C1_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C1_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C1_PMON_EVNTSEL3 is defined as MSR_C1_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C1_PMON_EVNTSEL3           0x00000E14


/**
  Package. Uncore C-box 1 perfmon box wide filter 0.

  @param  ECX  MSR_HASWELL_E_C1_PMON_BOX_FILTER0 (0x00000E15)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C1_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C1_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C1_PMON_BOX_FILTER0 is defined as MSR_C1_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C1_PMON_BOX_FILTER0        0x00000E15


/**
  Package. Uncore C-box 1 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C1_PMON_BOX_FILTER1 (0x00000E16)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C1_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C1_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C1_PMON_BOX_FILTER1 is defined as MSR_C1_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C1_PMON_BOX_FILTER1        0x00000E16


/**
  Package. Uncore C-box 1 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C1_PMON_BOX_STATUS (0x00000E17)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C1_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C1_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C1_PMON_BOX_STATUS is defined as MSR_C1_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C1_PMON_BOX_STATUS         0x00000E17


/**
  Package. Uncore C-box 1 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C1_PMON_CTR0 (0x00000E18)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C1_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C1_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C1_PMON_CTR0 is defined as MSR_C1_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C1_PMON_CTR0               0x00000E18


/**
  Package. Uncore C-box 1 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C1_PMON_CTR1 (0x00000E19)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C1_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C1_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C1_PMON_CTR1 is defined as MSR_C1_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C1_PMON_CTR1               0x00000E19


/**
  Package. Uncore C-box 1 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C1_PMON_CTR2 (0x00000E1A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C1_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C1_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C1_PMON_CTR2 is defined as MSR_C1_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C1_PMON_CTR2               0x00000E1A


/**
  Package. Uncore C-box 1 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C1_PMON_CTR3 (0x00000E1B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C1_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C1_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C1_PMON_CTR3 is defined as MSR_C1_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C1_PMON_CTR3               0x00000E1B


/**
  Package. Uncore C-box 2 perfmon for box-wide control.

  @param  ECX  MSR_HASWELL_E_C2_PMON_BOX_CTL (0x00000E20)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C2_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C2_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C2_PMON_BOX_CTL is defined as MSR_C2_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C2_PMON_BOX_CTL            0x00000E20


/**
  Package. Uncore C-box 2 perfmon event select for C-box 2 counter 0.

  @param  ECX  MSR_HASWELL_E_C2_PMON_EVNTSEL0 (0x00000E21)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C2_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C2_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C2_PMON_EVNTSEL0 is defined as MSR_C2_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C2_PMON_EVNTSEL0           0x00000E21


/**
  Package. Uncore C-box 2 perfmon event select for C-box 2 counter 1.

  @param  ECX  MSR_HASWELL_E_C2_PMON_EVNTSEL1 (0x00000E22)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C2_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C2_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C2_PMON_EVNTSEL1 is defined as MSR_C2_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C2_PMON_EVNTSEL1           0x00000E22


/**
  Package. Uncore C-box 2 perfmon event select for C-box 2 counter 2.

  @param  ECX  MSR_HASWELL_E_C2_PMON_EVNTSEL2 (0x00000E23)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C2_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C2_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C2_PMON_EVNTSEL2 is defined as MSR_C2_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C2_PMON_EVNTSEL2           0x00000E23


/**
  Package. Uncore C-box 2 perfmon event select for C-box 2 counter 3.

  @param  ECX  MSR_HASWELL_E_C2_PMON_EVNTSEL3 (0x00000E24)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C2_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C2_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C2_PMON_EVNTSEL3 is defined as MSR_C2_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C2_PMON_EVNTSEL3           0x00000E24


/**
  Package. Uncore C-box 2 perfmon box wide filter 0.

  @param  ECX  MSR_HASWELL_E_C2_PMON_BOX_FILTER0 (0x00000E25)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C2_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C2_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C2_PMON_BOX_FILTER0 is defined as MSR_C2_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C2_PMON_BOX_FILTER0        0x00000E25


/**
  Package. Uncore C-box 2 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C2_PMON_BOX_FILTER1 (0x00000E26)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C2_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C2_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C2_PMON_BOX_FILTER1 is defined as MSR_C2_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C2_PMON_BOX_FILTER1        0x00000E26


/**
  Package. Uncore C-box 2 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C2_PMON_BOX_STATUS (0x00000E27)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C2_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C2_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C2_PMON_BOX_STATUS is defined as MSR_C2_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C2_PMON_BOX_STATUS         0x00000E27


/**
  Package. Uncore C-box 2 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C2_PMON_CTR0 (0x00000E28)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C2_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C2_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C2_PMON_CTR0 is defined as MSR_C2_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C2_PMON_CTR0               0x00000E28


/**
  Package. Uncore C-box 2 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C2_PMON_CTR1 (0x00000E29)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C2_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C2_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C2_PMON_CTR1 is defined as MSR_C2_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C2_PMON_CTR1               0x00000E29


/**
  Package. Uncore C-box 2 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C2_PMON_CTR2 (0x00000E2A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C2_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C2_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C2_PMON_CTR2 is defined as MSR_C2_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C2_PMON_CTR2               0x00000E2A


/**
  Package. Uncore C-box 2 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C2_PMON_CTR3 (0x00000E2B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C2_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C2_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C2_PMON_CTR3 is defined as MSR_C2_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C2_PMON_CTR3               0x00000E2B


/**
  Package. Uncore C-box 3 perfmon for box-wide control.

  @param  ECX  MSR_HASWELL_E_C3_PMON_BOX_CTL (0x00000E30)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C3_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C3_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C3_PMON_BOX_CTL is defined as MSR_C3_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C3_PMON_BOX_CTL            0x00000E30


/**
  Package. Uncore C-box 3 perfmon event select for C-box 3 counter 0.

  @param  ECX  MSR_HASWELL_E_C3_PMON_EVNTSEL0 (0x00000E31)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C3_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C3_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C3_PMON_EVNTSEL0 is defined as MSR_C3_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C3_PMON_EVNTSEL0           0x00000E31


/**
  Package. Uncore C-box 3 perfmon event select for C-box 3 counter 1.

  @param  ECX  MSR_HASWELL_E_C3_PMON_EVNTSEL1 (0x00000E32)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C3_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C3_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C3_PMON_EVNTSEL1 is defined as MSR_C3_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C3_PMON_EVNTSEL1           0x00000E32


/**
  Package. Uncore C-box 3 perfmon event select for C-box 3 counter 2.

  @param  ECX  MSR_HASWELL_E_C3_PMON_EVNTSEL2 (0x00000E33)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C3_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C3_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C3_PMON_EVNTSEL2 is defined as MSR_C3_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C3_PMON_EVNTSEL2           0x00000E33


/**
  Package. Uncore C-box 3 perfmon event select for C-box 3 counter 3.

  @param  ECX  MSR_HASWELL_E_C3_PMON_EVNTSEL3 (0x00000E34)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C3_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C3_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C3_PMON_EVNTSEL3 is defined as MSR_C3_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C3_PMON_EVNTSEL3           0x00000E34


/**
  Package. Uncore C-box 3 perfmon box wide filter 0.

  @param  ECX  MSR_HASWELL_E_C3_PMON_BOX_FILTER0 (0x00000E35)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C3_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C3_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C3_PMON_BOX_FILTER0 is defined as MSR_C3_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C3_PMON_BOX_FILTER0        0x00000E35


/**
  Package. Uncore C-box 3 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C3_PMON_BOX_FILTER1 (0x00000E36)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C3_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C3_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C3_PMON_BOX_FILTER1 is defined as MSR_C3_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C3_PMON_BOX_FILTER1        0x00000E36


/**
  Package. Uncore C-box 3 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C3_PMON_BOX_STATUS (0x00000E37)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C3_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C3_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C3_PMON_BOX_STATUS is defined as MSR_C3_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C3_PMON_BOX_STATUS         0x00000E37


/**
  Package. Uncore C-box 3 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C3_PMON_CTR0 (0x00000E38)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C3_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C3_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C3_PMON_CTR0 is defined as MSR_C3_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C3_PMON_CTR0               0x00000E38


/**
  Package. Uncore C-box 3 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C3_PMON_CTR1 (0x00000E39)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C3_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C3_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C3_PMON_CTR1 is defined as MSR_C3_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C3_PMON_CTR1               0x00000E39


/**
  Package. Uncore C-box 3 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C3_PMON_CTR2 (0x00000E3A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C3_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C3_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C3_PMON_CTR2 is defined as MSR_C3_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C3_PMON_CTR2               0x00000E3A


/**
  Package. Uncore C-box 3 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C3_PMON_CTR3 (0x00000E3B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C3_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C3_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C3_PMON_CTR3 is defined as MSR_C3_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C3_PMON_CTR3               0x00000E3B


/**
  Package. Uncore C-box 4 perfmon for box-wide control.

  @param  ECX  MSR_HASWELL_E_C4_PMON_BOX_CTL (0x00000E40)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C4_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C4_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C4_PMON_BOX_CTL is defined as MSR_C4_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C4_PMON_BOX_CTL            0x00000E40


/**
  Package. Uncore C-box 4 perfmon event select for C-box 4 counter 0.

  @param  ECX  MSR_HASWELL_E_C4_PMON_EVNTSEL0 (0x00000E41)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C4_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C4_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C4_PMON_EVNTSEL0 is defined as MSR_C4_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C4_PMON_EVNTSEL0           0x00000E41


/**
  Package. Uncore C-box 4 perfmon event select for C-box 4 counter 1.

  @param  ECX  MSR_HASWELL_E_C4_PMON_EVNTSEL1 (0x00000E42)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C4_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C4_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C4_PMON_EVNTSEL1 is defined as MSR_C4_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C4_PMON_EVNTSEL1           0x00000E42


/**
  Package. Uncore C-box 4 perfmon event select for C-box 4 counter 2.

  @param  ECX  MSR_HASWELL_E_C4_PMON_EVNTSEL2 (0x00000E43)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C4_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C4_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C4_PMON_EVNTSEL2 is defined as MSR_C4_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C4_PMON_EVNTSEL2           0x00000E43


/**
  Package. Uncore C-box 4 perfmon event select for C-box 4 counter 3.

  @param  ECX  MSR_HASWELL_E_C4_PMON_EVNTSEL3 (0x00000E44)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C4_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C4_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C4_PMON_EVNTSEL3 is defined as MSR_C4_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C4_PMON_EVNTSEL3           0x00000E44


/**
  Package. Uncore C-box 4 perfmon box wide filter 0.

  @param  ECX  MSR_HASWELL_E_C4_PMON_BOX_FILTER0 (0x00000E45)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C4_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C4_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C4_PMON_BOX_FILTER0 is defined as MSR_C4_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C4_PMON_BOX_FILTER0        0x00000E45


/**
  Package. Uncore C-box 4 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C4_PMON_BOX_FILTER1 (0x00000E46)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C4_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C4_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C4_PMON_BOX_FILTER1 is defined as MSR_C4_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C4_PMON_BOX_FILTER1        0x00000E46


/**
  Package. Uncore C-box 4 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C4_PMON_BOX_STATUS (0x00000E47)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C4_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C4_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C4_PMON_BOX_STATUS is defined as MSR_C4_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C4_PMON_BOX_STATUS         0x00000E47


/**
  Package. Uncore C-box 4 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C4_PMON_CTR0 (0x00000E48)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C4_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C4_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C4_PMON_CTR0 is defined as MSR_C4_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C4_PMON_CTR0               0x00000E48


/**
  Package. Uncore C-box 4 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C4_PMON_CTR1 (0x00000E49)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C4_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C4_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C4_PMON_CTR1 is defined as MSR_C4_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C4_PMON_CTR1               0x00000E49


/**
  Package. Uncore C-box 4 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C4_PMON_CTR2 (0x00000E4A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C4_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C4_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C4_PMON_CTR2 is defined as MSR_C4_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C4_PMON_CTR2               0x00000E4A


/**
  Package. Uncore C-box 4 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C4_PMON_CTR3 (0x00000E4B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C4_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C4_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C4_PMON_CTR3 is defined as MSR_C4_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C4_PMON_CTR3               0x00000E4B


/**
  Package. Uncore C-box 5 perfmon for box-wide control.

  @param  ECX  MSR_HASWELL_E_C5_PMON_BOX_CTL (0x00000E50)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C5_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C5_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C5_PMON_BOX_CTL is defined as MSR_C5_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C5_PMON_BOX_CTL            0x00000E50


/**
  Package. Uncore C-box 5 perfmon event select for C-box 5 counter 0.

  @param  ECX  MSR_HASWELL_E_C5_PMON_EVNTSEL0 (0x00000E51)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C5_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C5_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C5_PMON_EVNTSEL0 is defined as MSR_C5_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C5_PMON_EVNTSEL0           0x00000E51


/**
  Package. Uncore C-box 5 perfmon event select for C-box 5 counter 1.

  @param  ECX  MSR_HASWELL_E_C5_PMON_EVNTSEL1 (0x00000E52)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C5_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C5_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C5_PMON_EVNTSEL1 is defined as MSR_C5_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C5_PMON_EVNTSEL1           0x00000E52


/**
  Package. Uncore C-box 5 perfmon event select for C-box 5 counter 2.

  @param  ECX  MSR_HASWELL_E_C5_PMON_EVNTSEL2 (0x00000E53)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C5_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C5_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C5_PMON_EVNTSEL2 is defined as MSR_C5_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C5_PMON_EVNTSEL2           0x00000E53


/**
  Package. Uncore C-box 5 perfmon event select for C-box 5 counter 3.

  @param  ECX  MSR_HASWELL_E_C5_PMON_EVNTSEL3 (0x00000E54)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C5_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C5_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C5_PMON_EVNTSEL3 is defined as MSR_C5_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C5_PMON_EVNTSEL3           0x00000E54


/**
  Package. Uncore C-box 5 perfmon box wide filter 0.

  @param  ECX  MSR_HASWELL_E_C5_PMON_BOX_FILTER0 (0x00000E55)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C5_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C5_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C5_PMON_BOX_FILTER0 is defined as MSR_C5_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C5_PMON_BOX_FILTER0        0x00000E55


/**
  Package. Uncore C-box 5 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C5_PMON_BOX_FILTER1 (0x00000E56)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C5_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C5_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C5_PMON_BOX_FILTER1 is defined as MSR_C5_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C5_PMON_BOX_FILTER1        0x00000E56


/**
  Package. Uncore C-box 5 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C5_PMON_BOX_STATUS (0x00000E57)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C5_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C5_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C5_PMON_BOX_STATUS is defined as MSR_C5_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C5_PMON_BOX_STATUS         0x00000E57


/**
  Package. Uncore C-box 5 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C5_PMON_CTR0 (0x00000E58)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C5_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C5_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C5_PMON_CTR0 is defined as MSR_C5_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C5_PMON_CTR0               0x00000E58


/**
  Package. Uncore C-box 5 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C5_PMON_CTR1 (0x00000E59)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C5_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C5_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C5_PMON_CTR1 is defined as MSR_C5_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C5_PMON_CTR1               0x00000E59


/**
  Package. Uncore C-box 5 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C5_PMON_CTR2 (0x00000E5A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C5_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C5_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C5_PMON_CTR2 is defined as MSR_C5_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C5_PMON_CTR2               0x00000E5A


/**
  Package. Uncore C-box 5 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C5_PMON_CTR3 (0x00000E5B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C5_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C5_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C5_PMON_CTR3 is defined as MSR_C5_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C5_PMON_CTR3               0x00000E5B


/**
  Package. Uncore C-box 6 perfmon for box-wide control.

  @param  ECX  MSR_HASWELL_E_C6_PMON_BOX_CTL (0x00000E60)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C6_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C6_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C6_PMON_BOX_CTL is defined as MSR_C6_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C6_PMON_BOX_CTL            0x00000E60


/**
  Package. Uncore C-box 6 perfmon event select for C-box 6 counter 0.

  @param  ECX  MSR_HASWELL_E_C6_PMON_EVNTSEL0 (0x00000E61)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C6_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C6_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C6_PMON_EVNTSEL0 is defined as MSR_C6_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C6_PMON_EVNTSEL0           0x00000E61


/**
  Package. Uncore C-box 6 perfmon event select for C-box 6 counter 1.

  @param  ECX  MSR_HASWELL_E_C6_PMON_EVNTSEL1 (0x00000E62)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C6_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C6_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C6_PMON_EVNTSEL1 is defined as MSR_C6_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C6_PMON_EVNTSEL1           0x00000E62


/**
  Package. Uncore C-box 6 perfmon event select for C-box 6 counter 2.

  @param  ECX  MSR_HASWELL_E_C6_PMON_EVNTSEL2 (0x00000E63)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C6_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C6_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C6_PMON_EVNTSEL2 is defined as MSR_C6_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C6_PMON_EVNTSEL2           0x00000E63


/**
  Package. Uncore C-box 6 perfmon event select for C-box 6 counter 3.

  @param  ECX  MSR_HASWELL_E_C6_PMON_EVNTSEL3 (0x00000E64)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C6_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C6_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C6_PMON_EVNTSEL3 is defined as MSR_C6_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C6_PMON_EVNTSEL3           0x00000E64


/**
  Package. Uncore C-box 6 perfmon box wide filter 0.

  @param  ECX  MSR_HASWELL_E_C6_PMON_BOX_FILTER0 (0x00000E65)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C6_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C6_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C6_PMON_BOX_FILTER0 is defined as MSR_C6_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C6_PMON_BOX_FILTER0        0x00000E65


/**
  Package. Uncore C-box 6 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C6_PMON_BOX_FILTER1 (0x00000E66)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C6_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C6_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C6_PMON_BOX_FILTER1 is defined as MSR_C6_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C6_PMON_BOX_FILTER1        0x00000E66


/**
  Package. Uncore C-box 6 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C6_PMON_BOX_STATUS (0x00000E67)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C6_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C6_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C6_PMON_BOX_STATUS is defined as MSR_C6_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C6_PMON_BOX_STATUS         0x00000E67


/**
  Package. Uncore C-box 6 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C6_PMON_CTR0 (0x00000E68)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C6_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C6_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C6_PMON_CTR0 is defined as MSR_C6_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C6_PMON_CTR0               0x00000E68


/**
  Package. Uncore C-box 6 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C6_PMON_CTR1 (0x00000E69)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C6_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C6_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C6_PMON_CTR1 is defined as MSR_C6_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C6_PMON_CTR1               0x00000E69


/**
  Package. Uncore C-box 6 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C6_PMON_CTR2 (0x00000E6A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C6_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C6_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C6_PMON_CTR2 is defined as MSR_C6_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C6_PMON_CTR2               0x00000E6A


/**
  Package. Uncore C-box 6 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C6_PMON_CTR3 (0x00000E6B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C6_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C6_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C6_PMON_CTR3 is defined as MSR_C6_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C6_PMON_CTR3               0x00000E6B


/**
  Package. Uncore C-box 7 perfmon for box-wide control.

  @param  ECX  MSR_HASWELL_E_C7_PMON_BOX_CTL (0x00000E70)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C7_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C7_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C7_PMON_BOX_CTL is defined as MSR_C7_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C7_PMON_BOX_CTL            0x00000E70


/**
  Package. Uncore C-box 7 perfmon event select for C-box 7 counter 0.

  @param  ECX  MSR_HASWELL_E_C7_PMON_EVNTSEL0 (0x00000E71)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C7_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C7_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C7_PMON_EVNTSEL0 is defined as MSR_C7_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C7_PMON_EVNTSEL0           0x00000E71


/**
  Package. Uncore C-box 7 perfmon event select for C-box 7 counter 1.

  @param  ECX  MSR_HASWELL_E_C7_PMON_EVNTSEL1 (0x00000E72)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C7_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C7_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C7_PMON_EVNTSEL1 is defined as MSR_C7_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C7_PMON_EVNTSEL1           0x00000E72


/**
  Package. Uncore C-box 7 perfmon event select for C-box 7 counter 2.

  @param  ECX  MSR_HASWELL_E_C7_PMON_EVNTSEL2 (0x00000E73)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C7_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C7_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C7_PMON_EVNTSEL2 is defined as MSR_C7_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C7_PMON_EVNTSEL2           0x00000E73


/**
  Package. Uncore C-box 7 perfmon event select for C-box 7 counter 3.

  @param  ECX  MSR_HASWELL_E_C7_PMON_EVNTSEL3 (0x00000E74)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C7_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C7_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C7_PMON_EVNTSEL3 is defined as MSR_C7_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C7_PMON_EVNTSEL3           0x00000E74


/**
  Package. Uncore C-box 7 perfmon box wide filter 0.

  @param  ECX  MSR_HASWELL_E_C7_PMON_BOX_FILTER0 (0x00000E75)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C7_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C7_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C7_PMON_BOX_FILTER0 is defined as MSR_C7_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C7_PMON_BOX_FILTER0        0x00000E75


/**
  Package. Uncore C-box 7 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C7_PMON_BOX_FILTER1 (0x00000E76)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C7_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C7_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C7_PMON_BOX_FILTER1 is defined as MSR_C7_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C7_PMON_BOX_FILTER1        0x00000E76


/**
  Package. Uncore C-box 7 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C7_PMON_BOX_STATUS (0x00000E77)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C7_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C7_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C7_PMON_BOX_STATUS is defined as MSR_C7_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C7_PMON_BOX_STATUS         0x00000E77


/**
  Package. Uncore C-box 7 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C7_PMON_CTR0 (0x00000E78)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C7_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C7_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C7_PMON_CTR0 is defined as MSR_C7_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C7_PMON_CTR0               0x00000E78


/**
  Package. Uncore C-box 7 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C7_PMON_CTR1 (0x00000E79)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C7_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C7_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C7_PMON_CTR1 is defined as MSR_C7_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C7_PMON_CTR1               0x00000E79


/**
  Package. Uncore C-box 7 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C7_PMON_CTR2 (0x00000E7A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C7_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C7_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C7_PMON_CTR2 is defined as MSR_C7_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C7_PMON_CTR2               0x00000E7A


/**
  Package. Uncore C-box 7 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C7_PMON_CTR3 (0x00000E7B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C7_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C7_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C7_PMON_CTR3 is defined as MSR_C7_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C7_PMON_CTR3               0x00000E7B


/**
  Package. Uncore C-box 8 perfmon local box wide control.

  @param  ECX  MSR_HASWELL_E_C8_PMON_BOX_CTL (0x00000E80)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C8_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C8_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C8_PMON_BOX_CTL is defined as MSR_C8_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C8_PMON_BOX_CTL            0x00000E80


/**
  Package. Uncore C-box 8 perfmon event select for C-box 8 counter 0.

  @param  ECX  MSR_HASWELL_E_C8_PMON_EVNTSEL0 (0x00000E81)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C8_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C8_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C8_PMON_EVNTSEL0 is defined as MSR_C8_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C8_PMON_EVNTSEL0           0x00000E81


/**
  Package. Uncore C-box 8 perfmon event select for C-box 8 counter 1.

  @param  ECX  MSR_HASWELL_E_C8_PMON_EVNTSEL1 (0x00000E82)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C8_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C8_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C8_PMON_EVNTSEL1 is defined as MSR_C8_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C8_PMON_EVNTSEL1           0x00000E82


/**
  Package. Uncore C-box 8 perfmon event select for C-box 8 counter 2.

  @param  ECX  MSR_HASWELL_E_C8_PMON_EVNTSEL2 (0x00000E83)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C8_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C8_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C8_PMON_EVNTSEL2 is defined as MSR_C8_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C8_PMON_EVNTSEL2           0x00000E83


/**
  Package. Uncore C-box 8 perfmon event select for C-box 8 counter 3.

  @param  ECX  MSR_HASWELL_E_C8_PMON_EVNTSEL3 (0x00000E84)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C8_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C8_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C8_PMON_EVNTSEL3 is defined as MSR_C8_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C8_PMON_EVNTSEL3           0x00000E84


/**
  Package. Uncore C-box 8 perfmon box wide filter0.

  @param  ECX  MSR_HASWELL_E_C8_PMON_BOX_FILTER0 (0x00000E85)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C8_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C8_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C8_PMON_BOX_FILTER0 is defined as MSR_C8_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C8_PMON_BOX_FILTER0        0x00000E85


/**
  Package. Uncore C-box 8 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C8_PMON_BOX_FILTER1 (0x00000E86)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C8_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C8_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C8_PMON_BOX_FILTER1 is defined as MSR_C8_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C8_PMON_BOX_FILTER1        0x00000E86


/**
  Package. Uncore C-box 8 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C8_PMON_BOX_STATUS (0x00000E87)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C8_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C8_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C8_PMON_BOX_STATUS is defined as MSR_C8_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C8_PMON_BOX_STATUS         0x00000E87


/**
  Package. Uncore C-box 8 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C8_PMON_CTR0 (0x00000E88)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C8_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C8_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C8_PMON_CTR0 is defined as MSR_C8_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C8_PMON_CTR0               0x00000E88


/**
  Package. Uncore C-box 8 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C8_PMON_CTR1 (0x00000E89)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C8_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C8_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C8_PMON_CTR1 is defined as MSR_C8_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C8_PMON_CTR1               0x00000E89


/**
  Package. Uncore C-box 8 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C8_PMON_CTR2 (0x00000E8A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C8_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C8_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C8_PMON_CTR2 is defined as MSR_C8_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C8_PMON_CTR2               0x00000E8A


/**
  Package. Uncore C-box 8 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C8_PMON_CTR3 (0x00000E8B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C8_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C8_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C8_PMON_CTR3 is defined as MSR_C8_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C8_PMON_CTR3               0x00000E8B


/**
  Package. Uncore C-box 9 perfmon local box wide control.

  @param  ECX  MSR_HASWELL_E_C9_PMON_BOX_CTL (0x00000E90)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C9_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C9_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C9_PMON_BOX_CTL is defined as MSR_C9_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C9_PMON_BOX_CTL            0x00000E90


/**
  Package. Uncore C-box 9 perfmon event select for C-box 9 counter 0.

  @param  ECX  MSR_HASWELL_E_C9_PMON_EVNTSEL0 (0x00000E91)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C9_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C9_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C9_PMON_EVNTSEL0 is defined as MSR_C9_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C9_PMON_EVNTSEL0           0x00000E91


/**
  Package. Uncore C-box 9 perfmon event select for C-box 9 counter 1.

  @param  ECX  MSR_HASWELL_E_C9_PMON_EVNTSEL1 (0x00000E92)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C9_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C9_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C9_PMON_EVNTSEL1 is defined as MSR_C9_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C9_PMON_EVNTSEL1           0x00000E92


/**
  Package. Uncore C-box 9 perfmon event select for C-box 9 counter 2.

  @param  ECX  MSR_HASWELL_E_C9_PMON_EVNTSEL2 (0x00000E93)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C9_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C9_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C9_PMON_EVNTSEL2 is defined as MSR_C9_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C9_PMON_EVNTSEL2           0x00000E93


/**
  Package. Uncore C-box 9 perfmon event select for C-box 9 counter 3.

  @param  ECX  MSR_HASWELL_E_C9_PMON_EVNTSEL3 (0x00000E94)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C9_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C9_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C9_PMON_EVNTSEL3 is defined as MSR_C9_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C9_PMON_EVNTSEL3           0x00000E94


/**
  Package. Uncore C-box 9 perfmon box wide filter0.

  @param  ECX  MSR_HASWELL_E_C9_PMON_BOX_FILTER0 (0x00000E95)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C9_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C9_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C9_PMON_BOX_FILTER0 is defined as MSR_C9_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C9_PMON_BOX_FILTER0        0x00000E95


/**
  Package. Uncore C-box 9 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C9_PMON_BOX_FILTER1 (0x00000E96)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C9_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C9_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C9_PMON_BOX_FILTER1 is defined as MSR_C9_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C9_PMON_BOX_FILTER1        0x00000E96


/**
  Package. Uncore C-box 9 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C9_PMON_BOX_STATUS (0x00000E97)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C9_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C9_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C9_PMON_BOX_STATUS is defined as MSR_C9_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C9_PMON_BOX_STATUS         0x00000E97


/**
  Package. Uncore C-box 9 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C9_PMON_CTR0 (0x00000E98)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C9_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C9_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C9_PMON_CTR0 is defined as MSR_C9_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C9_PMON_CTR0               0x00000E98


/**
  Package. Uncore C-box 9 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C9_PMON_CTR1 (0x00000E99)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C9_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C9_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C9_PMON_CTR1 is defined as MSR_C9_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C9_PMON_CTR1               0x00000E99


/**
  Package. Uncore C-box 9 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C9_PMON_CTR2 (0x00000E9A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C9_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C9_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C9_PMON_CTR2 is defined as MSR_C9_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C9_PMON_CTR2               0x00000E9A


/**
  Package. Uncore C-box 9 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C9_PMON_CTR3 (0x00000E9B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C9_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C9_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C9_PMON_CTR3 is defined as MSR_C9_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C9_PMON_CTR3               0x00000E9B


/**
  Package. Uncore C-box 10 perfmon local box wide control.

  @param  ECX  MSR_HASWELL_E_C10_PMON_BOX_CTL (0x00000EA0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C10_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C10_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C10_PMON_BOX_CTL is defined as MSR_C10_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C10_PMON_BOX_CTL           0x00000EA0


/**
  Package. Uncore C-box 10 perfmon event select for C-box 10 counter 0.

  @param  ECX  MSR_HASWELL_E_C10_PMON_EVNTSEL0 (0x00000EA1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C10_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C10_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C10_PMON_EVNTSEL0 is defined as MSR_C10_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C10_PMON_EVNTSEL0          0x00000EA1


/**
  Package. Uncore C-box 10 perfmon event select for C-box 10 counter 1.

  @param  ECX  MSR_HASWELL_E_C10_PMON_EVNTSEL1 (0x00000EA2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C10_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C10_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C10_PMON_EVNTSEL1 is defined as MSR_C10_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C10_PMON_EVNTSEL1          0x00000EA2


/**
  Package. Uncore C-box 10 perfmon event select for C-box 10 counter 2.

  @param  ECX  MSR_HASWELL_E_C10_PMON_EVNTSEL2 (0x00000EA3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C10_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C10_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C10_PMON_EVNTSEL2 is defined as MSR_C10_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C10_PMON_EVNTSEL2          0x00000EA3


/**
  Package. Uncore C-box 10 perfmon event select for C-box 10 counter 3.

  @param  ECX  MSR_HASWELL_E_C10_PMON_EVNTSEL3 (0x00000EA4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C10_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C10_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C10_PMON_EVNTSEL3 is defined as MSR_C10_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C10_PMON_EVNTSEL3          0x00000EA4


/**
  Package. Uncore C-box 10 perfmon box wide filter0.

  @param  ECX  MSR_HASWELL_E_C10_PMON_BOX_FILTER0 (0x00000EA5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C10_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C10_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C10_PMON_BOX_FILTER0 is defined as MSR_C10_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C10_PMON_BOX_FILTER0       0x00000EA5


/**
  Package. Uncore C-box 10 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C10_PMON_BOX_FILTER1 (0x00000EA6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C10_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C10_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C10_PMON_BOX_FILTER1 is defined as MSR_C10_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C10_PMON_BOX_FILTER1       0x00000EA6


/**
  Package. Uncore C-box 10 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C10_PMON_BOX_STATUS (0x00000EA7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C10_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C10_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C10_PMON_BOX_STATUS is defined as MSR_C10_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C10_PMON_BOX_STATUS        0x00000EA7


/**
  Package. Uncore C-box 10 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C10_PMON_CTR0 (0x00000EA8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C10_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C10_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C10_PMON_CTR0 is defined as MSR_C10_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C10_PMON_CTR0              0x00000EA8


/**
  Package. Uncore C-box 10 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C10_PMON_CTR1 (0x00000EA9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C10_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C10_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C10_PMON_CTR1 is defined as MSR_C10_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C10_PMON_CTR1              0x00000EA9


/**
  Package. Uncore C-box 10 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C10_PMON_CTR2 (0x00000EAA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C10_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C10_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C10_PMON_CTR2 is defined as MSR_C10_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C10_PMON_CTR2              0x00000EAA


/**
  Package. Uncore C-box 10 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C10_PMON_CTR3 (0x00000EAB)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C10_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C10_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C10_PMON_CTR3 is defined as MSR_C10_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C10_PMON_CTR3              0x00000EAB


/**
  Package. Uncore C-box 11 perfmon local box wide control.

  @param  ECX  MSR_HASWELL_E_C11_PMON_BOX_CTL (0x00000EB0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C11_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C11_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C11_PMON_BOX_CTL is defined as MSR_C11_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C11_PMON_BOX_CTL           0x00000EB0


/**
  Package. Uncore C-box 11 perfmon event select for C-box 11 counter 0.

  @param  ECX  MSR_HASWELL_E_C11_PMON_EVNTSEL0 (0x00000EB1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C11_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C11_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C11_PMON_EVNTSEL0 is defined as MSR_C11_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C11_PMON_EVNTSEL0          0x00000EB1


/**
  Package. Uncore C-box 11 perfmon event select for C-box 11 counter 1.

  @param  ECX  MSR_HASWELL_E_C11_PMON_EVNTSEL1 (0x00000EB2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C11_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C11_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C11_PMON_EVNTSEL1 is defined as MSR_C11_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C11_PMON_EVNTSEL1          0x00000EB2


/**
  Package. Uncore C-box 11 perfmon event select for C-box 11 counter 2.

  @param  ECX  MSR_HASWELL_E_C11_PMON_EVNTSEL2 (0x00000EB3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C11_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C11_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C11_PMON_EVNTSEL2 is defined as MSR_C11_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C11_PMON_EVNTSEL2          0x00000EB3


/**
  Package. Uncore C-box 11 perfmon event select for C-box 11 counter 3.

  @param  ECX  MSR_HASWELL_E_C11_PMON_EVNTSEL3 (0x00000EB4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C11_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C11_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C11_PMON_EVNTSEL3 is defined as MSR_C11_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C11_PMON_EVNTSEL3          0x00000EB4


/**
  Package. Uncore C-box 11 perfmon box wide filter0.

  @param  ECX  MSR_HASWELL_E_C11_PMON_BOX_FILTER0 (0x00000EB5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C11_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C11_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C11_PMON_BOX_FILTER0 is defined as MSR_C11_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C11_PMON_BOX_FILTER0       0x00000EB5


/**
  Package. Uncore C-box 11 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C11_PMON_BOX_FILTER1 (0x00000EB6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C11_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C11_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C11_PMON_BOX_FILTER1 is defined as MSR_C11_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C11_PMON_BOX_FILTER1       0x00000EB6


/**
  Package. Uncore C-box 11 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C11_PMON_BOX_STATUS (0x00000EB7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C11_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C11_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C11_PMON_BOX_STATUS is defined as MSR_C11_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C11_PMON_BOX_STATUS        0x00000EB7


/**
  Package. Uncore C-box 11 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C11_PMON_CTR0 (0x00000EB8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C11_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C11_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C11_PMON_CTR0 is defined as MSR_C11_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C11_PMON_CTR0              0x00000EB8


/**
  Package. Uncore C-box 11 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C11_PMON_CTR1 (0x00000EB9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C11_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C11_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C11_PMON_CTR1 is defined as MSR_C11_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C11_PMON_CTR1              0x00000EB9


/**
  Package. Uncore C-box 11 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C11_PMON_CTR2 (0x00000EBA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C11_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C11_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C11_PMON_CTR2 is defined as MSR_C11_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C11_PMON_CTR2              0x00000EBA


/**
  Package. Uncore C-box 11 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C11_PMON_CTR3 (0x00000EBB)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C11_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C11_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C11_PMON_CTR3 is defined as MSR_C11_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C11_PMON_CTR3              0x00000EBB


/**
  Package. Uncore C-box 12 perfmon local box wide control.

  @param  ECX  MSR_HASWELL_E_C12_PMON_BOX_CTL (0x00000EC0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C12_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C12_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C12_PMON_BOX_CTL is defined as MSR_C12_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C12_PMON_BOX_CTL           0x00000EC0


/**
  Package. Uncore C-box 12 perfmon event select for C-box 12 counter 0.

  @param  ECX  MSR_HASWELL_E_C12_PMON_EVNTSEL0 (0x00000EC1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C12_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C12_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C12_PMON_EVNTSEL0 is defined as MSR_C12_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C12_PMON_EVNTSEL0          0x00000EC1


/**
  Package. Uncore C-box 12 perfmon event select for C-box 12 counter 1.

  @param  ECX  MSR_HASWELL_E_C12_PMON_EVNTSEL1 (0x00000EC2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C12_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C12_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C12_PMON_EVNTSEL1 is defined as MSR_C12_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C12_PMON_EVNTSEL1          0x00000EC2


/**
  Package. Uncore C-box 12 perfmon event select for C-box 12 counter 2.

  @param  ECX  MSR_HASWELL_E_C12_PMON_EVNTSEL2 (0x00000EC3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C12_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C12_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C12_PMON_EVNTSEL2 is defined as MSR_C12_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C12_PMON_EVNTSEL2          0x00000EC3


/**
  Package. Uncore C-box 12 perfmon event select for C-box 12 counter 3.

  @param  ECX  MSR_HASWELL_E_C12_PMON_EVNTSEL3 (0x00000EC4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C12_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C12_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C12_PMON_EVNTSEL3 is defined as MSR_C12_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C12_PMON_EVNTSEL3          0x00000EC4


/**
  Package. Uncore C-box 12 perfmon box wide filter0.

  @param  ECX  MSR_HASWELL_E_C12_PMON_BOX_FILTER0 (0x00000EC5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C12_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C12_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C12_PMON_BOX_FILTER0 is defined as MSR_C12_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C12_PMON_BOX_FILTER0       0x00000EC5


/**
  Package. Uncore C-box 12 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C12_PMON_BOX_FILTER1 (0x00000EC6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C12_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C12_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C12_PMON_BOX_FILTER1 is defined as MSR_C12_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C12_PMON_BOX_FILTER1       0x00000EC6


/**
  Package. Uncore C-box 12 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C12_PMON_BOX_STATUS (0x00000EC7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C12_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C12_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C12_PMON_BOX_STATUS is defined as MSR_C12_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C12_PMON_BOX_STATUS        0x00000EC7


/**
  Package. Uncore C-box 12 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C12_PMON_CTR0 (0x00000EC8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C12_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C12_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C12_PMON_CTR0 is defined as MSR_C12_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C12_PMON_CTR0              0x00000EC8


/**
  Package. Uncore C-box 12 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C12_PMON_CTR1 (0x00000EC9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C12_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C12_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C12_PMON_CTR1 is defined as MSR_C12_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C12_PMON_CTR1              0x00000EC9


/**
  Package. Uncore C-box 12 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C12_PMON_CTR2 (0x00000ECA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C12_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C12_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C12_PMON_CTR2 is defined as MSR_C12_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C12_PMON_CTR2              0x00000ECA


/**
  Package. Uncore C-box 12 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C12_PMON_CTR3 (0x00000ECB)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C12_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C12_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C12_PMON_CTR3 is defined as MSR_C12_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C12_PMON_CTR3              0x00000ECB


/**
  Package. Uncore C-box 13 perfmon local box wide control.

  @param  ECX  MSR_HASWELL_E_C13_PMON_BOX_CTL (0x00000ED0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C13_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C13_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C13_PMON_BOX_CTL is defined as MSR_C13_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C13_PMON_BOX_CTL           0x00000ED0


/**
  Package. Uncore C-box 13 perfmon event select for C-box 13 counter 0.

  @param  ECX  MSR_HASWELL_E_C13_PMON_EVNTSEL0 (0x00000ED1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C13_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C13_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C13_PMON_EVNTSEL0 is defined as MSR_C13_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C13_PMON_EVNTSEL0          0x00000ED1


/**
  Package. Uncore C-box 13 perfmon event select for C-box 13 counter 1.

  @param  ECX  MSR_HASWELL_E_C13_PMON_EVNTSEL1 (0x00000ED2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C13_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C13_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C13_PMON_EVNTSEL1 is defined as MSR_C13_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C13_PMON_EVNTSEL1          0x00000ED2


/**
  Package. Uncore C-box 13 perfmon event select for C-box 13 counter 2.

  @param  ECX  MSR_HASWELL_E_C13_PMON_EVNTSEL2 (0x00000ED3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C13_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C13_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C13_PMON_EVNTSEL2 is defined as MSR_C13_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C13_PMON_EVNTSEL2          0x00000ED3


/**
  Package. Uncore C-box 13 perfmon event select for C-box 13 counter 3.

  @param  ECX  MSR_HASWELL_E_C13_PMON_EVNTSEL3 (0x00000ED4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C13_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C13_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C13_PMON_EVNTSEL3 is defined as MSR_C13_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C13_PMON_EVNTSEL3          0x00000ED4


/**
  Package. Uncore C-box 13 perfmon box wide filter0.

  @param  ECX  MSR_HASWELL_E_C13_PMON_BOX_FILTER0 (0x00000ED5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C13_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C13_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C13_PMON_BOX_FILTER0 is defined as MSR_C13_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C13_PMON_BOX_FILTER0       0x00000ED5


/**
  Package. Uncore C-box 13 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C13_PMON_BOX_FILTER1 (0x00000ED6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C13_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C13_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C13_PMON_BOX_FILTER1 is defined as MSR_C13_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C13_PMON_BOX_FILTER1       0x00000ED6


/**
  Package. Uncore C-box 13 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C13_PMON_BOX_STATUS (0x00000ED7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C13_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C13_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C13_PMON_BOX_STATUS is defined as MSR_C13_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C13_PMON_BOX_STATUS        0x00000ED7


/**
  Package. Uncore C-box 13 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C13_PMON_CTR0 (0x00000ED8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C13_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C13_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C13_PMON_CTR0 is defined as MSR_C13_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C13_PMON_CTR0              0x00000ED8


/**
  Package. Uncore C-box 13 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C13_PMON_CTR1 (0x00000ED9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C13_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C13_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C13_PMON_CTR1 is defined as MSR_C13_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C13_PMON_CTR1              0x00000ED9


/**
  Package. Uncore C-box 13 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C13_PMON_CTR2 (0x00000EDA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C13_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C13_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C13_PMON_CTR2 is defined as MSR_C13_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C13_PMON_CTR2              0x00000EDA


/**
  Package. Uncore C-box 13 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C13_PMON_CTR3 (0x00000EDB)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C13_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C13_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C13_PMON_CTR3 is defined as MSR_C13_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C13_PMON_CTR3              0x00000EDB


/**
  Package. Uncore C-box 14 perfmon local box wide control.

  @param  ECX  MSR_HASWELL_E_C14_PMON_BOX_CTL (0x00000EE0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C14_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C14_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C14_PMON_BOX_CTL is defined as MSR_C14_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C14_PMON_BOX_CTL           0x00000EE0


/**
  Package. Uncore C-box 14 perfmon event select for C-box 14 counter 0.

  @param  ECX  MSR_HASWELL_E_C14_PMON_EVNTSEL0 (0x00000EE1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C14_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C14_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C14_PMON_EVNTSEL0 is defined as MSR_C14_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C14_PMON_EVNTSEL0          0x00000EE1


/**
  Package. Uncore C-box 14 perfmon event select for C-box 14 counter 1.

  @param  ECX  MSR_HASWELL_E_C14_PMON_EVNTSEL1 (0x00000EE2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C14_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C14_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C14_PMON_EVNTSEL1 is defined as MSR_C14_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C14_PMON_EVNTSEL1          0x00000EE2


/**
  Package. Uncore C-box 14 perfmon event select for C-box 14 counter 2.

  @param  ECX  MSR_HASWELL_E_C14_PMON_EVNTSEL2 (0x00000EE3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C14_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C14_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C14_PMON_EVNTSEL2 is defined as MSR_C14_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C14_PMON_EVNTSEL2          0x00000EE3


/**
  Package. Uncore C-box 14 perfmon event select for C-box 14 counter 3.

  @param  ECX  MSR_HASWELL_E_C14_PMON_EVNTSEL3 (0x00000EE4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C14_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C14_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C14_PMON_EVNTSEL3 is defined as MSR_C14_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C14_PMON_EVNTSEL3          0x00000EE4


/**
  Package. Uncore C-box 14 perfmon box wide filter0.

  @param  ECX  MSR_HASWELL_E_C14_PMON_BOX_FILTER (0x00000EE5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C14_PMON_BOX_FILTER);
  AsmWriteMsr64 (MSR_HASWELL_E_C14_PMON_BOX_FILTER, Msr);
  @endcode
  @note MSR_HASWELL_E_C14_PMON_BOX_FILTER is defined as MSR_C14_PMON_BOX_FILTER in SDM.
**/
#define MSR_HASWELL_E_C14_PMON_BOX_FILTER        0x00000EE5


/**
  Package. Uncore C-box 14 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C14_PMON_BOX_FILTER1 (0x00000EE6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C14_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C14_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C14_PMON_BOX_FILTER1 is defined as MSR_C14_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C14_PMON_BOX_FILTER1       0x00000EE6


/**
  Package. Uncore C-box 14 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C14_PMON_BOX_STATUS (0x00000EE7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C14_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C14_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C14_PMON_BOX_STATUS is defined as MSR_C14_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C14_PMON_BOX_STATUS        0x00000EE7


/**
  Package. Uncore C-box 14 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C14_PMON_CTR0 (0x00000EE8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C14_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C14_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C14_PMON_CTR0 is defined as MSR_C14_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C14_PMON_CTR0              0x00000EE8


/**
  Package. Uncore C-box 14 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C14_PMON_CTR1 (0x00000EE9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C14_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C14_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C14_PMON_CTR1 is defined as MSR_C14_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C14_PMON_CTR1              0x00000EE9


/**
  Package. Uncore C-box 14 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C14_PMON_CTR2 (0x00000EEA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C14_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C14_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C14_PMON_CTR2 is defined as MSR_C14_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C14_PMON_CTR2              0x00000EEA


/**
  Package. Uncore C-box 14 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C14_PMON_CTR3 (0x00000EEB)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C14_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C14_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C14_PMON_CTR3 is defined as MSR_C14_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C14_PMON_CTR3              0x00000EEB


/**
  Package. Uncore C-box 15 perfmon local box wide control.

  @param  ECX  MSR_HASWELL_E_C15_PMON_BOX_CTL (0x00000EF0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C15_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C15_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C15_PMON_BOX_CTL is defined as MSR_C15_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C15_PMON_BOX_CTL           0x00000EF0


/**
  Package. Uncore C-box 15 perfmon event select for C-box 15 counter 0.

  @param  ECX  MSR_HASWELL_E_C15_PMON_EVNTSEL0 (0x00000EF1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C15_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C15_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C15_PMON_EVNTSEL0 is defined as MSR_C15_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C15_PMON_EVNTSEL0          0x00000EF1


/**
  Package. Uncore C-box 15 perfmon event select for C-box 15 counter 1.

  @param  ECX  MSR_HASWELL_E_C15_PMON_EVNTSEL1 (0x00000EF2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C15_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C15_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C15_PMON_EVNTSEL1 is defined as MSR_C15_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C15_PMON_EVNTSEL1          0x00000EF2


/**
  Package. Uncore C-box 15 perfmon event select for C-box 15 counter 2.

  @param  ECX  MSR_HASWELL_E_C15_PMON_EVNTSEL2 (0x00000EF3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C15_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C15_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C15_PMON_EVNTSEL2 is defined as MSR_C15_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C15_PMON_EVNTSEL2          0x00000EF3


/**
  Package. Uncore C-box 15 perfmon event select for C-box 15 counter 3.

  @param  ECX  MSR_HASWELL_E_C15_PMON_EVNTSEL3 (0x00000EF4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C15_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C15_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C15_PMON_EVNTSEL3 is defined as MSR_C15_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C15_PMON_EVNTSEL3          0x00000EF4


/**
  Package. Uncore C-box 15 perfmon box wide filter0.

  @param  ECX  MSR_HASWELL_E_C15_PMON_BOX_FILTER0 (0x00000EF5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C15_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C15_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C15_PMON_BOX_FILTER0 is defined as MSR_C15_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C15_PMON_BOX_FILTER0       0x00000EF5


/**
  Package. Uncore C-box 15 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C15_PMON_BOX_FILTER1 (0x00000EF6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C15_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C15_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C15_PMON_BOX_FILTER1 is defined as MSR_C15_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C15_PMON_BOX_FILTER1       0x00000EF6


/**
  Package. Uncore C-box 15 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C15_PMON_BOX_STATUS (0x00000EF7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C15_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C15_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C15_PMON_BOX_STATUS is defined as MSR_C15_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C15_PMON_BOX_STATUS        0x00000EF7


/**
  Package. Uncore C-box 15 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C15_PMON_CTR0 (0x00000EF8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C15_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C15_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C15_PMON_CTR0 is defined as MSR_C15_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C15_PMON_CTR0              0x00000EF8


/**
  Package. Uncore C-box 15 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C15_PMON_CTR1 (0x00000EF9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C15_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C15_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C15_PMON_CTR1 is defined as MSR_C15_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C15_PMON_CTR1              0x00000EF9


/**
  Package. Uncore C-box 15 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C15_PMON_CTR2 (0x00000EFA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C15_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C15_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C15_PMON_CTR2 is defined as MSR_C15_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C15_PMON_CTR2              0x00000EFA


/**
  Package. Uncore C-box 15 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C15_PMON_CTR3 (0x00000EFB)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C15_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C15_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C15_PMON_CTR3 is defined as MSR_C15_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C15_PMON_CTR3              0x00000EFB


/**
  Package. Uncore C-box 16 perfmon for box-wide control.

  @param  ECX  MSR_HASWELL_E_C16_PMON_BOX_CTL (0x00000F00)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C16_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C16_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C16_PMON_BOX_CTL is defined as MSR_C16_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C16_PMON_BOX_CTL           0x00000F00


/**
  Package. Uncore C-box 16 perfmon event select for C-box 16 counter 0.

  @param  ECX  MSR_HASWELL_E_C16_PMON_EVNTSEL0 (0x00000F01)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C16_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C16_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C16_PMON_EVNTSEL0 is defined as MSR_C16_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C16_PMON_EVNTSEL0          0x00000F01


/**
  Package. Uncore C-box 16 perfmon event select for C-box 16 counter 1.

  @param  ECX  MSR_HASWELL_E_C16_PMON_EVNTSEL1 (0x00000F02)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C16_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C16_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C16_PMON_EVNTSEL1 is defined as MSR_C16_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C16_PMON_EVNTSEL1          0x00000F02


/**
  Package. Uncore C-box 16 perfmon event select for C-box 16 counter 2.

  @param  ECX  MSR_HASWELL_E_C16_PMON_EVNTSEL2 (0x00000F03)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C16_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C16_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C16_PMON_EVNTSEL2 is defined as MSR_C16_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C16_PMON_EVNTSEL2          0x00000F03


/**
  Package. Uncore C-box 16 perfmon event select for C-box 16 counter 3.

  @param  ECX  MSR_HASWELL_E_C16_PMON_EVNTSEL3 (0x00000F04)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C16_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C16_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C16_PMON_EVNTSEL3 is defined as MSR_C16_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C16_PMON_EVNTSEL3          0x00000F04


/**
  Package. Uncore C-box 16 perfmon box wide filter 0.

  @param  ECX  MSR_HASWELL_E_C16_PMON_BOX_FILTER0 (0x00000F05)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C16_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C16_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C16_PMON_BOX_FILTER0 is defined as MSR_C16_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C16_PMON_BOX_FILTER0       0x00000F05


/**
  Package. Uncore C-box 16 perfmon box wide filter 1.

  @param  ECX  MSR_HASWELL_E_C16_PMON_BOX_FILTER1 (0x00000F06)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C16_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C16_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C16_PMON_BOX_FILTER1 is defined as MSR_C16_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C16_PMON_BOX_FILTER1       0x00000F06


/**
  Package. Uncore C-box 16 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C16_PMON_BOX_STATUS (0x00000F07)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C16_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C16_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C16_PMON_BOX_STATUS is defined as MSR_C16_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C16_PMON_BOX_STATUS        0x00000F07


/**
  Package. Uncore C-box 16 perfmon counter 0.

  @param  ECX  MSR_HASWELL_E_C16_PMON_CTR0 (0x00000F08)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C16_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C16_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C16_PMON_CTR0 is defined as MSR_C16_PMON_CTR0 in SDM.
**/
#define MSR_HASWELL_E_C16_PMON_CTR0              0x00000F08


/**
  Package. Uncore C-box 16 perfmon counter 1.

  @param  ECX  MSR_HASWELL_E_C16_PMON_CTR1 (0x00000F09)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C16_PMON_CTR1);
  AsmWriteMsr64 (MSR_HASWELL_E_C16_PMON_CTR1, Msr);
  @endcode
  @note MSR_HASWELL_E_C16_PMON_CTR1 is defined as MSR_C16_PMON_CTR1 in SDM.
**/
#define MSR_HASWELL_E_C16_PMON_CTR1              0x00000F09


/**
  Package. Uncore C-box 16 perfmon counter 2.

  @param  ECX  MSR_HASWELL_E_C16_PMON_CTR2 (0x00000F0A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C16_PMON_CTR2);
  AsmWriteMsr64 (MSR_HASWELL_E_C16_PMON_CTR2, Msr);
  @endcode
  @note MSR_HASWELL_E_C16_PMON_CTR2 is defined as MSR_C16_PMON_CTR2 in SDM.
**/
#define MSR_HASWELL_E_C16_PMON_CTR2              0x00000F0A


/**
  Package. Uncore C-box 16 perfmon counter 3.

  @param  ECX  MSR_HASWELL_E_C16_PMON_CTR3 (0x00000E0B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C16_PMON_CTR3);
  AsmWriteMsr64 (MSR_HASWELL_E_C16_PMON_CTR3, Msr);
  @endcode
  @note MSR_HASWELL_E_C16_PMON_CTR3 is defined as MSR_C16_PMON_CTR3 in SDM.
**/
#define MSR_HASWELL_E_C16_PMON_CTR3              0x00000E0B


/**
  Package. Uncore C-box 17 perfmon for box-wide control.

  @param  ECX  MSR_HASWELL_E_C17_PMON_BOX_CTL (0x00000F10)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C17_PMON_BOX_CTL);
  AsmWriteMsr64 (MSR_HASWELL_E_C17_PMON_BOX_CTL, Msr);
  @endcode
  @note MSR_HASWELL_E_C17_PMON_BOX_CTL is defined as MSR_C17_PMON_BOX_CTL in SDM.
**/
#define MSR_HASWELL_E_C17_PMON_BOX_CTL           0x00000F10


/**
  Package. Uncore C-box 17 perfmon event select for C-box 17 counter 0.

  @param  ECX  MSR_HASWELL_E_C17_PMON_EVNTSEL0 (0x00000F11)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C17_PMON_EVNTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_E_C17_PMON_EVNTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_E_C17_PMON_EVNTSEL0 is defined as MSR_C17_PMON_EVNTSEL0 in SDM.
**/
#define MSR_HASWELL_E_C17_PMON_EVNTSEL0          0x00000F11


/**
  Package. Uncore C-box 17 perfmon event select for C-box 17 counter 1.

  @param  ECX  MSR_HASWELL_E_C17_PMON_EVNTSEL1 (0x00000F12)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C17_PMON_EVNTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_E_C17_PMON_EVNTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_E_C17_PMON_EVNTSEL1 is defined as MSR_C17_PMON_EVNTSEL1 in SDM.
**/
#define MSR_HASWELL_E_C17_PMON_EVNTSEL1          0x00000F12


/**
  Package. Uncore C-box 17 perfmon event select for C-box 17 counter 2.

  @param  ECX  MSR_HASWELL_E_C17_PMON_EVNTSEL2 (0x00000F13)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C17_PMON_EVNTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_E_C17_PMON_EVNTSEL2, Msr);
  @endcode
  @note MSR_HASWELL_E_C17_PMON_EVNTSEL2 is defined as MSR_C17_PMON_EVNTSEL2 in SDM.
**/
#define MSR_HASWELL_E_C17_PMON_EVNTSEL2          0x00000F13


/**
  Package. Uncore C-box 17 perfmon event select for C-box 17 counter 3.

  @param  ECX  MSR_HASWELL_E_C17_PMON_EVNTSEL3 (0x00000F14)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C17_PMON_EVNTSEL3);
  AsmWriteMsr64 (MSR_HASWELL_E_C17_PMON_EVNTSEL3, Msr);
  @endcode
  @note MSR_HASWELL_E_C17_PMON_EVNTSEL3 is defined as MSR_C17_PMON_EVNTSEL3 in SDM.
**/
#define MSR_HASWELL_E_C17_PMON_EVNTSEL3          0x00000F14


/**
  Package. Uncore C-box 17 perfmon box wide filter 0.

  @param  ECX  MSR_HASWELL_E_C17_PMON_BOX_FILTER0 (0x00000F15)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C17_PMON_BOX_FILTER0);
  AsmWriteMsr64 (MSR_HASWELL_E_C17_PMON_BOX_FILTER0, Msr);
  @endcode
  @note MSR_HASWELL_E_C17_PMON_BOX_FILTER0 is defined as MSR_C17_PMON_BOX_FILTER0 in SDM.
**/
#define MSR_HASWELL_E_C17_PMON_BOX_FILTER0       0x00000F15


/**
  Package. Uncore C-box 17 perfmon box wide filter1.

  @param  ECX  MSR_HASWELL_E_C17_PMON_BOX_FILTER1 (0x00000F16)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C17_PMON_BOX_FILTER1);
  AsmWriteMsr64 (MSR_HASWELL_E_C17_PMON_BOX_FILTER1, Msr);
  @endcode
  @note MSR_HASWELL_E_C17_PMON_BOX_FILTER1 is defined as MSR_C17_PMON_BOX_FILTER1 in SDM.
**/
#define MSR_HASWELL_E_C17_PMON_BOX_FILTER1       0x00000F16

/**
  Package. Uncore C-box 17 perfmon box wide status.

  @param  ECX  MSR_HASWELL_E_C17_PMON_BOX_STATUS (0x00000F17)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C17_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_E_C17_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_HASWELL_E_C17_PMON_BOX_STATUS is defined as MSR_C17_PMON_BOX_STATUS in SDM.
**/
#define MSR_HASWELL_E_C17_PMON_BOX_STATUS        0x00000F17


/**
  Package. Uncore C-box 17 perfmon counter n.

  @param  ECX  MSR_HASWELL_E_C17_PMON_CTRn
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_E_C17_PMON_CTR0);
  AsmWriteMsr64 (MSR_HASWELL_E_C17_PMON_CTR0, Msr);
  @endcode
  @note MSR_HASWELL_E_C17_PMON_CTR0 is defined as MSR_C17_PMON_CTR0 in SDM.
        MSR_HASWELL_E_C17_PMON_CTR1 is defined as MSR_C17_PMON_CTR1 in SDM.
        MSR_HASWELL_E_C17_PMON_CTR2 is defined as MSR_C17_PMON_CTR2 in SDM.
        MSR_HASWELL_E_C17_PMON_CTR3 is defined as MSR_C17_PMON_CTR3 in SDM.
  @{
**/
#define MSR_HASWELL_E_C17_PMON_CTR0              0x00000F18
#define MSR_HASWELL_E_C17_PMON_CTR1              0x00000F19
#define MSR_HASWELL_E_C17_PMON_CTR2              0x00000F1A
#define MSR_HASWELL_E_C17_PMON_CTR3              0x00000F1B
/// @}

#endif
