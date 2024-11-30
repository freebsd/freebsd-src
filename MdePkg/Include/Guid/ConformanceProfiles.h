/** @file
  GUIDs used for UEFI Conformance Profiles Table in the UEFI 2.10 specification.

  Copyright (c) 2024, Arm Limited. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CONFORMANCE_PROFILES_TABLE_GUID_H_
#define CONFORMANCE_PROFILES_TABLE_GUID_H_

//
// This table allows the platform to advertise its UEFI specification conformance
// in the form of pre-defined profiles. Each profile is identified by a GUID, with
// known profiles listed in the section below.
// The absence of this table shall indicate that the platform implementation is
// conformant with the UEFI specification requirements, as defined in Section 2.6.
// This is equivalent to publishing this configuration table with the
// EFI_CONFORMANCE_PROFILES_UEFI_SPEC_GUID conformance profile.
//
#define EFI_CONFORMANCE_PROFILES_TABLE_GUID \
  { \
    0x36122546, 0xf7e7, 0x4c8f, { 0xbd, 0x9b, 0xeb, 0x85, 0x25, 0xb5, 0x0c, 0x0b } \
  }

#pragma pack(1)

typedef struct {
  ///
  /// Version of the table must be 0x1
  ///
  UINT16    Version;
  ///
  /// The number of profiles GUIDs present in ConformanceProfiles
  ///
  UINT16    NumberOfProfiles;
  ///
  /// An array of conformance profile GUIDs that are supported by this system.
  /// EFI_GUID        ConformanceProfiles[];
  ///
} EFI_CONFORMANCE_PROFILES_TABLE;

#pragma pack()

#define EFI_CONFORMANCE_PROFILES_TABLE_VERSION  0x1

//
// GUID defined in UEFI 2.10
//
#define EFI_CONFORMANCE_PROFILES_UEFI_SPEC_GUID \
    { 0x523c91af, 0xa195, 0x4382, \
    { 0x81, 0x8d, 0x29, 0x5f, 0xe4, 0x00, 0x64, 0x65 }}

//
// GUID defined in EBBR
//
#define EFI_CONFORMANCE_PROFILE_EBBR_2_1_GUID \
    { 0xcce33c35, 0x74ac, 0x4087, \
    { 0xbc, 0xe7, 0x8b, 0x29, 0xb0, 0x2e, 0xeb, 0x27 }}
#define EFI_CONFORMANCE_PROFILE_EBBR_2_2_GUID \
    { 0x9073eed4, 0xe50d, 0x11ee, \
    { 0xb8, 0xb0, 0x8b, 0x68, 0xda, 0x62, 0xfc, 0x80 }}

extern EFI_GUID  gEfiConfProfilesTableGuid;
extern EFI_GUID  gEfiConfProfilesUefiSpecGuid;

#endif
