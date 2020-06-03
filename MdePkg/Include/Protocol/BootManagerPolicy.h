/** @file
  Boot Manager Policy Protocol as defined in UEFI Specification.

  This protocol is used by EFI Applications to request the UEFI Boot Manager
  to connect devices using platform policy.

  Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __BOOT_MANAGER_POLICY_H__
#define __BOOT_MANAGER_POLICY_H__

#define EFI_BOOT_MANAGER_POLICY_PROTOCOL_GUID \
  { \
    0xFEDF8E0C, 0xE147, 0x11E3, { 0x99, 0x03, 0xB8, 0xE8, 0x56, 0x2C, 0xBA, 0xFA } \
  }

#define EFI_BOOT_MANAGER_POLICY_CONSOLE_GUID \
  { \
    0xCAB0E94C, 0xE15F, 0x11E3, { 0x91, 0x8D, 0xB8, 0xE8, 0x56, 0x2C, 0xBA, 0xFA } \
  }

#define EFI_BOOT_MANAGER_POLICY_NETWORK_GUID \
  { \
    0xD04159DC, 0xE15F, 0x11E3, { 0xB2, 0x61, 0xB8, 0xE8, 0x56, 0x2C, 0xBA, 0xFA } \
  }

#define EFI_BOOT_MANAGER_POLICY_CONNECT_ALL_GUID \
  { \
    0x113B2126, 0xFC8A, 0x11E3, { 0xBD, 0x6C, 0xB8, 0xE8, 0x56, 0x2C, 0xBA, 0xFA } \
  }

typedef struct _EFI_BOOT_MANAGER_POLICY_PROTOCOL EFI_BOOT_MANAGER_POLICY_PROTOCOL;

#define EFI_BOOT_MANAGER_POLICY_PROTOCOL_REVISION 0x00010000

/**
  Connect a device path following the platforms EFI Boot Manager policy.

  The ConnectDevicePath() function allows the caller to connect a DevicePath using the
  same policy as the EFI Boot Manger.

  @param[in] This       A pointer to the EFI_BOOT_MANAGER_POLICY_PROTOCOL instance.
  @param[in] DevicePath Points to the start of the EFI device path to connect.
                        If DevicePath is NULL then all the controllers in the
                        system will be connected using the platforms EFI Boot
                        Manager policy.
  @param[in] Recursive  If TRUE, then ConnectController() is called recursively
                        until the entire tree of controllers below the
                        controller specified by DevicePath have been created.
                        If FALSE, then the tree of controllers is only expanded
                        one level. If DevicePath is NULL then Recursive is ignored.

  @retval EFI_SUCCESS            The DevicePath was connected.
  @retval EFI_NOT_FOUND          The DevicePath was not found.
  @retval EFI_NOT_FOUND          No driver was connected to DevicePath.
  @retval EFI_SECURITY_VIOLATION The user has no permission to start UEFI device
                                 drivers on the DevicePath.
  @retval EFI_UNSUPPORTED        The current TPL is not TPL_APPLICATION.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_BOOT_MANAGER_POLICY_CONNECT_DEVICE_PATH)(
  IN EFI_BOOT_MANAGER_POLICY_PROTOCOL *This,
  IN EFI_DEVICE_PATH                  *DevicePath,
  IN BOOLEAN                          Recursive
  );

/**
  Connect a class of devices using the platform Boot Manager policy.

  The ConnectDeviceClass() function allows the caller to request that the Boot
  Manager connect a class of devices.

  If Class is EFI_BOOT_MANAGER_POLICY_CONSOLE_GUID then the Boot Manager will
  use platform policy to connect consoles. Some platforms may restrict the
  number of consoles connected as they attempt to fast boot, and calling
  ConnectDeviceClass() with a Class value of EFI_BOOT_MANAGER_POLICY_CONSOLE_GUID
  must connect the set of consoles that follow the Boot Manager platform policy,
  and the EFI_SIMPLE_TEXT_INPUT_PROTOCOL, EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL, and
  the EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL are produced on the connected handles.
  The Boot Manager may restrict which consoles get connect due to platform policy,
  for example a security policy may require that a given console is not connected.

  If Class is EFI_BOOT_MANAGER_POLICY_NETWORK_GUID then the Boot Manager will
  connect the protocols the platforms supports for UEFI general purpose network
  applications on one or more handles. If more than one network controller is
  available a platform will connect, one, many, or all of the networks based
  on platform policy. Connecting UEFI networking protocols, like EFI_DHCP4_PROTOCOL,
  does not establish connections on the network. The UEFI general purpose network
  application that called ConnectDeviceClass() may need to use the published
  protocols to establish the network connection. The Boot Manager can optionally
  have a policy to establish a network connection.

  If Class is EFI_BOOT_MANAGER_POLICY_CONNECT_ALL_GUID then the Boot Manager
  will connect all UEFI drivers using the UEFI Boot Service
  EFI_BOOT_SERVICES.ConnectController(). If the Boot Manager has policy
  associated with connect all UEFI drivers this policy will be used.

  A platform can also define platform specific Class values as a properly generated
  EFI_GUID would never conflict with this specification.

  @param[in] This  A pointer to the EFI_BOOT_MANAGER_POLICY_PROTOCOL instance.
  @param[in] Class A pointer to an EFI_GUID that represents a class of devices
                   that will be connected using the Boot Mangers platform policy.

  @retval EFI_SUCCESS      At least one devices of the Class was connected.
  @retval EFI_DEVICE_ERROR Devices were not connected due to an error.
  @retval EFI_NOT_FOUND    The Class is not supported by the platform.
  @retval EFI_UNSUPPORTED  The current TPL is not TPL_APPLICATION.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_BOOT_MANAGER_POLICY_CONNECT_DEVICE_CLASS)(
  IN EFI_BOOT_MANAGER_POLICY_PROTOCOL *This,
  IN EFI_GUID                         *Class
  );

struct _EFI_BOOT_MANAGER_POLICY_PROTOCOL {
  UINT64                                       Revision;
  EFI_BOOT_MANAGER_POLICY_CONNECT_DEVICE_PATH  ConnectDevicePath;
  EFI_BOOT_MANAGER_POLICY_CONNECT_DEVICE_CLASS ConnectDeviceClass;
};

extern EFI_GUID gEfiBootManagerPolicyProtocolGuid;

extern EFI_GUID gEfiBootManagerPolicyConsoleGuid;
extern EFI_GUID gEfiBootManagerPolicyNetworkGuid;
extern EFI_GUID gEfiBootManagerPolicyConnectAllGuid;

#endif
