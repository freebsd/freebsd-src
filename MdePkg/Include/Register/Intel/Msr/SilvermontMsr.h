/** @file
  MSR Definitions for Intel processors based on the Silvermont microarchitecture.

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

#ifndef __SILVERMONT_MSR_H__
#define __SILVERMONT_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>

/**
  Is Intel processors based on the Silvermont microarchitecture?

  @param   DisplayFamily  Display Family ID
  @param   DisplayModel   Display Model ID

  @retval  TRUE   Yes, it is.
  @retval  FALSE  No, it isn't.
**/
#define IS_SILVERMONT_PROCESSOR(DisplayFamily, DisplayModel) \
  (DisplayFamily == 0x06 && \
   (                        \
    DisplayModel == 0x37 || \
    DisplayModel == 0x4A || \
    DisplayModel == 0x4D || \
    DisplayModel == 0x5A || \
    DisplayModel == 0x5D    \
    )                       \
   )

/**
  Module. Model Specific Platform ID (R).

  @param  ECX  MSR_SILVERMONT_PLATFORM_ID (0x00000017)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PLATFORM_ID_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PLATFORM_ID_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_PLATFORM_ID_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_PLATFORM_ID);
  @endcode
  @note MSR_SILVERMONT_PLATFORM_ID is defined as MSR_PLATFORM_ID in SDM.
**/
#define MSR_SILVERMONT_PLATFORM_ID  0x00000017

/**
  MSR information returned for MSR index #MSR_SILVERMONT_PLATFORM_ID
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
} MSR_SILVERMONT_PLATFORM_ID_REGISTER;

/**
  Module. Processor Hard Power-On Configuration (R/W) Writes ignored.

  @param  ECX  MSR_SILVERMONT_EBL_CR_POWERON (0x0000002A)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_EBL_CR_POWERON_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_EBL_CR_POWERON_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_EBL_CR_POWERON_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_EBL_CR_POWERON);
  AsmWriteMsr64 (MSR_SILVERMONT_EBL_CR_POWERON, Msr.Uint64);
  @endcode
  @note MSR_SILVERMONT_EBL_CR_POWERON is defined as MSR_EBL_CR_POWERON in SDM.
**/
#define MSR_SILVERMONT_EBL_CR_POWERON  0x0000002A

/**
  MSR information returned for MSR index #MSR_SILVERMONT_EBL_CR_POWERON
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1 : 32;
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
} MSR_SILVERMONT_EBL_CR_POWERON_REGISTER;

/**
  Core. SMI Counter (R/O).

  @param  ECX  MSR_SILVERMONT_SMI_COUNT (0x00000034)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_SMI_COUNT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_SMI_COUNT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_SMI_COUNT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_SMI_COUNT);
  @endcode
  @note MSR_SILVERMONT_SMI_COUNT is defined as MSR_SMI_COUNT in SDM.
**/
#define MSR_SILVERMONT_SMI_COUNT  0x00000034

/**
  MSR information returned for MSR index #MSR_SILVERMONT_SMI_COUNT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 31:0] SMI Count (R/O)  Running count of SMI events since last
    /// RESET.
    ///
    UINT32    SMICount : 32;
    UINT32    Reserved : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_SILVERMONT_SMI_COUNT_REGISTER;

/**
  Core. Control Features in Intel 64 Processor (R/W). See Table 2-2.

  @param  ECX  MSR_IA32_SILVERMONT_FEATURE_CONTROL (0x0000003A)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type
               MSR_SILVERMONT_IA32_FEATURE_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type
               MSR_SILVERMONT_IA32_FEATURE_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_IA32_FEATURE_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_IA32_FEATURE_CONTROL);
  AsmWriteMsr64 (MSR_SILVERMONT_IA32_FEATURE_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_SILVERMONT_IA32_FEATURE_CONTROL is defined as IA32_FEATURE_CONTROL in SDM.
**/
#define MSR_SILVERMONT_IA32_FEATURE_CONTROL  0x0000003A

/**
  MSR information returned for MSR index #MSR_SILVERMONT_IA32_FEATURE_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Lock (R/WL).
    ///
    UINT32    Lock                : 1;
    UINT32    Reserved1           : 1;
    ///
    /// [Bit 2] Enable VMX outside SMX operation (R/WL).
    ///
    UINT32    EnableVmxOutsideSmx : 1;
    UINT32    Reserved2           : 29;
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
} MSR_SILVERMONT_IA32_FEATURE_CONTROL_REGISTER;

/**
  Core. Last Branch Record n From IP (R/W) One of eight pairs of last branch
  record registers on the last branch record stack. The From_IP part of the
  stack contains pointers to the source instruction. See also: -  Last Branch
  Record Stack TOS at 1C9H -  Section 17.5 and record format in Section
  17.4.8.1.

  @param  ECX  MSR_SILVERMONT_LASTBRANCH_n_FROM_IP
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_LASTBRANCH_0_FROM_IP);
  AsmWriteMsr64 (MSR_SILVERMONT_LASTBRANCH_0_FROM_IP, Msr);
  @endcode
  @note MSR_SILVERMONT_LASTBRANCH_0_FROM_IP is defined as MSR_LASTBRANCH_0_FROM_IP in SDM.
        MSR_SILVERMONT_LASTBRANCH_1_FROM_IP is defined as MSR_LASTBRANCH_1_FROM_IP in SDM.
        MSR_SILVERMONT_LASTBRANCH_2_FROM_IP is defined as MSR_LASTBRANCH_2_FROM_IP in SDM.
        MSR_SILVERMONT_LASTBRANCH_3_FROM_IP is defined as MSR_LASTBRANCH_3_FROM_IP in SDM.
        MSR_SILVERMONT_LASTBRANCH_4_FROM_IP is defined as MSR_LASTBRANCH_4_FROM_IP in SDM.
        MSR_SILVERMONT_LASTBRANCH_5_FROM_IP is defined as MSR_LASTBRANCH_5_FROM_IP in SDM.
        MSR_SILVERMONT_LASTBRANCH_6_FROM_IP is defined as MSR_LASTBRANCH_6_FROM_IP in SDM.
        MSR_SILVERMONT_LASTBRANCH_7_FROM_IP is defined as MSR_LASTBRANCH_7_FROM_IP in SDM.
  @{
**/
#define MSR_SILVERMONT_LASTBRANCH_0_FROM_IP  0x00000040
#define MSR_SILVERMONT_LASTBRANCH_1_FROM_IP  0x00000041
#define MSR_SILVERMONT_LASTBRANCH_2_FROM_IP  0x00000042
#define MSR_SILVERMONT_LASTBRANCH_3_FROM_IP  0x00000043
#define MSR_SILVERMONT_LASTBRANCH_4_FROM_IP  0x00000044
#define MSR_SILVERMONT_LASTBRANCH_5_FROM_IP  0x00000045
#define MSR_SILVERMONT_LASTBRANCH_6_FROM_IP  0x00000046
#define MSR_SILVERMONT_LASTBRANCH_7_FROM_IP  0x00000047
/// @}

/**
  Core. Last Branch Record n To IP (R/W) One of eight pairs of last branch
  record registers on the last branch record stack. The To_IP part of the
  stack contains pointers to the destination instruction.

  @param  ECX  MSR_SILVERMONT_LASTBRANCH_n_TO_IP
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_LASTBRANCH_0_TO_IP);
  AsmWriteMsr64 (MSR_SILVERMONT_LASTBRANCH_0_TO_IP, Msr);
  @endcode
  @note MSR_SILVERMONT_LASTBRANCH_0_TO_IP is defined as MSR_LASTBRANCH_0_TO_IP in SDM.
        MSR_SILVERMONT_LASTBRANCH_1_TO_IP is defined as MSR_LASTBRANCH_1_TO_IP in SDM.
        MSR_SILVERMONT_LASTBRANCH_2_TO_IP is defined as MSR_LASTBRANCH_2_TO_IP in SDM.
        MSR_SILVERMONT_LASTBRANCH_3_TO_IP is defined as MSR_LASTBRANCH_3_TO_IP in SDM.
        MSR_SILVERMONT_LASTBRANCH_4_TO_IP is defined as MSR_LASTBRANCH_4_TO_IP in SDM.
        MSR_SILVERMONT_LASTBRANCH_5_TO_IP is defined as MSR_LASTBRANCH_5_TO_IP in SDM.
        MSR_SILVERMONT_LASTBRANCH_6_TO_IP is defined as MSR_LASTBRANCH_6_TO_IP in SDM.
        MSR_SILVERMONT_LASTBRANCH_7_TO_IP is defined as MSR_LASTBRANCH_7_TO_IP in SDM.
  @{
**/
#define MSR_SILVERMONT_LASTBRANCH_0_TO_IP  0x00000060
#define MSR_SILVERMONT_LASTBRANCH_1_TO_IP  0x00000061
#define MSR_SILVERMONT_LASTBRANCH_2_TO_IP  0x00000062
#define MSR_SILVERMONT_LASTBRANCH_3_TO_IP  0x00000063
#define MSR_SILVERMONT_LASTBRANCH_4_TO_IP  0x00000064
#define MSR_SILVERMONT_LASTBRANCH_5_TO_IP  0x00000065
#define MSR_SILVERMONT_LASTBRANCH_6_TO_IP  0x00000066
#define MSR_SILVERMONT_LASTBRANCH_7_TO_IP  0x00000067
/// @}

/**
  Module. Scalable Bus Speed(RO) This field indicates the intended scalable
  bus clock speed for processors based on Silvermont microarchitecture:.

  @param  ECX  MSR_SILVERMONT_FSB_FREQ (0x000000CD)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_FSB_FREQ_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_FSB_FREQ_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_FSB_FREQ_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_FSB_FREQ);
  @endcode
  @note MSR_SILVERMONT_FSB_FREQ is defined as MSR_FSB_FREQ in SDM.
**/
#define MSR_SILVERMONT_FSB_FREQ  0x000000CD

/**
  MSR information returned for MSR index #MSR_SILVERMONT_FSB_FREQ
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 3:0] Scalable Bus Speed
    ///
    /// Silvermont Processor Family
    /// ---------------------------
    ///   100B: 080.0 MHz
    ///   000B: 083.3 MHz
    ///   001B: 100.0 MHz
    ///   010B: 133.3 MHz
    ///   011B: 116.7 MHz
    ///
    /// Airmont Processor Family
    /// ---------------------------
    ///   0000B: 083.3 MHz
    ///   0001B: 100.0 MHz
    ///   0010B: 133.3 MHz
    ///   0011B: 116.7 MHz
    ///   0100B: 080.0 MHz
    ///   0101B: 093.3 MHz
    ///   0110B: 090.0 MHz
    ///   0111B: 088.9 MHz
    ///   1000B: 087.5 MHz
    ///
    UINT32    ScalableBusSpeed : 4;
    UINT32    Reserved1        : 28;
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
} MSR_SILVERMONT_FSB_FREQ_REGISTER;

/**
  Package. Platform Information: Contains power management and other model
  specific features enumeration. See http://biosbits.org.

  @param  ECX  MSR_SILVERMONT_PLATFORM_INFO (0x000000CE)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PLATFORM_INFO_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PLATFORM_INFO_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_PLATFORM_INFO_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_PLATFORM_INFO);
  AsmWriteMsr64 (MSR_SILVERMONT_PLATFORM_INFO, Msr.Uint64);
  @endcode
**/
#define MSR_SILVERMONT_PLATFORM_INFO  0x000000CE

/**
  MSR information returned for MSR index #MSR_SILVERMONT_PLATFORM_INFO
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1             : 8;
    ///
    /// [Bits 15:8] Package. Maximum Non-Turbo Ratio (R/O) This is the ratio
    /// of the maximum frequency that does not require turbo. Frequency =
    /// ratio * Scalable Bus Frequency.
    ///
    UINT32    MaximumNon_TurboRatio : 8;
    UINT32    Reserved2             : 16;
    UINT32    Reserved3             : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_SILVERMONT_PLATFORM_INFO_REGISTER;

/**
  Module. C-State Configuration Control (R/W)  Note: C-state values are
  processor specific C-state code names, unrelated to MWAIT extension C-state
  parameters or ACPI CStates. See http://biosbits.org.

  @param  ECX  MSR_SILVERMONT_PKG_CST_CONFIG_CONTROL (0x000000E2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PKG_CST_CONFIG_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PKG_CST_CONFIG_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_PKG_CST_CONFIG_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_PKG_CST_CONFIG_CONTROL);
  AsmWriteMsr64 (MSR_SILVERMONT_PKG_CST_CONFIG_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_SILVERMONT_PKG_CST_CONFIG_CONTROL is defined as MSR_PKG_CST_CONFIG_CONTROL in SDM.
**/
#define MSR_SILVERMONT_PKG_CST_CONFIG_CONTROL  0x000000E2

/**
  MSR information returned for MSR index #MSR_SILVERMONT_PKG_CST_CONFIG_CONTROL
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
    /// C0 (no package C-sate support) 001b: C1 (Behavior is the same as 000b)
    /// 100b: C4 110b: C6 111b: C7 (Silvermont only).
    ///
    UINT32    Limit     : 3;
    UINT32    Reserved1 : 7;
    ///
    /// [Bit 10] I/O MWAIT Redirection Enable (R/W)  When set, will map
    /// IO_read instructions sent to IO register specified by
    /// MSR_PMG_IO_CAPTURE_BASE to MWAIT instructions.
    ///
    UINT32    IO_MWAIT  : 1;
    UINT32    Reserved2 : 4;
    ///
    /// [Bit 15] CFG Lock (R/WO)  When set, lock bits 15:0 of this register
    /// until next reset.
    ///
    UINT32    CFGLock   : 1;
    UINT32    Reserved3 : 16;
    UINT32    Reserved4 : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_SILVERMONT_PKG_CST_CONFIG_CONTROL_REGISTER;

/**
  Module. Power Management IO Redirection in C-state (R/W) See
  http://biosbits.org.

  @param  ECX  MSR_SILVERMONT_PMG_IO_CAPTURE_BASE (0x000000E4)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PMG_IO_CAPTURE_BASE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PMG_IO_CAPTURE_BASE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_PMG_IO_CAPTURE_BASE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_PMG_IO_CAPTURE_BASE);
  AsmWriteMsr64 (MSR_SILVERMONT_PMG_IO_CAPTURE_BASE, Msr.Uint64);
  @endcode
  @note MSR_SILVERMONT_PMG_IO_CAPTURE_BASE is defined as MSR_PMG_IO_CAPTURE_BASE in SDM.
**/
#define MSR_SILVERMONT_PMG_IO_CAPTURE_BASE  0x000000E4

/**
  MSR information returned for MSR index #MSR_SILVERMONT_PMG_IO_CAPTURE_BASE
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
    UINT32    Lvl2Base    : 16;
    ///
    /// [Bits 18:16] C-state Range (R/W)  Specifies the encoding value of the
    /// maximum C-State code name to be included when IO read to MWAIT
    /// redirection is enabled by MSR_PKG_CST_CONFIG_CONTROL[bit10]: 100b - C4
    /// is the max C-State to include 110b - C6 is the max C-State to include
    /// 111b - C7 is the max C-State to include.
    ///
    UINT32    CStateRange : 3;
    UINT32    Reserved1   : 13;
    UINT32    Reserved2   : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_SILVERMONT_PMG_IO_CAPTURE_BASE_REGISTER;

/**
  Module.

  @param  ECX  MSR_SILVERMONT_BBL_CR_CTL3 (0x0000011E)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_BBL_CR_CTL3_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_BBL_CR_CTL3_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_BBL_CR_CTL3_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_BBL_CR_CTL3);
  AsmWriteMsr64 (MSR_SILVERMONT_BBL_CR_CTL3, Msr.Uint64);
  @endcode
  @note MSR_SILVERMONT_BBL_CR_CTL3 is defined as MSR_BBL_CR_CTL3 in SDM.
**/
#define MSR_SILVERMONT_BBL_CR_CTL3  0x0000011E

/**
  MSR information returned for MSR index #MSR_SILVERMONT_BBL_CR_CTL3
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
    /// [Bit 8] L2 Enabled. (R/W)  1 = L2 cache has been initialized 0 =
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
} MSR_SILVERMONT_BBL_CR_CTL3_REGISTER;

/**
  Core. AES Configuration (RW-L) Privileged post-BIOS agent must provide a #GP
  handler to handle unsuccessful read of this MSR.

  @param  ECX  MSR_SILVERMONT_FEATURE_CONFIG (0x0000013C)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_FEATURE_CONFIG_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_FEATURE_CONFIG_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_FEATURE_CONFIG_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_FEATURE_CONFIG);
  AsmWriteMsr64 (MSR_SILVERMONT_FEATURE_CONFIG, Msr.Uint64);
  @endcode
  @note MSR_SILVERMONT_FEATURE_CONFIG is defined as MSR_FEATURE_CONFIG in SDM.
**/
#define MSR_SILVERMONT_FEATURE_CONFIG  0x0000013C

/**
  MSR information returned for MSR index #MSR_SILVERMONT_FEATURE_CONFIG
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
    UINT32    AESConfiguration : 2;
    UINT32    Reserved1        : 30;
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
} MSR_SILVERMONT_FEATURE_CONFIG_REGISTER;

/**
  Enable Misc. Processor Features (R/W)  Allows a variety of processor
  functions to be enabled and disabled.

  @param  ECX  MSR_SILVERMONT_IA32_MISC_ENABLE (0x000001A0)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_IA32_MISC_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_IA32_MISC_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_IA32_MISC_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_IA32_MISC_ENABLE);
  AsmWriteMsr64 (MSR_SILVERMONT_IA32_MISC_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_SILVERMONT_IA32_MISC_ENABLE is defined as IA32_MISC_ENABLE in SDM.
**/
#define MSR_SILVERMONT_IA32_MISC_ENABLE  0x000001A0

/**
  MSR information returned for MSR index #MSR_SILVERMONT_IA32_MISC_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Core. Fast-Strings Enable See Table 2-2.
    ///
    UINT32    FastStrings                    : 1;
    UINT32    Reserved1                      : 2;
    ///
    /// [Bit 3] Module. Automatic Thermal Control Circuit Enable (R/W) See
    /// Table 2-2. Default value is 0.
    ///
    UINT32    AutomaticThermalControlCircuit : 1;
    UINT32    Reserved2                      : 3;
    ///
    /// [Bit 7] Core. Performance Monitoring Available (R) See Table 2-2.
    ///
    UINT32    PerformanceMonitoring          : 1;
    UINT32    Reserved3                      : 3;
    ///
    /// [Bit 11] Core. Branch Trace Storage Unavailable (RO) See Table 2-2.
    ///
    UINT32    BTS                            : 1;
    ///
    /// [Bit 12] Core. Processor Event Based Sampling Unavailable (RO) See
    /// Table 2-2.
    ///
    UINT32    PEBS                           : 1;
    UINT32    Reserved4                      : 3;
    ///
    /// [Bit 16] Module. Enhanced Intel SpeedStep Technology Enable (R/W) See
    /// Table 2-2.
    ///
    UINT32    EIST                           : 1;
    UINT32    Reserved5                      : 1;
    ///
    /// [Bit 18] Core. ENABLE MONITOR FSM (R/W) See Table 2-2.
    ///
    UINT32    MONITOR                        : 1;
    UINT32    Reserved6                      : 3;
    ///
    /// [Bit 22] Core. Limit CPUID Maxval (R/W) See Table 2-2.
    ///
    UINT32    LimitCpuidMaxval               : 1;
    ///
    /// [Bit 23] Module. xTPR Message Disable (R/W) See Table 2-2.
    ///
    UINT32    xTPR_Message_Disable           : 1;
    UINT32    Reserved7                      : 8;
    UINT32    Reserved8                      : 2;
    ///
    /// [Bit 34] Core. XD Bit Disable (R/W) See Table 2-2.
    ///
    UINT32    XD                             : 1;
    UINT32    Reserved9                      : 3;
    ///
    /// [Bit 38] Module. Turbo Mode Disable (R/W) When set to 1 on processors
    /// that support Intel Turbo Boost Technology, the turbo mode feature is
    /// disabled and the IDA_Enable feature flag will be clear (CPUID.06H:
    /// EAX[1]=0). When set to a 0 on processors that support IDA, CPUID.06H:
    /// EAX[1] reports the processor's support of turbo mode is enabled. Note:
    /// the power-on default value is used by BIOS to detect hardware support
    /// of turbo mode. If power-on default value is 1, turbo mode is available
    /// in the processor. If power-on default value is 0, turbo mode is not
    /// available.
    ///
    UINT32    TurboModeDisable : 1;
    UINT32    Reserved10       : 25;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_SILVERMONT_IA32_MISC_ENABLE_REGISTER;

/**
  Package.

  @param  ECX  MSR_SILVERMONT_TEMPERATURE_TARGET (0x000001A2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_TEMPERATURE_TARGET_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_TEMPERATURE_TARGET_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_TEMPERATURE_TARGET_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_TEMPERATURE_TARGET);
  AsmWriteMsr64 (MSR_SILVERMONT_TEMPERATURE_TARGET, Msr.Uint64);
  @endcode
  @note MSR_SILVERMONT_TEMPERATURE_TARGET is defined as MSR_TEMPERATURE_TARGET in SDM.
**/
#define MSR_SILVERMONT_TEMPERATURE_TARGET  0x000001A2

/**
  MSR information returned for MSR index #MSR_SILVERMONT_TEMPERATURE_TARGET
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1         : 16;
    ///
    /// [Bits 23:16] Temperature Target (R)  The default thermal throttling or
    /// PROCHOT# activation temperature in degree C, The effective temperature
    /// for thermal throttling or PROCHOT# activation is "Temperature Target"
    /// + "Target Offset".
    ///
    UINT32    TemperatureTarget : 8;
    ///
    /// [Bits 29:24] Target Offset (R/W)  Specifies an offset in degrees C to
    /// adjust the throttling and PROCHOT# activation temperature from the
    /// default target specified in TEMPERATURE_TARGET (bits 23:16).
    ///
    UINT32    TargetOffset      : 6;
    UINT32    Reserved2         : 2;
    UINT32    Reserved3         : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_SILVERMONT_TEMPERATURE_TARGET_REGISTER;

/**
  Miscellaneous Feature Control (R/W).

  @param  ECX  MSR_SILVERMONT_MISC_FEATURE_CONTROL (0x000001A4)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_MISC_FEATURE_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_MISC_FEATURE_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_MISC_FEATURE_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_MISC_FEATURE_CONTROL);
  AsmWriteMsr64 (MSR_SILVERMONT_MISC_FEATURE_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_SILVERMONT_MISC_FEATURE_CONTROL is defined as MSR_MISC_FEATURE_CONTROL in SDM.
**/
#define MSR_SILVERMONT_MISC_FEATURE_CONTROL  0x000001A4

/**
  MSR information returned for MSR index #MSR_SILVERMONT_MISC_FEATURE_CONTROL
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
    UINT32    L2HardwarePrefetcherDisable  : 1;
    UINT32    Reserved1                    : 1;
    ///
    /// [Bit 2] Core. DCU Hardware Prefetcher Disable (R/W)  If 1, disables
    /// the L1 data cache prefetcher, which fetches the next cache line into
    /// L1 data cache.
    ///
    UINT32    DCUHardwarePrefetcherDisable : 1;
    UINT32    Reserved2                    : 29;
    UINT32    Reserved3                    : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_SILVERMONT_MISC_FEATURE_CONTROL_REGISTER;

/**
  Module. Offcore Response Event Select Register (R/W).

  @param  ECX  MSR_SILVERMONT_OFFCORE_RSP_0 (0x000001A6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_OFFCORE_RSP_0);
  AsmWriteMsr64 (MSR_SILVERMONT_OFFCORE_RSP_0, Msr);
  @endcode
  @note MSR_SILVERMONT_OFFCORE_RSP_0 is defined as MSR_OFFCORE_RSP_0 in SDM.
**/
#define MSR_SILVERMONT_OFFCORE_RSP_0  0x000001A6

/**
  Module. Offcore Response Event Select Register (R/W).

  @param  ECX  MSR_SILVERMONT_OFFCORE_RSP_1 (0x000001A7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_OFFCORE_RSP_1);
  AsmWriteMsr64 (MSR_SILVERMONT_OFFCORE_RSP_1, Msr);
  @endcode
  @note MSR_SILVERMONT_OFFCORE_RSP_1 is defined as MSR_OFFCORE_RSP_1 in SDM.
**/
#define MSR_SILVERMONT_OFFCORE_RSP_1  0x000001A7

/**
  Package. Maximum Ratio Limit of Turbo Mode (RW).

  @param  ECX  MSR_SILVERMONT_TURBO_RATIO_LIMIT (0x000001AD)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_TURBO_RATIO_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_TURBO_RATIO_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_TURBO_RATIO_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_TURBO_RATIO_LIMIT);
  AsmWriteMsr64 (MSR_SILVERMONT_TURBO_RATIO_LIMIT, Msr.Uint64);
  @endcode
  @note MSR_SILVERMONT_TURBO_RATIO_LIMIT is defined as MSR_TURBO_RATIO_LIMIT in SDM.
**/
#define MSR_SILVERMONT_TURBO_RATIO_LIMIT  0x000001AD

/**
  MSR information returned for MSR index #MSR_SILVERMONT_TURBO_RATIO_LIMIT
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
    UINT32    Maximum1C : 8;
    ///
    /// [Bits 15:8] Package. Maximum Ratio Limit for 2C Maximum turbo ratio
    /// limit of 2 core active.
    ///
    UINT32    Maximum2C : 8;
    ///
    /// [Bits 23:16] Package. Maximum Ratio Limit for 3C Maximum turbo ratio
    /// limit of 3 core active.
    ///
    UINT32    Maximum3C : 8;
    ///
    /// [Bits 31:24] Package. Maximum Ratio Limit for 4C Maximum turbo ratio
    /// limit of 4 core active.
    ///
    UINT32    Maximum4C : 8;
    ///
    /// [Bits 39:32] Package. Maximum Ratio Limit for 5C Maximum turbo ratio
    /// limit of 5 core active.
    ///
    UINT32    Maximum5C : 8;
    ///
    /// [Bits 47:40] Package. Maximum Ratio Limit for 6C Maximum turbo ratio
    /// limit of 6 core active.
    ///
    UINT32    Maximum6C : 8;
    ///
    /// [Bits 55:48] Package. Maximum Ratio Limit for 7C Maximum turbo ratio
    /// limit of 7 core active.
    ///
    UINT32    Maximum7C : 8;
    ///
    /// [Bits 63:56] Package. Maximum Ratio Limit for 8C Maximum turbo ratio
    /// limit of 8 core active.
    ///
    UINT32    Maximum8C : 8;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_SILVERMONT_TURBO_RATIO_LIMIT_REGISTER;

/**
  Core. Last Branch Record Filtering Select Register (R/W) See Section 17.9.2,
  "Filtering of Last Branch Records.".

  @param  ECX  MSR_SILVERMONT_LBR_SELECT (0x000001C8)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_LBR_SELECT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_LBR_SELECT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_LBR_SELECT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_LBR_SELECT);
  AsmWriteMsr64 (MSR_SILVERMONT_LBR_SELECT, Msr.Uint64);
  @endcode
  @note MSR_SILVERMONT_LBR_SELECT is defined as MSR_LBR_SELECT in SDM.
**/
#define MSR_SILVERMONT_LBR_SELECT  0x000001C8

/**
  MSR information returned for MSR index #MSR_SILVERMONT_LBR_SELECT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] CPL_EQ_0.
    ///
    UINT32    CPL_EQ_0      : 1;
    ///
    /// [Bit 1] CPL_NEQ_0.
    ///
    UINT32    CPL_NEQ_0     : 1;
    ///
    /// [Bit 2] JCC.
    ///
    UINT32    JCC           : 1;
    ///
    /// [Bit 3] NEAR_REL_CALL.
    ///
    UINT32    NEAR_REL_CALL : 1;
    ///
    /// [Bit 4] NEAR_IND_CALL.
    ///
    UINT32    NEAR_IND_CALL : 1;
    ///
    /// [Bit 5] NEAR_RET.
    ///
    UINT32    NEAR_RET      : 1;
    ///
    /// [Bit 6] NEAR_IND_JMP.
    ///
    UINT32    NEAR_IND_JMP  : 1;
    ///
    /// [Bit 7] NEAR_REL_JMP.
    ///
    UINT32    NEAR_REL_JMP  : 1;
    ///
    /// [Bit 8] FAR_BRANCH.
    ///
    UINT32    FAR_BRANCH    : 1;
    UINT32    Reserved1     : 23;
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
} MSR_SILVERMONT_LBR_SELECT_REGISTER;

/**
  Core. Last Branch Record Stack TOS (R/W)  Contains an index (bits 0-2) that
  points to the MSR containing the most recent branch record. See
  MSR_LASTBRANCH_0_FROM_IP.

  @param  ECX  MSR_SILVERMONT_LASTBRANCH_TOS (0x000001C9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_LASTBRANCH_TOS);
  AsmWriteMsr64 (MSR_SILVERMONT_LASTBRANCH_TOS, Msr);
  @endcode
  @note MSR_SILVERMONT_LASTBRANCH_TOS is defined as MSR_LASTBRANCH_TOS in SDM.
**/
#define MSR_SILVERMONT_LASTBRANCH_TOS  0x000001C9

/**
  Core. Last Exception Record From Linear IP (R)  Contains a pointer to the
  last branch instruction that the processor executed prior to the last
  exception that was generated or the last interrupt that was handled.

  @param  ECX  MSR_SILVERMONT_LER_FROM_LIP (0x000001DD)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_LER_FROM_LIP);
  @endcode
  @note MSR_SILVERMONT_LER_FROM_LIP is defined as MSR_LER_FROM_LIP in SDM.
**/
#define MSR_SILVERMONT_LER_FROM_LIP  0x000001DD

/**
  Core. Last Exception Record To Linear IP (R)  This area contains a pointer
  to the target of the last branch instruction that the processor executed
  prior to the last exception that was generated or the last interrupt that
  was handled.

  @param  ECX  MSR_SILVERMONT_LER_TO_LIP (0x000001DE)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_LER_TO_LIP);
  @endcode
  @note MSR_SILVERMONT_LER_TO_LIP is defined as MSR_LER_TO_LIP in SDM.
**/
#define MSR_SILVERMONT_LER_TO_LIP  0x000001DE

/**
  Core. See Table 2-2. See Section 18.6.2.4, "Processor Event Based Sampling
  (PEBS).".

  @param  ECX  MSR_SILVERMONT_PEBS_ENABLE (0x000003F1)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PEBS_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PEBS_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_PEBS_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_PEBS_ENABLE);
  AsmWriteMsr64 (MSR_SILVERMONT_PEBS_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_SILVERMONT_PEBS_ENABLE is defined as MSR_PEBS_ENABLE in SDM.
**/
#define MSR_SILVERMONT_PEBS_ENABLE  0x000003F1

/**
  MSR information returned for MSR index #MSR_SILVERMONT_PEBS_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Enable PEBS for precise event on IA32_PMC0. (R/W).
    ///
    UINT32    PEBS      : 1;
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
} MSR_SILVERMONT_PEBS_ENABLE_REGISTER;

/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. Package C6
  Residency Counter. (R/O) Value since last reset that this package is in
  processor-specific C6 states. Counts at the TSC Frequency.

  @param  ECX  MSR_SILVERMONT_PKG_C6_RESIDENCY (0x000003FA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_PKG_C6_RESIDENCY);
  AsmWriteMsr64 (MSR_SILVERMONT_PKG_C6_RESIDENCY, Msr);
  @endcode
  @note MSR_SILVERMONT_PKG_C6_RESIDENCY is defined as MSR_PKG_C6_RESIDENCY in SDM.
**/
#define MSR_SILVERMONT_PKG_C6_RESIDENCY  0x000003FA

/**
  Core. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. CORE C6
  Residency Counter. (R/O) Value since last reset that this core is in
  processor-specific C6 states. Counts at the TSC Frequency.

  @param  ECX  MSR_SILVERMONT_CORE_C6_RESIDENCY (0x000003FD)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_CORE_C6_RESIDENCY);
  AsmWriteMsr64 (MSR_SILVERMONT_CORE_C6_RESIDENCY, Msr);
  @endcode
  @note MSR_SILVERMONT_CORE_C6_RESIDENCY is defined as MSR_CORE_C6_RESIDENCY in SDM.
**/
#define MSR_SILVERMONT_CORE_C6_RESIDENCY  0x000003FD

/**
  Core. Capability Reporting Register of EPT and VPID (R/O) See Table 2-2.

  @param  ECX  MSR_SILVERMONT_IA32_VMX_EPT_VPID_ENUM (0x0000048C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_IA32_VMX_EPT_VPID_ENUM);
  @endcode
  @note MSR_SILVERMONT_IA32_VMX_EPT_VPID_ENUM is defined as IA32_VMX_EPT_VPID_ENUM in SDM.
**/
#define MSR_SILVERMONT_IA32_VMX_EPT_VPID_ENUM  0x0000048C

/**
  Core. Capability Reporting Register of VM-Function Controls (R/O) See Table
  2-2.

  @param  ECX  MSR_SILVERMONT_IA32_VMX_FMFUNC (0x00000491)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_IA32_VMX_FMFUNC);
  @endcode
  @note MSR_SILVERMONT_IA32_VMX_FMFUNC is defined as IA32_VMX_FMFUNC in SDM.
**/
#define MSR_SILVERMONT_IA32_VMX_FMFUNC  0x00000491

/**
  Core. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. CORE C1
  Residency Counter. (R/O) Value since last reset that this core is in
  processor-specific C1 states. Counts at the TSC frequency.

  @param  ECX  MSR_SILVERMONT_CORE_C1_RESIDENCY (0x00000660)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_CORE_C1_RESIDENCY);
  AsmWriteMsr64 (MSR_SILVERMONT_CORE_C1_RESIDENCY, Msr);
  @endcode
  @note MSR_SILVERMONT_CORE_C1_RESIDENCY is defined as MSR_CORE_C1_RESIDENCY in SDM.
**/
#define MSR_SILVERMONT_CORE_C1_RESIDENCY  0x00000660

/**
  Package. Unit Multipliers used in RAPL Interfaces (R/O) See Section 14.9.1,
  "RAPL Interfaces.".

  @param  ECX  MSR_SILVERMONT_RAPL_POWER_UNIT (0x00000606)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_RAPL_POWER_UNIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_RAPL_POWER_UNIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_RAPL_POWER_UNIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_RAPL_POWER_UNIT);
  @endcode
  @note MSR_SILVERMONT_RAPL_POWER_UNIT is defined as MSR_RAPL_POWER_UNIT in SDM.
**/
#define MSR_SILVERMONT_RAPL_POWER_UNIT  0x00000606

/**
  MSR information returned for MSR index #MSR_SILVERMONT_RAPL_POWER_UNIT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 3:0] Power Units. Power related information (in milliWatts) is
    /// based on the multiplier, 2^PU; where PU is an unsigned integer
    /// represented by bits 3:0. Default value is 0101b, indicating power unit
    /// is in 32 milliWatts increment.
    ///
    UINT32    PowerUnits        : 4;
    UINT32    Reserved1         : 4;
    ///
    /// [Bits 12:8] Energy Status Units. Energy related information (in
    /// microJoules) is based on the multiplier, 2^ESU; where ESU is an
    /// unsigned integer represented by bits 12:8. Default value is 00101b,
    /// indicating energy unit is in 32 microJoules increment.
    ///
    UINT32    EnergyStatusUnits : 5;
    UINT32    Reserved2         : 3;
    ///
    /// [Bits 19:16] Time Unit. The value is 0000b, indicating time unit is in
    /// one second.
    ///
    UINT32    TimeUnits         : 4;
    UINT32    Reserved3         : 12;
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
} MSR_SILVERMONT_RAPL_POWER_UNIT_REGISTER;

/**
  Package. PKG RAPL Power Limit Control (R/W).

  @param  ECX  MSR_SILVERMONT_PKG_POWER_LIMIT (0x00000610)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PKG_POWER_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PKG_POWER_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_PKG_POWER_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_PKG_POWER_LIMIT);
  AsmWriteMsr64 (MSR_SILVERMONT_PKG_POWER_LIMIT, Msr.Uint64);
  @endcode
  @note MSR_SILVERMONT_PKG_POWER_LIMIT is defined as MSR_PKG_POWER_LIMIT in SDM.
**/
#define MSR_SILVERMONT_PKG_POWER_LIMIT  0x00000610

/**
  MSR information returned for MSR index #MSR_SILVERMONT_PKG_POWER_LIMIT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 14:0] Package Power Limit #1 (R/W) See Section 14.9.3, "Package
    /// RAPL Domain." and MSR_RAPL_POWER_UNIT in Table 2-8.
    ///
    UINT32    Limit         : 15;
    ///
    /// [Bit 15] Enable Power Limit #1. (R/W) See Section 14.9.3, "Package
    /// RAPL Domain.".
    ///
    UINT32    Enable        : 1;
    ///
    /// [Bit 16] Package Clamping Limitation #1. (R/W) See Section 14.9.3,
    /// "Package RAPL Domain.".
    ///
    UINT32    ClampingLimit : 1;
    ///
    /// [Bits 23:17] Time Window for Power Limit #1. (R/W) in unit of second.
    /// If 0 is specified in bits [23:17], defaults to 1 second window.
    ///
    UINT32    Time          : 7;
    UINT32    Reserved1     : 8;
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
} MSR_SILVERMONT_PKG_POWER_LIMIT_REGISTER;

/**
  Package. PKG Energy Status (R/O) See Section 14.9.3, "Package RAPL Domain."
  and MSR_RAPL_POWER_UNIT in Table 2-8.

  @param  ECX  MSR_SILVERMONT_PKG_ENERGY_STATUS (0x00000611)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_PKG_ENERGY_STATUS);
  @endcode
  @note MSR_SILVERMONT_PKG_ENERGY_STATUS is defined as MSR_PKG_ENERGY_STATUS in SDM.
**/
#define MSR_SILVERMONT_PKG_ENERGY_STATUS  0x00000611

/**
  Package. PP0 Energy Status (R/O) See Section 14.9.4, "PP0/PP1 RAPL Domains."
  and MSR_RAPL_POWER_UNIT in Table 2-8.

  @param  ECX  MSR_SILVERMONT_PP0_ENERGY_STATUS (0x00000639)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_PP0_ENERGY_STATUS);
  @endcode
  @note MSR_SILVERMONT_PP0_ENERGY_STATUS is defined as MSR_PP0_ENERGY_STATUS in SDM.
**/
#define MSR_SILVERMONT_PP0_ENERGY_STATUS  0x00000639

/**
  Package. Core C6 demotion policy config MSR. Controls per-core C6 demotion
  policy. Writing a value of 0 disables core level HW demotion policy.

  @param  ECX  MSR_SILVERMONT_CC6_DEMOTION_POLICY_CONFIG (0x00000668)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_CC6_DEMOTION_POLICY_CONFIG);
  AsmWriteMsr64 (MSR_SILVERMONT_CC6_DEMOTION_POLICY_CONFIG, Msr);
  @endcode
  @note MSR_SILVERMONT_CC6_DEMOTION_POLICY_CONFIG is defined as MSR_CC6_DEMOTION_POLICY_CONFIG in SDM.
**/
#define MSR_SILVERMONT_CC6_DEMOTION_POLICY_CONFIG  0x00000668

/**
  Package. Module C6 demotion policy config MSR. Controls module (i.e. two
  cores sharing the second-level cache) C6 demotion policy. Writing a value of
  0 disables module level HW demotion policy.

  @param  ECX  MSR_SILVERMONT_MC6_DEMOTION_POLICY_CONFIG (0x00000669)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_MC6_DEMOTION_POLICY_CONFIG);
  AsmWriteMsr64 (MSR_SILVERMONT_MC6_DEMOTION_POLICY_CONFIG, Msr);
  @endcode
  @note MSR_SILVERMONT_MC6_DEMOTION_POLICY_CONFIG is defined as MSR_MC6_DEMOTION_POLICY_CONFIG in SDM.
**/
#define MSR_SILVERMONT_MC6_DEMOTION_POLICY_CONFIG  0x00000669

/**
  Module. Module C6 Residency Counter (R/0) Note: C-state values are processor
  specific C-state code names, unrelated to MWAIT extension C-state parameters
  or ACPI CStates. Time that this module is in module-specific C6 states since
  last reset. Counts at 1 Mhz frequency.

  @param  ECX  MSR_SILVERMONT_MC6_RESIDENCY_COUNTER (0x00000664)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SILVERMONT_MC6_RESIDENCY_COUNTER);
  @endcode
  @note MSR_SILVERMONT_MC6_RESIDENCY_COUNTER is defined as MSR_MC6_RESIDENCY_COUNTER in SDM.
**/
#define MSR_SILVERMONT_MC6_RESIDENCY_COUNTER  0x00000664

/**
  Package. PKG RAPL Parameter (R/0).

  @param  ECX  MSR_SILVERMONT_PKG_POWER_INFO (0x0000066E)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PKG_POWER_INFO_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PKG_POWER_INFO_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_PKG_POWER_INFO_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_PKG_POWER_INFO);
  @endcode
  @note MSR_SILVERMONT_PKG_POWER_INFO is defined as MSR_PKG_POWER_INFO in SDM.
**/
#define MSR_SILVERMONT_PKG_POWER_INFO  0x0000066E

/**
  MSR information returned for MSR index #MSR_SILVERMONT_PKG_POWER_INFO
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 14:0] Thermal Spec Power. (R/0) The unsigned integer value is
    /// the equivalent of thermal specification power of the package domain.
    /// The unit of this field is specified by the "Power Units" field of
    /// MSR_RAPL_POWER_UNIT.
    ///
    UINT32    ThermalSpecPower : 15;
    UINT32    Reserved1        : 17;
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
} MSR_SILVERMONT_PKG_POWER_INFO_REGISTER;

/**
  Package. PP0 RAPL Power Limit Control (R/W).

  @param  ECX  MSR_SILVERMONT_PP0_POWER_LIMIT (0x00000638)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PP0_POWER_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SILVERMONT_PP0_POWER_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SILVERMONT_PP0_POWER_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SILVERMONT_PP0_POWER_LIMIT);
  AsmWriteMsr64 (MSR_SILVERMONT_PP0_POWER_LIMIT, Msr.Uint64);
  @endcode
  @note MSR_SILVERMONT_PP0_POWER_LIMIT is defined as MSR_PP0_POWER_LIMIT in SDM.
**/
#define MSR_SILVERMONT_PP0_POWER_LIMIT  0x00000638

/**
  MSR information returned for MSR index #MSR_SILVERMONT_PP0_POWER_LIMIT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 14:0] PP0 Power Limit #1. (R/W) See Section 14.9.4, "PP0/PP1
    /// RAPL Domains." and MSR_RAPL_POWER_UNIT in Table 35-8.
    ///
    UINT32    Limit     : 15;
    ///
    /// [Bit 15] Enable Power Limit #1. (R/W) See Section 14.9.4, "PP0/PP1
    /// RAPL Domains.".
    ///
    UINT32    Enable    : 1;
    UINT32    Reserved1 : 1;
    ///
    /// [Bits 23:17] Time Window for Power Limit #1. (R/W) Specifies the time
    /// duration over which the average power must remain below
    /// PP0_POWER_LIMIT #1(14:0). Supported Encodings: 0x0: 1 second time
    /// duration. 0x1: 5 second time duration (Default). 0x2: 10 second time
    /// duration. 0x3: 15 second time duration. 0x4: 20 second time duration.
    /// 0x5: 25 second time duration. 0x6: 30 second time duration. 0x7: 35
    /// second time duration. 0x8: 40 second time duration. 0x9: 45 second
    /// time duration. 0xA: 50 second time duration. 0xB-0x7F - reserved.
    ///
    UINT32    Time      : 7;
    UINT32    Reserved2 : 8;
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
} MSR_SILVERMONT_PP0_POWER_LIMIT_REGISTER;

#endif
