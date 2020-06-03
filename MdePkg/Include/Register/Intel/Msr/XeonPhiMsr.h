/** @file
  MSR Definitions for Intel(R) Xeon(R) Phi(TM) processor Family.

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

#ifndef __XEON_PHI_MSR_H__
#define __XEON_PHI_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>

/**
  Is Intel(R) Xeon(R) Phi(TM) processor Family?

  @param   DisplayFamily  Display Family ID
  @param   DisplayModel   Display Model ID

  @retval  TRUE   Yes, it is.
  @retval  FALSE  No, it isn't.
**/
#define IS_XEON_PHI_PROCESSOR(DisplayFamily, DisplayModel) \
  (DisplayFamily == 0x06 && \
   (                        \
    DisplayModel == 0x57 || \
    DisplayModel == 0x85    \
    )                       \
   )

/**
  Thread. SMI Counter (R/O).

  @param  ECX  MSR_XEON_PHI_SMI_COUNT (0x00000034)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_SMI_COUNT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_SMI_COUNT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_XEON_PHI_SMI_COUNT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_XEON_PHI_SMI_COUNT);
  @endcode
  @note MSR_XEON_PHI_SMI_COUNT is defined as MSR_SMI_COUNT in SDM.
**/
#define MSR_XEON_PHI_SMI_COUNT                   0x00000034

/**
  MSR information returned for MSR index #MSR_XEON_PHI_SMI_COUNT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 31:0] SMI Count (R/O).
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
} MSR_XEON_PHI_SMI_COUNT_REGISTER;

/**
  Package. Protected Processor Inventory Number Enable Control (R/W).

  @param  ECX  MSR_XEON_PHI_PPIN_CTL (0x0000004E)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_PPIN_CTL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_PPIN_CTL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_XEON_PHI_PPIN_CTL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_XEON_PHI_PPIN_CTL);
  AsmWriteMsr64 (MSR_XEON_PHI_PPIN_CTL, Msr.Uint64);
  @endcode
**/
#define MSR_XEON_PHI_PPIN_CTL                    0x0000004E

/**
  MSR information returned for MSR index #MSR_XEON_PHI_PPIN_CTL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] LockOut (R/WO) Set 1 to prevent further writes to
    /// MSR_PPIN_CTL. Writing 1 to MSR_PPINCTL[bit 0] is permitted only if
    /// MSR_PPIN_CTL[bit 1] is clear. Default is 0. BIOS should provide an
    /// opt-in menu to enable the user to turn on MSR_PPIN_CTL[bit 1] for a
    /// privileged inventory initialization agent to access MSR_PPIN. After
    /// reading MSR_PPIN, the privileged inventory initialization agent should
    /// write '01b' to MSR_PPIN_CTL to disable further access to MSR_PPIN and
    /// prevent unauthorized modification to MSR_PPIN_CTL.
    ///
    UINT32  LockOut:1;
    ///
    /// [Bit 1] Enable_PPIN (R/W) If 1, enables MSR_PPIN to be accessible
    /// using RDMSR. Once set, an attempt to write 1 to MSR_PPIN_CTL[bit 0]
    /// will cause #GP. If 0, an attempt to read MSR_PPIN will cause #GP.
    /// Default is 0.
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
} MSR_XEON_PHI_PPIN_CTL_REGISTER;


/**
  Package. Protected Processor Inventory Number (R/O). Protected Processor
  Inventory Number (R/O) A unique value within a given CPUID
  family/model/stepping signature that a privileged inventory initialization
  agent can access to identify each physical processor, when access to
  MSR_PPIN is enabled. Access to MSR_PPIN is permitted only if
  MSR_PPIN_CTL[bits 1:0] = '10b'.

  @param  ECX  MSR_XEON_PHI_PPIN (0x0000004F)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_PPIN);
  @endcode
**/
#define MSR_XEON_PHI_PPIN                        0x0000004F

/**
  Package. Platform Information Contains power management and other model
  specific features enumeration. See http://biosbits.org.

  @param  ECX  MSR_XEON_PHI_PLATFORM_INFO (0x000000CE)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_PLATFORM_INFO_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_PLATFORM_INFO_REGISTER.

  <b>Example usage</b>
  @code
  MSR_XEON_PHI_PLATFORM_INFO_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_XEON_PHI_PLATFORM_INFO);
  AsmWriteMsr64 (MSR_XEON_PHI_PLATFORM_INFO, Msr.Uint64);
  @endcode
  @note MSR_XEON_PHI_PLATFORM_INFO is defined as MSR_PLATFORM_INFO in SDM.
**/
#define MSR_XEON_PHI_PLATFORM_INFO               0x000000CE

/**
  MSR information returned for MSR index #MSR_XEON_PHI_PLATFORM_INFO
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
} MSR_XEON_PHI_PLATFORM_INFO_REGISTER;


/**
  Module. C-State Configuration Control (R/W).

  @param  ECX  MSR_XEON_PHI_PKG_CST_CONFIG_CONTROL (0x000000E2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_PKG_CST_CONFIG_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_PKG_CST_CONFIG_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_XEON_PHI_PKG_CST_CONFIG_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_XEON_PHI_PKG_CST_CONFIG_CONTROL);
  AsmWriteMsr64 (MSR_XEON_PHI_PKG_CST_CONFIG_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_XEON_PHI_PKG_CST_CONFIG_CONTROL is defined as MSR_PKG_CST_CONFIG_CONTROL in SDM.
**/
#define MSR_XEON_PHI_PKG_CST_CONFIG_CONTROL      0x000000E2

/**
  MSR information returned for MSR index #MSR_XEON_PHI_PKG_CST_CONFIG_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 2:0] Package C-State Limit (R/W)  The following C-state code
    /// name encodings are supported: 000b: C0/C1 001b: C2 010b: C6 No
    /// Retention 011b: C6 Retention 111b: No limit.
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
    UINT32  Reserved5:10;
    ///
    /// [Bit 26] C1 State Auto Demotion Enable (R/W) When set, the processor
    /// will conditionally demote C3/C6/C7 requests to C1 based on uncore
    /// auto-demote information.
    ///
    UINT32  C1StateAutoDemotionEnable:1;
    UINT32  Reserved6:1;
    ///
    /// [Bit 28] C1 State Auto Undemotion Enable (R/W) When set, enables
    /// Undemotion from Demoted C1.
    ///
    UINT32  C1StateAutoUndemotionEnable:1;
    ///
    /// [Bit 29] PKG C-State Auto Demotion Enable (R/W) When set, enables
    /// Package C state demotion.
    ///
    UINT32  PKGC_StateAutoDemotionEnable:1;
    UINT32  Reserved7:2;
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
} MSR_XEON_PHI_PKG_CST_CONFIG_CONTROL_REGISTER;


/**
  Module. Power Management IO Redirection in C-state (R/W).

  @param  ECX  MSR_XEON_PHI_PMG_IO_CAPTURE_BASE (0x000000E4)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_PMG_IO_CAPTURE_BASE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_PMG_IO_CAPTURE_BASE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_XEON_PHI_PMG_IO_CAPTURE_BASE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_XEON_PHI_PMG_IO_CAPTURE_BASE);
  AsmWriteMsr64 (MSR_XEON_PHI_PMG_IO_CAPTURE_BASE, Msr.Uint64);
  @endcode
  @note MSR_XEON_PHI_PMG_IO_CAPTURE_BASE is defined as MSR_PMG_IO_CAPTURE_BASE in SDM.
**/
#define MSR_XEON_PHI_PMG_IO_CAPTURE_BASE         0x000000E4

/**
  MSR information returned for MSR index #MSR_XEON_PHI_PMG_IO_CAPTURE_BASE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 15:0] LVL_2 Base Address (R/W).
    ///
    UINT32  Lvl2Base:16;
    ///
    /// [Bits 22:16] C-State Range (R/W) The IO-port block size in which
    /// IO-redirection will be executed (0-127). Should be programmed based on
    /// the number of LVLx registers existing in the chipset.
    ///
    UINT32  CStateRange:7;
    UINT32  Reserved3:9;
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
} MSR_XEON_PHI_PMG_IO_CAPTURE_BASE_REGISTER;


/**
  Core. AES Configuration (RW-L) Privileged post-BIOS agent must provide a #GP
  handler to handle unsuccessful read of this MSR.

  @param  ECX  MSR_XEON_PHI_FEATURE_CONFIG (0x0000013C)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_FEATURE_CONFIG_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_FEATURE_CONFIG_REGISTER.

  <b>Example usage</b>
  @code
  MSR_XEON_PHI_FEATURE_CONFIG_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_XEON_PHI_FEATURE_CONFIG);
  AsmWriteMsr64 (MSR_XEON_PHI_FEATURE_CONFIG, Msr.Uint64);
  @endcode
  @note MSR_XEON_PHI_FEATURE_CONFIG is defined as MSR_FEATURE_CONFIG in SDM.
**/
#define MSR_XEON_PHI_FEATURE_CONFIG              0x0000013C

/**
  MSR information returned for MSR index #MSR_XEON_PHI_FEATURE_CONFIG
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
} MSR_XEON_PHI_FEATURE_CONFIG_REGISTER;


/**
  Thread. MISC_FEATURE_ENABLES.

  @param  ECX  MSR_XEON_PHI_MISC_FEATURE_ENABLES (0x00000140)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_MISC_FEATURE_ENABLES_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_MISC_FEATURE_ENABLES_REGISTER.

  <b>Example usage</b>
  @code
  MSR_XEON_PHI_MISC_FEATURE_ENABLES_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_XEON_PHI_MISC_FEATURE_ENABLES);
  AsmWriteMsr64 (MSR_XEON_PHI_MISC_FEATURE_ENABLES, Msr.Uint64);
  @endcode
**/
#define MSR_XEON_PHI_MISC_FEATURE_ENABLES        0x00000140

/**
  MSR information returned for MSR index #MSR_XEON_PHI_MISC_FEATURE_ENABLES
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:1;
    ///
    /// [Bit 1] User Mode MONITOR and MWAIT (R/W) If set to 1, the MONITOR and
    /// MWAIT instructions do not cause invalid-opcode exceptions when
    /// executed with CPL > 0 or in virtual-8086 mode. If MWAIT is executed
    /// when CPL > 0 or in virtual-8086 mode, and if EAX indicates a C-state
    /// other than C0 or C1, the instruction operates as if EAX indicated the
    /// C-state C1.
    ///
    UINT32  UserModeMonitorAndMwait:1;
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
} MSR_XEON_PHI_MISC_FEATURE_ENABLES_REGISTER;

/**
  THREAD. Enhanced SMM Capabilities (SMM-RO) Reports SMM capability
  Enhancement. Accessible only while in SMM.

  @param  ECX  MSR_XEON_PHI_SMM_MCA_CAP (0x0000017D)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_SMM_MCA_CAP_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_SMM_MCA_CAP_REGISTER.

  <b>Example usage</b>
  @code
  MSR_XEON_PHI_SMM_MCA_CAP_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_XEON_PHI_SMM_MCA_CAP);
  AsmWriteMsr64 (MSR_XEON_PHI_SMM_MCA_CAP, Msr.Uint64);
  @endcode
  @note MSR_XEON_PHI_SMM_MCA_CAP is defined as MSR_SMM_MCA_CAP in SDM.
**/
#define MSR_XEON_PHI_SMM_MCA_CAP                 0x0000017D

/**
  MSR information returned for MSR index #MSR_XEON_PHI_SMM_MCA_CAP
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 31:0] Bank Support (SMM-RO) One bit per MCA bank. If the bit is
    /// set, that bank supports Enhanced MCA (Default all 0; does not support
    /// EMCA).
    ///
    UINT32  BankSupport:32;
    UINT32  Reserved4:24;
    ///
    /// [Bit 56] Targeted SMI (SMM-RO) Set if targeted SMI is supported.
    ///
    UINT32  TargetedSMI:1;
    ///
    /// [Bit 57] SMM_CPU_SVRSTR (SMM-RO) Set if SMM SRAM save/restore feature
    /// is supported.
    ///
    UINT32  SMM_CPU_SVRSTR:1;
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
} MSR_XEON_PHI_SMM_MCA_CAP_REGISTER;


/**
  Thread. Enable Misc. Processor Features (R/W)  Allows a variety of processor
  functions to be enabled and disabled.

  @param  ECX  MSR_XEON_PHI_IA32_MISC_ENABLE (0x000001A0)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_IA32_MISC_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_IA32_MISC_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_XEON_PHI_IA32_MISC_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_XEON_PHI_IA32_MISC_ENABLE);
  AsmWriteMsr64 (MSR_XEON_PHI_IA32_MISC_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_XEON_PHI_IA32_MISC_ENABLE is defined as IA32_MISC_ENABLE in SDM.
**/
#define MSR_XEON_PHI_IA32_MISC_ENABLE            0x000001A0

/**
  MSR information returned for MSR index #MSR_XEON_PHI_IA32_MISC_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Fast-Strings Enable.
    ///
    UINT32  FastStrings:1;
    UINT32  Reserved1:2;
    ///
    /// [Bit 3] Automatic Thermal Control Circuit Enable (R/W) Default value
    /// is 1.
    ///
    UINT32  AutomaticThermalControlCircuit:1;
    UINT32  Reserved2:3;
    ///
    /// [Bit 7] Performance Monitoring Available (R).
    ///
    UINT32  PerformanceMonitoring:1;
    UINT32  Reserved3:3;
    ///
    /// [Bit 11] Branch Trace Storage Unavailable (RO).
    ///
    UINT32  BTS:1;
    ///
    /// [Bit 12] Processor Event Based Sampling Unavailable (RO).
    ///
    UINT32  PEBS:1;
    UINT32  Reserved4:3;
    ///
    /// [Bit 16] Enhanced Intel SpeedStep Technology Enable (R/W).
    ///
    UINT32  EIST:1;
    UINT32  Reserved5:1;
    ///
    /// [Bit 18] ENABLE MONITOR FSM (R/W).
    ///
    UINT32  MONITOR:1;
    UINT32  Reserved6:3;
    ///
    /// [Bit 22] Limit CPUID Maxval (R/W).
    ///
    UINT32  LimitCpuidMaxval:1;
    ///
    /// [Bit 23] xTPR Message Disable (R/W).
    ///
    UINT32  xTPR_Message_Disable:1;
    UINT32  Reserved7:8;
    UINT32  Reserved8:2;
    ///
    /// [Bit 34] XD Bit Disable (R/W).
    ///
    UINT32  XD:1;
    UINT32  Reserved9:3;
    ///
    /// [Bit 38] Turbo Mode Disable (R/W).
    ///
    UINT32  TurboModeDisable:1;
    UINT32  Reserved10:25;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_XEON_PHI_IA32_MISC_ENABLE_REGISTER;


/**
  Package.

  @param  ECX  MSR_XEON_PHI_TEMPERATURE_TARGET (0x000001A2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_TEMPERATURE_TARGET_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_TEMPERATURE_TARGET_REGISTER.

  <b>Example usage</b>
  @code
  MSR_XEON_PHI_TEMPERATURE_TARGET_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_XEON_PHI_TEMPERATURE_TARGET);
  AsmWriteMsr64 (MSR_XEON_PHI_TEMPERATURE_TARGET, Msr.Uint64);
  @endcode
  @note MSR_XEON_PHI_TEMPERATURE_TARGET is defined as MSR_TEMPERATURE_TARGET in SDM.
**/
#define MSR_XEON_PHI_TEMPERATURE_TARGET          0x000001A2

/**
  MSR information returned for MSR index #MSR_XEON_PHI_TEMPERATURE_TARGET
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:16;
    ///
    /// [Bits 23:16] Temperature Target (R).
    ///
    UINT32  TemperatureTarget:8;
    ///
    /// [Bits 29:24] Target Offset (R/W).
    ///
    UINT32  TargetOffset:6;
    UINT32  Reserved2:2;
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
} MSR_XEON_PHI_TEMPERATURE_TARGET_REGISTER;


/**
  Miscellaneous Feature Control (R/W).

  @param  ECX  MSR_XEON_PHI_MISC_FEATURE_CONTROL (0x000001A4)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_MISC_FEATURE_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_MISC_FEATURE_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_XEON_PHI_MISC_FEATURE_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_XEON_PHI_MISC_FEATURE_CONTROL);
  AsmWriteMsr64 (MSR_XEON_PHI_MISC_FEATURE_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_XEON_PHI_MISC_FEATURE_CONTROL is defined as MSR_MISC_FEATURE_CONTROL in SDM.
**/
#define MSR_XEON_PHI_MISC_FEATURE_CONTROL        0x000001A4

/**
  MSR information returned for MSR index #MSR_XEON_PHI_MISC_FEATURE_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Core. DCU Hardware Prefetcher Disable (R/W) If 1, disables the
    /// L1 data cache prefetcher.
    ///
    UINT32  DCUHardwarePrefetcherDisable:1;
    ///
    /// [Bit 1] Core. L2 Hardware Prefetcher Disable (R/W)  If 1, disables the
    /// L2 hardware prefetcher.
    ///
    UINT32  L2HardwarePrefetcherDisable:1;
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
} MSR_XEON_PHI_MISC_FEATURE_CONTROL_REGISTER;


/**
  Shared. Offcore Response Event Select Register (R/W).

  @param  ECX  MSR_XEON_PHI_OFFCORE_RSP_0 (0x000001A6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_OFFCORE_RSP_0);
  AsmWriteMsr64 (MSR_XEON_PHI_OFFCORE_RSP_0, Msr);
  @endcode
  @note MSR_XEON_PHI_OFFCORE_RSP_0 is defined as MSR_OFFCORE_RSP_0 in SDM.
**/
#define MSR_XEON_PHI_OFFCORE_RSP_0               0x000001A6


/**
  Shared. Offcore Response Event Select Register (R/W).

  @param  ECX  MSR_XEON_PHI_OFFCORE_RSP_1 (0x000001A7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_OFFCORE_RSP_1);
  AsmWriteMsr64 (MSR_XEON_PHI_OFFCORE_RSP_1, Msr);
  @endcode
  @note MSR_XEON_PHI_OFFCORE_RSP_1 is defined as MSR_OFFCORE_RSP_1 in SDM.
**/
#define MSR_XEON_PHI_OFFCORE_RSP_1               0x000001A7


/**
  Package. Maximum Ratio Limit of Turbo Mode for Groups of Cores (RW).

  @param  ECX  MSR_XEON_PHI_TURBO_RATIO_LIMIT (0x000001AD)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_TURBO_RATIO_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_TURBO_RATIO_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_XEON_PHI_TURBO_RATIO_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_XEON_PHI_TURBO_RATIO_LIMIT);
  AsmWriteMsr64 (MSR_XEON_PHI_TURBO_RATIO_LIMIT, Msr.Uint64);
  @endcode
  @note MSR_XEON_PHI_TURBO_RATIO_LIMIT is defined as MSR_TURBO_RATIO_LIMIT in SDM.
**/
#define MSR_XEON_PHI_TURBO_RATIO_LIMIT           0x000001AD

/**
  MSR information returned for MSR index #MSR_XEON_PHI_TURBO_RATIO_LIMIT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved:1;
    ///
    /// [Bits 7:1] Package. Maximum Number of Cores in Group 0 Number active
    /// processor cores which operates under the maximum ratio limit for group
    /// 0.
    ///
    UINT32  MaxCoresGroup0:7;
    ///
    /// [Bits 15:8] Package. Maximum Ratio Limit for Group 0 Maximum turbo
    /// ratio limit when the number of active cores are not more than the
    /// group 0 maximum core count.
    ///
    UINT32  MaxRatioLimitGroup0:8;
    ///
    /// [Bits 20:16] Package. Number of Incremental Cores Added to Group 1
    /// Group 1, which includes the specified number of additional cores plus
    /// the cores in group 0, operates under the group 1 turbo max ratio limit
    /// = "group 0 Max ratio limit" - "group ratio delta for group 1".
    ///
    UINT32  MaxIncrementalCoresGroup1:5;
    ///
    /// [Bits 23:21] Package. Group Ratio Delta for Group 1 An unsigned
    /// integer specifying the ratio decrement relative to the Max ratio limit
    /// to Group 0.
    ///
    UINT32  DeltaRatioGroup1:3;
    ///
    /// [Bits 28:24] Package. Number of Incremental Cores Added to Group 2
    /// Group 2, which includes the specified number of additional cores plus
    /// all the cores in group 1, operates under the group 2 turbo max ratio
    /// limit = "group 1 Max ratio limit" - "group ratio delta for group 2".
    ///
    UINT32  MaxIncrementalCoresGroup2:5;
    ///
    /// [Bits 31:29] Package. Group Ratio Delta for Group 2 An unsigned
    /// integer specifying the ratio decrement relative to the Max ratio limit
    /// for Group 1.
    ///
    UINT32  DeltaRatioGroup2:3;
    ///
    /// [Bits 36:32] Package. Number of Incremental Cores Added to Group 3
    /// Group 3, which includes the specified number of additional cores plus
    /// all the cores in group 2, operates under the group 3 turbo max ratio
    /// limit = "group 2 Max ratio limit" - "group ratio delta for group 3".
    ///
    UINT32  MaxIncrementalCoresGroup3:5;
    ///
    /// [Bits 39:37] Package. Group Ratio Delta for Group 3 An unsigned
    /// integer specifying the ratio decrement relative to the Max ratio limit
    /// for Group 2.
    ///
    UINT32  DeltaRatioGroup3:3;
    ///
    /// [Bits 44:40] Package. Number of Incremental Cores Added to Group 4
    /// Group 4, which includes the specified number of additional cores plus
    /// all the cores in group 3, operates under the group 4 turbo max ratio
    /// limit = "group 3 Max ratio limit" - "group ratio delta for group 4".
    ///
    UINT32  MaxIncrementalCoresGroup4:5;
    ///
    /// [Bits 47:45] Package. Group Ratio Delta for Group 4 An unsigned
    /// integer specifying the ratio decrement relative to the Max ratio limit
    /// for Group 3.
    ///
    UINT32  DeltaRatioGroup4:3;
    ///
    /// [Bits 52:48] Package. Number of Incremental Cores Added to Group 5
    /// Group 5, which includes the specified number of additional cores plus
    /// all the cores in group 4, operates under the group 5 turbo max ratio
    /// limit = "group 4 Max ratio limit" - "group ratio delta for group 5".
    ///
    UINT32  MaxIncrementalCoresGroup5:5;
    ///
    /// [Bits 55:53] Package. Group Ratio Delta for Group 5 An unsigned
    /// integer specifying the ratio decrement relative to the Max ratio limit
    /// for Group 4.
    ///
    UINT32  DeltaRatioGroup5:3;
    ///
    /// [Bits 60:56] Package. Number of Incremental Cores Added to Group 6
    /// Group 6, which includes the specified number of additional cores plus
    /// all the cores in group 5, operates under the group 6 turbo max ratio
    /// limit = "group 5 Max ratio limit" - "group ratio delta for group 6".
    ///
    UINT32  MaxIncrementalCoresGroup6:5;
    ///
    /// [Bits 63:61] Package. Group Ratio Delta for Group 6 An unsigned
    /// integer specifying the ratio decrement relative to the Max ratio limit
    /// for Group 5.
    ///
    UINT32  DeltaRatioGroup6:3;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_XEON_PHI_TURBO_RATIO_LIMIT_REGISTER;


/**
  Thread. Last Branch Record Filtering Select Register (R/W).

  @param  ECX  MSR_XEON_PHI_LBR_SELECT (0x000001C8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_LBR_SELECT);
  AsmWriteMsr64 (MSR_XEON_PHI_LBR_SELECT, Msr);
  @endcode
  @note MSR_XEON_PHI_LBR_SELECT is defined as MSR_LBR_SELECT in SDM.
**/
#define MSR_XEON_PHI_LBR_SELECT                  0x000001C8


/**
  MSR information returned for MSR index #MSR_XEON_PHI_LBR_SELECT
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
} MSR_XEON_PHI_LBR_SELECT_REGISTER;

/**
  Thread. Last Branch Record Stack TOS (R/W).

  @param  ECX  MSR_XEON_PHI_LASTBRANCH_TOS (0x000001C9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_LASTBRANCH_TOS);
  AsmWriteMsr64 (MSR_XEON_PHI_LASTBRANCH_TOS, Msr);
  @endcode
  @note MSR_XEON_PHI_LASTBRANCH_TOS is defined as MSR_LASTBRANCH_TOS in SDM.
**/
#define MSR_XEON_PHI_LASTBRANCH_TOS              0x000001C9


/**
  Thread. Last Exception Record From Linear IP (R).

  @param  ECX  MSR_XEON_PHI_LER_FROM_LIP (0x000001DD)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_LER_FROM_LIP);
  @endcode
  @note MSR_XEON_PHI_LER_FROM_LIP is defined as MSR_LER_FROM_LIP in SDM.
**/
#define MSR_XEON_PHI_LER_FROM_LIP                0x000001DD


/**
  Thread. Last Exception Record To Linear IP (R).

  @param  ECX  MSR_XEON_PHI_LER_TO_LIP (0x000001DE)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_LER_TO_LIP);
  @endcode
  @note MSR_XEON_PHI_LER_TO_LIP is defined as MSR_LER_TO_LIP in SDM.
**/
#define MSR_XEON_PHI_LER_TO_LIP                  0x000001DE


/**
  Thread. See Table 2-2.

  @param  ECX  MSR_XEON_PHI_PEBS_ENABLE (0x000003F1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_PEBS_ENABLE);
  AsmWriteMsr64 (MSR_XEON_PHI_PEBS_ENABLE, Msr);
  @endcode
  @note MSR_XEON_PHI_PEBS_ENABLE is defined as MSR_PEBS_ENABLE in SDM.
**/
#define MSR_XEON_PHI_PEBS_ENABLE                 0x000003F1


/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI C-States. Package C3
  Residency Counter. (R/O).

  @param  ECX  MSR_XEON_PHI_PKG_C3_RESIDENCY (0x000003F8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_PKG_C3_RESIDENCY);
  AsmWriteMsr64 (MSR_XEON_PHI_PKG_C3_RESIDENCY, Msr);
  @endcode
  @note MSR_XEON_PHI_PKG_C3_RESIDENCY is defined as MSR_PKG_C3_RESIDENCY in SDM.
**/
#define MSR_XEON_PHI_PKG_C3_RESIDENCY            0x000003F8


/**
  Package. Package C6 Residency Counter. (R/O).

  @param  ECX  MSR_XEON_PHI_PKG_C6_RESIDENCY (0x000003F9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_PKG_C6_RESIDENCY);
  AsmWriteMsr64 (MSR_XEON_PHI_PKG_C6_RESIDENCY, Msr);
  @endcode
  @note MSR_XEON_PHI_PKG_C6_RESIDENCY is defined as MSR_PKG_C6_RESIDENCY in SDM.
**/
#define MSR_XEON_PHI_PKG_C6_RESIDENCY            0x000003F9


/**
  Package. Package C7 Residency Counter. (R/O).

  @param  ECX  MSR_XEON_PHI_PKG_C7_RESIDENCY (0x000003FA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_PKG_C7_RESIDENCY);
  AsmWriteMsr64 (MSR_XEON_PHI_PKG_C7_RESIDENCY, Msr);
  @endcode
  @note MSR_XEON_PHI_PKG_C7_RESIDENCY is defined as MSR_PKG_C7_RESIDENCY in SDM.
**/
#define MSR_XEON_PHI_PKG_C7_RESIDENCY            0x000003FA


/**
  Module. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI C-States. Module C0
  Residency Counter. (R/O).

  @param  ECX  MSR_XEON_PHI_MC0_RESIDENCY (0x000003FC)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_MC0_RESIDENCY);
  AsmWriteMsr64 (MSR_XEON_PHI_MC0_RESIDENCY, Msr);
  @endcode
  @note MSR_XEON_PHI_MC0_RESIDENCY is defined as MSR_MC0_RESIDENCY in SDM.
**/
#define MSR_XEON_PHI_MC0_RESIDENCY               0x000003FC


/**
  Module. Module C6 Residency Counter. (R/O).

  @param  ECX  MSR_XEON_PHI_MC6_RESIDENCY (0x000003FD)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_MC6_RESIDENCY);
  AsmWriteMsr64 (MSR_XEON_PHI_MC6_RESIDENCY, Msr);
  @endcode
  @note MSR_XEON_PHI_MC6_RESIDENCY is defined as MSR_MC6_RESIDENCY in SDM.
**/
#define MSR_XEON_PHI_MC6_RESIDENCY               0x000003FD


/**
  Core. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI C-States. CORE C6
  Residency Counter. (R/O).

  @param  ECX  MSR_XEON_PHI_CORE_C6_RESIDENCY (0x000003FF)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_CORE_C6_RESIDENCY);
  AsmWriteMsr64 (MSR_XEON_PHI_CORE_C6_RESIDENCY, Msr);
  @endcode
  @note MSR_XEON_PHI_CORE_C6_RESIDENCY is defined as MSR_CORE_C6_RESIDENCY in SDM.
**/
#define MSR_XEON_PHI_CORE_C6_RESIDENCY           0x000003FF


/**
  Core. Capability Reporting Register of EPT and VPID (R/O)  See Table 2-2.

  @param  ECX  MSR_XEON_PHI_IA32_VMX_EPT_VPID_ENUM (0x0000048C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_IA32_VMX_EPT_VPID_ENUM);
  @endcode
  @note MSR_XEON_PHI_IA32_VMX_EPT_VPID_ENUM is defined as IA32_VMX_EPT_VPID_ENUM in SDM.
**/
#define MSR_XEON_PHI_IA32_VMX_EPT_VPID_ENUM      0x0000048C


/**
  Core. Capability Reporting Register of VM-Function Controls (R/O) See Table
  2-2.

  @param  ECX  MSR_XEON_PHI_IA32_VMX_FMFUNC (0x00000491)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_IA32_VMX_FMFUNC);
  @endcode
  @note MSR_XEON_PHI_IA32_VMX_FMFUNC is defined as IA32_VMX_FMFUNC in SDM.
**/
#define MSR_XEON_PHI_IA32_VMX_FMFUNC             0x00000491


/**
  Package. Unit Multipliers used in RAPL Interfaces (R/O).

  @param  ECX  MSR_XEON_PHI_RAPL_POWER_UNIT (0x00000606)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_RAPL_POWER_UNIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_RAPL_POWER_UNIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_XEON_PHI_RAPL_POWER_UNIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_XEON_PHI_RAPL_POWER_UNIT);
  @endcode
  @note MSR_XEON_PHI_RAPL_POWER_UNIT is defined as MSR_RAPL_POWER_UNIT in SDM.
**/
#define MSR_XEON_PHI_RAPL_POWER_UNIT             0x00000606

/**
  MSR information returned for MSR index #MSR_XEON_PHI_RAPL_POWER_UNIT
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
} MSR_XEON_PHI_RAPL_POWER_UNIT_REGISTER;


/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI C-States. Package C2
  Residency Counter. (R/O).

  @param  ECX  MSR_XEON_PHI_PKG_C2_RESIDENCY (0x0000060D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_PKG_C2_RESIDENCY);
  AsmWriteMsr64 (MSR_XEON_PHI_PKG_C2_RESIDENCY, Msr);
  @endcode
  @note MSR_XEON_PHI_PKG_C2_RESIDENCY is defined as MSR_PKG_C2_RESIDENCY in SDM.
**/
#define MSR_XEON_PHI_PKG_C2_RESIDENCY            0x0000060D


/**
  Package. PKG RAPL Power Limit Control (R/W) See Section 14.9.3, "Package
  RAPL Domain.".

  @param  ECX  MSR_XEON_PHI_PKG_POWER_LIMIT (0x00000610)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_PKG_POWER_LIMIT);
  AsmWriteMsr64 (MSR_XEON_PHI_PKG_POWER_LIMIT, Msr);
  @endcode
  @note MSR_XEON_PHI_PKG_POWER_LIMIT is defined as MSR_PKG_POWER_LIMIT in SDM.
**/
#define MSR_XEON_PHI_PKG_POWER_LIMIT             0x00000610


/**
  Package. PKG Energy Status (R/O) See Section 14.9.3, "Package RAPL Domain.".

  @param  ECX  MSR_XEON_PHI_PKG_ENERGY_STATUS (0x00000611)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_PKG_ENERGY_STATUS);
  @endcode
  @note MSR_XEON_PHI_PKG_ENERGY_STATUS is defined as MSR_PKG_ENERGY_STATUS in SDM.
**/
#define MSR_XEON_PHI_PKG_ENERGY_STATUS           0x00000611


/**
  Package. PKG Perf Status (R/O) See Section 14.9.3, "Package RAPL Domain.".

  @param  ECX  MSR_XEON_PHI_PKG_PERF_STATUS (0x00000613)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_PKG_PERF_STATUS);
  @endcode
  @note MSR_XEON_PHI_PKG_PERF_STATUS is defined as MSR_PKG_PERF_STATUS in SDM.
**/
#define MSR_XEON_PHI_PKG_PERF_STATUS             0x00000613


/**
  Package. PKG RAPL Parameters (R/W) See Section 14.9.3, "Package RAPL
  Domain.".

  @param  ECX  MSR_XEON_PHI_PKG_POWER_INFO (0x00000614)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_PKG_POWER_INFO);
  AsmWriteMsr64 (MSR_XEON_PHI_PKG_POWER_INFO, Msr);
  @endcode
  @note MSR_XEON_PHI_PKG_POWER_INFO is defined as MSR_PKG_POWER_INFO in SDM.
**/
#define MSR_XEON_PHI_PKG_POWER_INFO              0x00000614


/**
  Package. DRAM RAPL Power Limit Control (R/W)  See Section 14.9.5, "DRAM RAPL
  Domain.".

  @param  ECX  MSR_XEON_PHI_DRAM_POWER_LIMIT (0x00000618)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_DRAM_POWER_LIMIT);
  AsmWriteMsr64 (MSR_XEON_PHI_DRAM_POWER_LIMIT, Msr);
  @endcode
  @note MSR_XEON_PHI_DRAM_POWER_LIMIT is defined as MSR_DRAM_POWER_LIMIT in SDM.
**/
#define MSR_XEON_PHI_DRAM_POWER_LIMIT            0x00000618


/**
  Package. DRAM Energy Status (R/O)  See Section 14.9.5, "DRAM RAPL Domain.".

  @param  ECX  MSR_XEON_PHI_DRAM_ENERGY_STATUS (0x00000619)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_DRAM_ENERGY_STATUS);
  @endcode
  @note MSR_XEON_PHI_DRAM_ENERGY_STATUS is defined as MSR_DRAM_ENERGY_STATUS in SDM.
**/
#define MSR_XEON_PHI_DRAM_ENERGY_STATUS          0x00000619


/**
  Package. DRAM Performance Throttling Status (R/O) See Section 14.9.5, "DRAM
  RAPL Domain.".

  @param  ECX  MSR_XEON_PHI_DRAM_PERF_STATUS (0x0000061B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_DRAM_PERF_STATUS);
  @endcode
  @note MSR_XEON_PHI_DRAM_PERF_STATUS is defined as MSR_DRAM_PERF_STATUS in SDM.
**/
#define MSR_XEON_PHI_DRAM_PERF_STATUS            0x0000061B


/**
  Package. DRAM RAPL Parameters (R/W) See Section 14.9.5, "DRAM RAPL Domain.".

  @param  ECX  MSR_XEON_PHI_DRAM_POWER_INFO (0x0000061C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_DRAM_POWER_INFO);
  AsmWriteMsr64 (MSR_XEON_PHI_DRAM_POWER_INFO, Msr);
  @endcode
  @note MSR_XEON_PHI_DRAM_POWER_INFO is defined as MSR_DRAM_POWER_INFO in SDM.
**/
#define MSR_XEON_PHI_DRAM_POWER_INFO             0x0000061C


/**
  Package. Uncore Ratio Limit (R/W) Out of reset, the min_ratio and max_ratio
  fields represent the widest possible range of uncore frequencies. Writing to
  these fields allows software to control the minimum and the maximum
  frequency that hardware will select.

  @param  ECX  MSR_XEON_PHI_MSRUNCORE_RATIO_LIMIT (0x00000620)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_MSRUNCORE_RATIO_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_MSRUNCORE_RATIO_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_XEON_PHI_MSRUNCORE_RATIO_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_XEON_PHI_MSRUNCORE_RATIO_LIMIT);
  AsmWriteMsr64 (MSR_XEON_PHI_MSRUNCORE_RATIO_LIMIT, Msr.Uint64);
  @endcode
**/
#define MSR_XEON_PHI_MSRUNCORE_RATIO_LIMIT       0x00000620

/**
  MSR information returned for MSR index #MSR_XEON_PHI_MSRUNCORE_RATIO_LIMIT
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
} MSR_XEON_PHI_MSRUNCORE_RATIO_LIMIT_REGISTER;


/**
  Package. PP0 RAPL Power Limit Control (R/W) See Section 14.9.4, "PP0/PP1
  RAPL Domains.".

  @param  ECX  MSR_XEON_PHI_PP0_POWER_LIMIT (0x00000638)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_PP0_POWER_LIMIT);
  AsmWriteMsr64 (MSR_XEON_PHI_PP0_POWER_LIMIT, Msr);
  @endcode
  @note MSR_XEON_PHI_PP0_POWER_LIMIT is defined as MSR_PP0_POWER_LIMIT in SDM.
**/
#define MSR_XEON_PHI_PP0_POWER_LIMIT             0x00000638


/**
  Package. PP0 Energy Status (R/O)  See Section 14.9.4, "PP0/PP1 RAPL
  Domains.".

  @param  ECX  MSR_XEON_PHI_PP0_ENERGY_STATUS (0x00000639)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_PP0_ENERGY_STATUS);
  @endcode
  @note MSR_XEON_PHI_PP0_ENERGY_STATUS is defined as MSR_PP0_ENERGY_STATUS in SDM.
**/
#define MSR_XEON_PHI_PP0_ENERGY_STATUS           0x00000639


/**
  Package. Base TDP Ratio (R/O) See Table 2-24.

  @param  ECX  MSR_XEON_PHI_CONFIG_TDP_NOMINAL (0x00000648)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_CONFIG_TDP_NOMINAL);
  @endcode
  @note MSR_XEON_PHI_CONFIG_TDP_NOMINAL is defined as MSR_CONFIG_TDP_NOMINAL in SDM.
**/
#define MSR_XEON_PHI_CONFIG_TDP_NOMINAL          0x00000648


/**
  Package. ConfigTDP Level 1 ratio and power level (R/O) See Table 2-24.

  @param  ECX  MSR_XEON_PHI_CONFIG_TDP_LEVEL1 (0x00000649)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_CONFIG_TDP_LEVEL1);
  @endcode
  @note MSR_XEON_PHI_CONFIG_TDP_LEVEL1 is defined as MSR_CONFIG_TDP_LEVEL1 in SDM.
**/
#define MSR_XEON_PHI_CONFIG_TDP_LEVEL1           0x00000649


/**
  Package. ConfigTDP Level 2 ratio and power level (R/O) See Table 2-24.

  @param  ECX  MSR_XEON_PHI_CONFIG_TDP_LEVEL2 (0x0000064A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_CONFIG_TDP_LEVEL2);
  @endcode
  @note MSR_XEON_PHI_CONFIG_TDP_LEVEL2 is defined as MSR_CONFIG_TDP_LEVEL2 in SDM.
**/
#define MSR_XEON_PHI_CONFIG_TDP_LEVEL2           0x0000064A


/**
  Package. ConfigTDP Control (R/W) See Table 2-24.

  @param  ECX  MSR_XEON_PHI_CONFIG_TDP_CONTROL (0x0000064B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_CONFIG_TDP_CONTROL);
  AsmWriteMsr64 (MSR_XEON_PHI_CONFIG_TDP_CONTROL, Msr);
  @endcode
  @note MSR_XEON_PHI_CONFIG_TDP_CONTROL is defined as MSR_CONFIG_TDP_CONTROL in SDM.
**/
#define MSR_XEON_PHI_CONFIG_TDP_CONTROL          0x0000064B


/**
  Package. ConfigTDP Control (R/W) See Table 2-24.

  @param  ECX  MSR_XEON_PHI_TURBO_ACTIVATION_RATIO (0x0000064C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_XEON_PHI_TURBO_ACTIVATION_RATIO);
  AsmWriteMsr64 (MSR_XEON_PHI_TURBO_ACTIVATION_RATIO, Msr);
  @endcode
  @note MSR_XEON_PHI_TURBO_ACTIVATION_RATIO is defined as MSR_TURBO_ACTIVATION_RATIO in SDM.
**/
#define MSR_XEON_PHI_TURBO_ACTIVATION_RATIO      0x0000064C


/**
  Package. Indicator of Frequency Clipping in Processor Cores (R/W) (frequency
  refers to processor core frequency).

  @param  ECX  MSR_XEON_PHI_CORE_PERF_LIMIT_REASONS (0x00000690)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_CORE_PERF_LIMIT_REASONS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_XEON_PHI_CORE_PERF_LIMIT_REASONS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_XEON_PHI_CORE_PERF_LIMIT_REASONS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_XEON_PHI_CORE_PERF_LIMIT_REASONS);
  AsmWriteMsr64 (MSR_XEON_PHI_CORE_PERF_LIMIT_REASONS, Msr.Uint64);
  @endcode
  @note MSR_XEON_PHI_CORE_PERF_LIMIT_REASONS is defined as MSR_CORE_PERF_LIMIT_REASONS in SDM.
**/
#define MSR_XEON_PHI_CORE_PERF_LIMIT_REASONS     0x00000690

/**
  MSR information returned for MSR index #MSR_XEON_PHI_CORE_PERF_LIMIT_REASONS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] PROCHOT Status (R0).
    ///
    UINT32  PROCHOT_Status:1;
    ///
    /// [Bit 1] Thermal Status (R0).
    ///
    UINT32  ThermalStatus:1;
    UINT32  Reserved1:4;
    ///
    /// [Bit 6] VR Therm Alert Status (R0).
    ///
    UINT32  VRThermAlertStatus:1;
    UINT32  Reserved2:1;
    ///
    /// [Bit 8] Electrical Design Point Status (R0).
    ///
    UINT32  ElectricalDesignPointStatus:1;
    UINT32  Reserved3:23;
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
} MSR_XEON_PHI_CORE_PERF_LIMIT_REASONS_REGISTER;

#endif
