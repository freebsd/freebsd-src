/** @file
Guid & data structure for EFI_RT _PROPERTIES_TABLE, designed to be published by a
platform if it no longer  supports all EFI runtime services once ExitBootServices()
has been called by the OS. Introduced in UEFI 2.8a.


Copyright (c) 2020, American Megatrends International LLC. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __RT_PROPERTIES_TABLE_GUID_H__
#define __RT_PROPERTIES_TABLE_GUID_H__

//
// Table, defined here, should be published by a platform if it no longer supports all EFI runtime
// services once ExitBootServices() has been called by the OS. Note that this is merely a hint
// to the OS, which it is free to ignore, and so the platform is still required to provide callable
// implementations of unsupported runtime services that simply return EFI_UNSUPPORTED.
//
#define EFI_RT_PROPERTIES_TABLE_GUID \
    { 0xeb66918a, 0x7eef, 0x402a, \
    { 0x84, 0x2e, 0x93, 0x1d, 0x21, 0xc3, 0x8a, 0xe9 }}




#pragma pack(1)

typedef struct {
  ///
  /// Version of the structure, must be 0x1.
  ///
  UINT16 Version;

  ///
  /// Size in bytes of the entire EFI_RT_PROPERTIES_TABLE, must be 8.
  ///
  UINT16 Length;

  ///
  /// Bitmask of which calls are or are not supported, where a bit set to 1 indicates
  /// that the call is supported, and 0 indicates that it is not.
  ///
  UINT32 RuntimeServicesSupported;
} EFI_RT_PROPERTIES_TABLE;

#pragma pack()

#define EFI_RT_PROPERTIES_TABLE_VERSION 0x1

#define EFI_RT_SUPPORTED_GET_TIME                       0x0001
#define EFI_RT_SUPPORTED_SET_TIME                       0x0002
#define EFI_RT_SUPPORTED_GET_WAKEUP_TIME                0x0004
#define EFI_RT_SUPPORTED_SET_WAKEUP_TIME                0x0008
#define EFI_RT_SUPPORTED_GET_VARIABLE                   0x0010
#define EFI_RT_SUPPORTED_GET_NEXT_VARIABLE_NAME         0x0020
#define EFI_RT_SUPPORTED_SET_VARIABLE                   0x0040
#define EFI_RT_SUPPORTED_SET_VIRTUAL_ADDRESS_MAP        0x0080
#define EFI_RT_SUPPORTED_CONVERT_POINTER                0x0100
#define EFI_RT_SUPPORTED_GET_NEXT_HIGH_MONOTONIC_COUNT  0x0200
#define EFI_RT_SUPPORTED_RESET_SYSTEM                   0x0400
#define EFI_RT_SUPPORTED_UPDATE_CAPSULE                 0x0800
#define EFI_RT_SUPPORTED_QUERY_CAPSULE_CAPABILITIES     0x1000
#define EFI_RT_SUPPORTED_QUERY_VARIABLE_INFO            0x2000

extern EFI_GUID gEfiRtPropertiesTableGuid;

#endif
