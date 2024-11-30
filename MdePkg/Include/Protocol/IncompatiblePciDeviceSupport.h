/** @file
  This file declares Incompatible PCI Device Support Protocol

  Allows the PCI bus driver to support resource allocation for some PCI devices
  that do not comply with the PCI Specification.

  @par Note:
    This protocol is optional. Only those platforms that implement this protocol
    will have the capability to support incompatible PCI devices. The absence of
    this protocol can cause the PCI bus driver to configure these incompatible
    PCI devices incorrectly. As a result, these devices may not work properly.

  The EFI_INCOMPATIBLE_PCI_DEVICE_SUPPORT_PROTOCOL is used by the PCI bus driver
  to support resource allocation for some PCI devices that do not comply with the
  PCI Specification.  This protocol can find some incompatible PCI devices and
  report their special resource requirements to the PCI bus driver. The generic
  PCI bus driver does not have prior knowledge of any incompatible PCI devices.
  It interfaces with the EFI_INCOMPATIBLE_PCI_DEVICE_SUPPORT_PROTOCOL to find out
  if a device is incompatible and to obtain the special configuration requirements
  for a specific incompatible PCI device.

  This protocol is optional, and only one instance of this protocol can be present
  in the system. If a platform supports this protocol, this protocol is produced
  by a Driver Execution Environment (DXE) driver and must be made available before
  the Boot Device Selection (BDS) phase. The PCI bus driver will look for the
  presence of this protocol before it begins PCI enumeration.  If this protocol
  exists in a platform, it indicates that the platform has the capability to support
  those incompatible PCI devices. However, final support for incompatible PCI
  devices still depends on the implementation of the PCI bus driver. The PCI bus
  driver may fully, partially, or not even support these incompatible devices.

  During PCI bus enumeration, the PCI bus driver will probe the PCI Base Address
  Registers (BARs) for each PCI device regardless of whether the PCI device is
  incompatible or not to determine the resource requirements so that the PCI bus
  driver can invoke the proper PCI resources for them.  Generally, this resource
  information includes the following:
    - Resource type
    - Resource length
    - Alignment

  However, some incompatible PCI devices may have special requirements. As a result,
  the length or the alignment that is derived through BAR probing may not be exactly
  the same as the actual resource requirement of the device. For example, there
  are some devices that request I/O resources at a length of 0x100 from their I/O
  BAR, but these incompatible devices will never work correctly if an odd I/O base
  address, such as 0x100, 0x300, or 0x500, is assigned to the BAR. Instead, these
  devices request an even base address, such as 0x200 or 0x400. The Incompatible
  PCI Device Support Protocol can then be used to obtain these special resource
  requirements for these incompatible PCI devices. In this way, the PCI bus driver
  will take special consideration for these devices during PCI resource allocation
  to ensure that they can work correctly.

  This protocol may support the following incompatible PCI BAR types:
    - I/O or memory length that is different from what the BAR reports
    - I/O or memory alignment that is different from what the BAR reports
    - Fixed I/O or memory base address

  See the Conventional PCI Specification 3.0 for the details of how a PCI BAR
  reports the resource length and the alignment that it requires.

  Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is defined in UEFI Platform Initialization Specification 1.2
  Volume 5: Standards

**/

#ifndef _INCOMPATIBLE_PCI_DEVICE_SUPPORT_H_
#define _INCOMPATIBLE_PCI_DEVICE_SUPPORT_H_

///
/// Global ID for EFI_INCOMPATIBLE_PCI_DEVICE_SUPPORT_PROTOCOL
///
#define EFI_INCOMPATIBLE_PCI_DEVICE_SUPPORT_PROTOCOL_GUID \
  { \
    0xeb23f55a, 0x7863, 0x4ac2, {0x8d, 0x3d, 0x95, 0x65, 0x35, 0xde, 0x03, 0x75} \
  }

///
/// Forward declaration for EFI_INCOMPATIBLE_PCI_DEVICE_SUPPORT_PROTOCOL
///
typedef struct _EFI_INCOMPATIBLE_PCI_DEVICE_SUPPORT_PROTOCOL EFI_INCOMPATIBLE_PCI_DEVICE_SUPPORT_PROTOCOL;

/**
  Returns a list of ACPI resource descriptors that detail the special resource
  configuration requirements for an incompatible PCI device.

  This function returns a list of ACPI resource descriptors that detail the
  special resource configuration requirements for an incompatible PCI device.

  Prior to bus enumeration, the PCI bus driver will look for the presence
  of the EFI_INCOMPATIBLE_PCI_DEVICE_SUPPORT_PROTOCOL. Only one instance of this
  protocol can be present in the system. For each PCI device that the PCI bus
  driver discovers, the PCI bus driver calls this function with the device's vendor
  ID, device ID, revision ID, subsystem vendor ID, and subsystem device ID. If the
  VendorId, DeviceId, RevisionId, SubsystemVendorId, or SubsystemDeviceId value is
  set to (UINTN)-1, that field will be ignored. The ID values that are not (UINTN)-1
  will be used to identify the current device.

  This function will only return EFI_SUCCESS. However, if the device is an
  incompatible PCI device, a list of ACPI resource descriptors will be returned
  in Configuration. Otherwise, NULL will be returned in Configuration instead.
  The PCI bus driver does not need to allocate memory for Configuration. However,
  it is the PCI bus driver's responsibility to free it. The PCI bus driver then
  can configure this device with the information that is derived from this list
  of resource nodes, rather than the result of BAR probing.

  Only the following two resource descriptor types from the ACPI Specification
  may be used to describe the incompatible PCI device resource requirements:
    - QWORD Address Space Descriptor (ACPI 2.0, section 6.4.3.5.1; also ACPI 3.0)
    - End Tag (ACPI 2.0, section 6.4.2.8; also ACPI 3.0)

  The QWORD Address Space Descriptor can describe memory, I/O, and bus number
  ranges for dynamic or fixed resources.  The configuration of a PCI root bridge
  is described with one or more QWORD Address Space Descriptors, followed by an
  End Tag. See the ACPI Specification for details on the field values.

  @param[in]  This                Pointer to the EFI_INCOMPATIBLE_PCI_DEVICE_SUPPORT_PROTOCOL
                                  instance.
  @param[in]  VendorId            A unique ID to identify the manufacturer of
                                  the PCI device.  See the Conventional PCI
                                  Specification 3.0 for details.
  @param[in]  DeviceId            A unique ID to identify the particular PCI
                                  device. See the Conventional PCI Specification
                                  3.0 for details.
  @param[in]  RevisionId          A PCI device-specific revision identifier.
                                  See the Conventional PCI Specification 3.0
                                  for details.
  @param[in]  SubsystemVendorId   Specifies the subsystem vendor ID. See the
                                  Conventional PCI Specification 3.0 for details.
  @param[in]  SubsystemDeviceId   Specifies the subsystem device ID. See the
                                  Conventional PCI Specification 3.0 for details.
  @param[out] Configuration       A list of ACPI resource descriptors that detail
                                  the configuration requirement.

  @retval EFI_SUCCESS   The function always returns EFI_SUCCESS.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_INCOMPATIBLE_PCI_DEVICE_SUPPORT_CHECK_DEVICE)(
  IN  EFI_INCOMPATIBLE_PCI_DEVICE_SUPPORT_PROTOCOL  *This,
  IN  UINTN                                         VendorId,
  IN  UINTN                                         DeviceId,
  IN  UINTN                                         RevisionId,
  IN  UINTN                                         SubsystemVendorId,
  IN  UINTN                                         SubsystemDeviceId,
  OUT VOID                                          **Configuration
  );

///
/// Interface structure for the Incompatible PCI Device Support Protocol
///
struct _EFI_INCOMPATIBLE_PCI_DEVICE_SUPPORT_PROTOCOL {
  ///
  ///  Returns a list of ACPI resource descriptors that detail any special
  ///  resource configuration requirements if the specified device is a recognized
  ///  incompatible PCI device.
  ///
  EFI_INCOMPATIBLE_PCI_DEVICE_SUPPORT_CHECK_DEVICE    CheckDevice;
};

extern EFI_GUID  gEfiIncompatiblePciDeviceSupportProtocolGuid;

#endif
