/** @file
Guid & data structure for tables defined for reporting firmware configuration data to EFI
Configuration Tables and also for processing JSON payload capsule.


Copyright (c) 2020, American Megatrends International LLC. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __JSON_CAPSULE_GUID_H__
#define __JSON_CAPSULE_GUID_H__

//
// The address reported in the table entry identified by EFI_JSON_CAPSULE_DATA_TABLE_GUID will be
// referenced as physical and will not be fixed up when transition from preboot to runtime phase. The
// addresses reported in these table entries identified by EFI_JSON_CONFIG_DATA_TABLE_GUID and
// EFI_JSON_CAPSULE_RESULT_TABLE_GUID will be referenced as virtual and will be fixed up when
// transition from preboot to runtime phase.
//
#define EFI_JSON_CONFIG_DATA_TABLE_GUID \
    {0x87367f87, 0x1119, 0x41ce, \
    {0xaa, 0xec, 0x8b, 0xe0, 0x11, 0x1f, 0x55, 0x8a }}
#define EFI_JSON_CAPSULE_DATA_TABLE_GUID \
    {0x35e7a725, 0x8dd2, 0x4cac, \
    {0x80, 0x11, 0x33, 0xcd, 0xa8, 0x10, 0x90, 0x56 }}
#define EFI_JSON_CAPSULE_RESULT_TABLE_GUID \
    {0xdbc461c3, 0xb3de, 0x422a,\
    {0xb9, 0xb4, 0x98, 0x86, 0xfd, 0x49, 0xa1, 0xe5 }}
#define EFI_JSON_CAPSULE_ID_GUID \
    {0x67d6f4cd, 0xd6b8,  0x4573, \
    {0xbf, 0x4a, 0xde, 0x5e, 0x25, 0x2d, 0x61, 0xae }}

#pragma pack(1)

typedef struct {
  ///
  /// Version of the structure, initially 0x00000001.
  ///
  UINT32    Version;

  ///
  /// The unique identifier of this capsule.
  ///
  UINT32    CapsuleId;

  ///
  /// The length of the JSON payload immediately following this header, in bytes.
  ///
  UINT32    PayloadLength;

  ///
  /// Variable length buffer containing the JSON payload that should be parsed and applied to the system. The
  /// definition of the JSON schema used in the payload is beyond the scope of this specification.
  ///
  UINT8     Payload[];
} EFI_JSON_CAPSULE_HEADER;

typedef struct {
  ///
  /// The length of the following ConfigData, in bytes.
  ///
  UINT32    ConfigDataLength;

  ///
  /// Variable length buffer containing the JSON payload that describes one group of configuration data within
  /// current system. The definition of the JSON schema used in this payload is beyond the scope of this specification.
  ///
  UINT8     ConfigData[];
} EFI_JSON_CONFIG_DATA_ITEM;

typedef struct {
  ///
  /// Version of the structure, initially 0x00000001.
  ///
  UINT32                       Version;

  ///
  ////The total length of EFI_JSON_CAPSULE_CONFIG_DATA, in bytes.
  ///
  UINT32                       TotalLength;

  ///
  /// Array of configuration data groups.
  ///
  EFI_JSON_CONFIG_DATA_ITEM    ConfigDataList[];
} EFI_JSON_CAPSULE_CONFIG_DATA;

#pragma pack()

extern EFI_GUID  gEfiJsonConfigDataTableGuid;
extern EFI_GUID  gEfiJsonCapsuleDataTableGuid;
extern EFI_GUID  gEfiJsonCapsuleResultTableGuid;
extern EFI_GUID  gEfiJsonCapsuleIdGuid;

#endif
