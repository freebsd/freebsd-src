/** @file
  Guid & data structure used for Delivering Capsules Containing Updates to Firmware
  Management Protocol

  Copyright (c) 2013 - 2015, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  @par Revision Reference:
  GUIDs defined in UEFI 2.4 spec.

**/


#ifndef _FMP_CAPSULE_GUID_H__
#define _FMP_CAPSULE_GUID_H__

//
// This is the GUID of the capsule for Firmware Management Protocol.
//
#define EFI_FIRMWARE_MANAGEMENT_CAPSULE_ID_GUID \
  { \
    0x6dcbd5ed, 0xe82d, 0x4c44, {0xbd, 0xa1, 0x71, 0x94, 0x19, 0x9a, 0xd9, 0x2a } \
  }

#pragma pack(1)

typedef struct {
  UINT32 Version;

  ///
  /// The number of drivers included in the capsule and the number of corresponding
  /// offsets stored in ItemOffsetList array.
  ///
  UINT16 EmbeddedDriverCount;

  ///
  /// The number of payload items included in the capsule and the number of
  /// corresponding offsets stored in the ItemOffsetList array.
  ///
  UINT16 PayloadItemCount;

  ///
  /// Variable length array of dimension [EmbeddedDriverCount + PayloadItemCount]
  /// containing offsets of each of the drivers and payload items contained within the capsule
  ///
  // UINT64 ItemOffsetList[];
} EFI_FIRMWARE_MANAGEMENT_CAPSULE_HEADER;

typedef struct {
  UINT32   Version;

  ///
  /// Used to identify device firmware targeted by this update. This guid is matched by
  /// system firmware against ImageTypeId field within a EFI_FIRMWARE_IMAGE_DESCRIPTOR
  ///
  EFI_GUID UpdateImageTypeId;

  ///
  /// Passed as ImageIndex in call to EFI_FIRMWARE_MANAGEMENT_PROTOCOL.SetImage()
  ///
  UINT8    UpdateImageIndex;
  UINT8    reserved_bytes[3];

  ///
  /// Size of the binary update image which immediately follows this structure
  ///
  UINT32   UpdateImageSize;

  ///
  /// Size of the VendorCode bytes which optionally immediately follow binary update image in the capsule
  ///
  UINT32   UpdateVendorCodeSize;

  ///
  /// The HardwareInstance to target with this update. If value is zero it means match all
  /// HardwareInstances. This field allows update software to target only a single device in
  /// cases where there are more than one device with the same ImageTypeId GUID.
  /// This header is outside the signed data of the Authentication Info structure and
  /// therefore can be modified without changing the Auth data.
  ///
  UINT64   UpdateHardwareInstance;
} EFI_FIRMWARE_MANAGEMENT_CAPSULE_IMAGE_HEADER;

#pragma pack()


#define EFI_FIRMWARE_MANAGEMENT_CAPSULE_HEADER_INIT_VERSION       0x00000001
#define EFI_FIRMWARE_MANAGEMENT_CAPSULE_IMAGE_HEADER_INIT_VERSION 0x00000002

extern EFI_GUID gEfiFmpCapsuleGuid;

#endif
