/** @file
  MSR Definitions.

  Provides defines for Machine Specific Registers(MSR) indexes. Data structures
  are provided for MSRs that contain one or more bit fields.  If the MSR value
  returned is a single 32-bit or 64-bit value, then a data structure is not
  provided for that MSR.

  Copyright (c) 2017 - 2024, Advanced Micro Devices. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
  AMD64 Architecture Programming Manual volume 2, March 2024

**/

#ifndef SEV_SNP_MSR_H_
#define SEV_SNP_MSR_H_

/**
  Secure Encrypted Virtualization - Encrypted State (SEV-ES) GHCB register

**/
#define MSR_SEV_ES_GHCB  0xc0010130

/**
  MSR information returned for #MSR_SEV_ES_GHCB
**/
typedef union {
  struct {
    UINT32    Function  : 12;
    UINT32    Reserved1 : 20;
    UINT32    Reserved2 : 32;
  } GhcbInfo;

  struct {
    UINT8     Reserved[3];
    UINT8     SevEncryptionBitPos;
    UINT16    SevEsProtocolMin;
    UINT16    SevEsProtocolMax;
  } GhcbProtocol;

  struct {
    UINT32    Function      : 12;
    UINT32    ReasonCodeSet : 4;
    UINT32    ReasonCode    : 8;
    UINT32    Reserved1     : 8;
    UINT32    Reserved2     : 32;
  } GhcbTerminate;

  struct {
    UINT64    Function : 12;
    UINT64    Features : 52;
  } GhcbHypervisorFeatures;

  struct {
    UINT64    Function         : 12;
    UINT64    GuestFrameNumber : 52;
  } GhcbGpaRegister;

  struct {
    UINT64    Function         : 12;
    UINT64    GuestFrameNumber : 40;
    UINT64    Operation        : 4;
    UINT64    Reserved         : 8;
  } SnpPageStateChangeRequest;

  struct {
    UINT32    Function : 12;
    UINT32    Reserved : 20;
    UINT32    ErrorCode;
  } SnpPageStateChangeResponse;

  struct {
    UINT64    Function  : 12;
    UINT64    Reserved1 : 20;
    UINT64    Vmpl      : 8;
    UINT64    Reserved2 : 56;
  } SnpVmplRequest;

  struct {
    UINT32    Function : 12;
    UINT32    Reserved : 20;
    UINT32    ErrorCode;
  } SnpVmplResponse;

  VOID      *Ghcb;

  UINT64    GhcbPhysicalAddress;

  UINT64    Uint64;
} MSR_SEV_ES_GHCB_REGISTER;

#define GHCB_INFO_SEV_INFO                        1
#define GHCB_INFO_SEV_INFO_GET                    2
#define GHCB_INFO_CPUID_REQUEST                   4
#define GHCB_INFO_CPUID_RESPONSE                  5
#define GHCB_INFO_GHCB_GPA_REGISTER_REQUEST       18
#define GHCB_INFO_GHCB_GPA_REGISTER_RESPONSE      19
#define GHCB_INFO_SNP_PAGE_STATE_CHANGE_REQUEST   20
#define GHCB_INFO_SNP_PAGE_STATE_CHANGE_RESPONSE  21
#define GHCB_INFO_SNP_VMPL_REQUEST                22
#define GHCB_INFO_SNP_VMPL_RESPONSE               23
#define GHCB_HYPERVISOR_FEATURES_REQUEST          128
#define GHCB_HYPERVISOR_FEATURES_RESPONSE         129
#define GHCB_INFO_TERMINATE_REQUEST               256

#define GHCB_TERMINATE_GHCB           0
#define GHCB_TERMINATE_GHCB_GENERAL   0
#define GHCB_TERMINATE_GHCB_PROTOCOL  1

/**
  Secure Encrypted Virtualization (SEV) status register

**/
#define MSR_SEV_STATUS  0xc0010131

/**
  MSR information returned for #MSR_SEV_STATUS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Secure Encrypted Virtualization (Sev) is enabled
    ///
    UINT32    SevBit              : 1;

    ///
    /// [Bit 1] Secure Encrypted Virtualization Encrypted State (SevEs) is enabled
    ///
    UINT32    SevEsBit            : 1;

    ///
    /// [Bit 2] Secure Nested Paging (SevSnp) is enabled
    ///
    UINT32    SevSnpBit           : 1;

    ///
    /// [Bit 3] Virtual TOM feature is enabled in SEV_FEATURES[1]
    ///
    UINT32    vTOM                : 1;

    ///
    /// [Bit 4] ReflectVC feature is enabled in SEV_FEATURES[2]
    ///
    UINT32    ReflectVC           : 1;

    ///
    /// [Bit 5] Restricted Injection feature is enabled in SEV_FEATURES[3]
    ///
    UINT32    RestrictedInjection : 1;

    ///
    /// [Bit 6] Alternate Injection feature is enabled in SEV_FEATURES[4]
    ///
    UINT32    AlternateInjection  : 1;

    ///
    /// [Bit 7] Debug Virtualization feature is enabled in SEV_FEATURES[5]
    ///
    UINT32    DebugVirtualization : 1;

    ///
    /// [Bit 8]  PreventHostIBS feature is enabled in SEV_FEATURES[6]
    ///
    UINT32    PreventHostIBS      : 1;

    ///
    /// [Bit 9] BTB isolation feature is enabled in SEV_FEATURES[7]
    ///
    UINT32    SNPBTBIsolation     : 1;

    ///
    /// [Bit 10] VMPL SSS feature is enabled in SEV_FEATURES[8]
    ///
    UINT32    VmplSSS             : 1;

    ///
    /// [Bit 11] Secure TSC feature is enabled in SEV_FEATURES[9]
    ///
    UINT32    SecureTsc           : 1;

    ///
    /// [Bit 12] VMGEXIT Parameter feature is enabled in SEV_FEATURES[10]
    ///
    UINT32    VmgexitParameter    : 1;

    ///
    /// [Bit 13] PMC Virtualization feature is enabled in SEV_FEATURES[11]
    ///
    UINT32    PmcVirtualization   : 1;

    ///
    /// [Bit 14] IBS Virtualization feature is enabled in SEV_FEATURES[12]
    ///
    UINT32    IbsVirtualization   : 1;

    ///
    /// [Bit 15]
    ///
    UINT32    Reserved1           : 1;

    ///
    /// [Bit 16] VMSA Register Protection feature is enabled in SEV_FEATURES[14]
    ///
    UINT32    VmsaRegProt         : 1;

    ///
    /// [Bit 17] SMT Protection feature is enabled in SEV_FEATURES[15]
    ///
    UINT32    SmtProtection       : 1;
    ///
    ///
    /// [Bit 18] Secure AVIC feature is enabled in SEV_FEATURES[16]
    ///
    UINT32    SecureAVIC          : 1;

    UINT32    Reserved2           : 4;

    ///
    /// [Bit 23] IBPB on Entry feature is enabled in SEV_FEATURES[21]
    ///
    UINT32    IbpbOnEntry         : 1;

    UINT32    Reserved3           : 8;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_SEV_STATUS_REGISTER;

#endif
