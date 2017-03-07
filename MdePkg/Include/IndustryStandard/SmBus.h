/** @file
  This file declares the SMBus definitions defined in SmBus Specifciation V2.0
  and defined in PI1.0 specification volume 5.

  Copyright (c) 2007 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef _SMBUS_H_
#define _SMBUS_H_


///
/// UDID of SMBUS device.
///
typedef struct {
  UINT32  VendorSpecificId;
  UINT16  SubsystemDeviceId;
  UINT16  SubsystemVendorId;
  UINT16  Interface;
  UINT16  DeviceId;
  UINT16  VendorId;
  UINT8   VendorRevision;
  UINT8   DeviceCapabilities;
} EFI_SMBUS_UDID;

///
/// Smbus Device Address
///
typedef struct {
  ///
  /// The SMBUS hardware address to which the SMBUS device is preassigned or allocated.
  ///
  UINTN SmbusDeviceAddress : 7;
} EFI_SMBUS_DEVICE_ADDRESS;

typedef struct {
  ///
  /// The SMBUS hardware address to which the SMBUS device is preassigned or
  /// allocated. Type EFI_SMBUS_DEVICE_ADDRESS is defined in EFI_PEI_SMBUS2_PPI.Execute().
  ///
  EFI_SMBUS_DEVICE_ADDRESS  SmbusDeviceAddress;
  ///
  /// The SMBUS Unique Device Identifier (UDID) as defined in EFI_SMBUS_UDID.
  /// Type EFI_SMBUS_UDID is defined in EFI_PEI_SMBUS2_PPI.ArpDevice().
  ///
  EFI_SMBUS_UDID            SmbusDeviceUdid;
} EFI_SMBUS_DEVICE_MAP;

///
/// Smbus Operations
///
typedef enum _EFI_SMBUS_OPERATION {
  EfiSmbusQuickRead,
  EfiSmbusQuickWrite,
  EfiSmbusReceiveByte,
  EfiSmbusSendByte,
  EfiSmbusReadByte,
  EfiSmbusWriteByte,
  EfiSmbusReadWord,
  EfiSmbusWriteWord,
  EfiSmbusReadBlock,
  EfiSmbusWriteBlock,
  EfiSmbusProcessCall,
  EfiSmbusBWBRProcessCall
} EFI_SMBUS_OPERATION;

///
/// EFI_SMBUS_DEVICE_COMMAND
///
typedef UINTN   EFI_SMBUS_DEVICE_COMMAND;

#endif

