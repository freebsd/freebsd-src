/** @file
  EFI Network Interface Identifier Protocol.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in EFI Specification 1.10.

**/

#ifndef __EFI_NETWORK_INTERFACE_IDENTIFER_H__
#define __EFI_NETWORK_INTERFACE_IDENTIFER_H__

//
// GUID retired from UEFI Specification 2.1b
//
#define EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL_GUID \
  { \
    0xE18541CD, 0xF755, 0x4f73, {0x92, 0x8D, 0x64, 0x3C, 0x8A, 0x79, 0xB2, 0x29 } \
  }

//
// GUID intruduced in UEFI Specification 2.1b
//
#define EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL_GUID_31 \
  { \
    0x1ACED566, 0x76ED, 0x4218, {0xBC, 0x81, 0x76, 0x7F, 0x1F, 0x97, 0x7A, 0x89 } \
  }

//
// Revision defined in UEFI Specification 2.4
//
#define EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL_REVISION    0x00020000


///
/// Revision defined in EFI1.1.
///
#define EFI_NETWORK_INTERFACE_IDENTIFIER_INTERFACE_REVISION   EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL_REVISION

///
/// Forward reference for pure ANSI compatability.
///
typedef struct _EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL  EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL;

///
/// Protocol defined in EFI1.1.
///
typedef EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL   EFI_NETWORK_INTERFACE_IDENTIFIER_INTERFACE;

///
/// An optional protocol that is used to describe details about the software
/// layer that is used to produce the Simple Network Protocol.
///
struct _EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL {
  UINT64    Revision;   ///< The revision of the EFI_NETWORK_INTERFACE_IDENTIFIER protocol.
  UINT64    Id;         ///< The address of the first byte of the identifying structure for this network
                        ///< interface. This is only valid when the network interface is started
                        ///< (see Start()). When the network interface is not started, this field is set to zero.
  UINT64    ImageAddr;  ///< The address of the first byte of the identifying structure for this
                        ///< network interface.  This is set to zero if there is no structure.
  UINT32    ImageSize;  ///< The size of unrelocated network interface image.
  CHAR8     StringId[4];///< A four-character ASCII string that is sent in the class identifier field of
                        ///< option 60 in DHCP. For a Type of EfiNetworkInterfaceUndi, this field is UNDI.
  UINT8     Type;       ///< Network interface type. This will be set to one of the values
                        ///< in EFI_NETWORK_INTERFACE_TYPE.
  UINT8     MajorVer;   ///< Major version number.
  UINT8     MinorVer;   ///< Minor version number.
  BOOLEAN   Ipv6Supported; ///< TRUE if the network interface supports IPv6; otherwise FALSE.
  UINT16    IfNum;      ///< The network interface number that is being identified by this Network
                        ///< Interface Identifier Protocol. This field must be less than or
                        ///< equal to the (IFcnt | IFcntExt <<8 ) fields in the !PXE structure.

};

///
///*******************************************************
/// EFI_NETWORK_INTERFACE_TYPE
///*******************************************************
///
typedef enum {
  EfiNetworkInterfaceUndi = 1
} EFI_NETWORK_INTERFACE_TYPE;

///
/// Forward reference for pure ANSI compatability.
///
typedef struct undiconfig_table  UNDI_CONFIG_TABLE;

///
/// The format of the configuration table for UNDI
///
struct undiconfig_table {
  UINT32             NumberOfInterfaces;    ///< The number of NIC devices
                                            ///< that this UNDI controls.
  UINT32             reserved;
  UNDI_CONFIG_TABLE  *nextlink;             ///< A pointer to the next UNDI
                                            ///< configuration table.
  ///
  /// The length of this array is given in the NumberOfInterfaces field.
  ///
  struct {
    VOID             *NII_InterfacePointer; ///< Pointer to the NII interface structure.
    VOID             *DevicePathPointer;    ///< Pointer to the device path for this NIC.
  } NII_entry[1];
};

extern EFI_GUID gEfiNetworkInterfaceIdentifierProtocolGuid;
extern EFI_GUID gEfiNetworkInterfaceIdentifierProtocolGuid_31;

#endif
