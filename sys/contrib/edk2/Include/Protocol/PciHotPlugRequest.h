/** @file
  Provides services to notify the PCI bus driver that some events have happened
  in a hot-plug controller (such as a PC Card socket, or PHPC), and to ask the
  PCI bus driver to create or destroy handles for PCI-like devices.

  A hot-plug capable PCI bus driver should produce the EFI PCI Hot Plug Request
  protocol. When a PCI device or a PCI-like device (for example, 32-bit PC Card)
  is installed after PCI bus does the enumeration, the PCI bus driver can be
  notified through this protocol. For example, when a 32-bit PC Card is inserted
  into the PC Card socket, the PC Card bus driver can call interface of this
  protocol to notify PCI bus driver to allocate resource and create handles for
  this PC Card.

  The EFI_PCI_HOTPLUG_REQUEST_PROTOCOL is installed by the PCI bus driver on a
  separate handle when PCI bus driver starts up. There is only one instance in
  the system.  Any driver that wants to use this protocol must locate it globally.
  The EFI_PCI_HOTPLUG_REQUEST_PROTOCOL allows the driver of hot-plug controller,
  for example, PC Card Bus driver, to notify PCI bus driver that an event has
  happened in the hot-plug controller, and the PCI bus driver is requested to
  create (add) or destroy (remove) handles for the specified PCI-like devices.
  For example, when a 32-bit PC Card is inserted, this protocol interface will
  be called with an add operation, and the PCI bus driver will enumerate and
  start the devices inserted; when a 32-bit PC Card is removed, this protocol
  interface will be called with a remove operation, and the PCI bus driver will
  stop the devices and destroy their handles.  The existence of this protocol
  represents the capability of the PCI bus driver. If this protocol exists in
  system, it means PCI bus driver is hot-plug capable, thus together with the
  effort of PC Card bus driver, hot-plug of PC Card can be supported. Otherwise,
  the hot-plug capability is not provided.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is defined in UEFI Platform Initialization Specification 1.2
  Volume 5: Standards

**/

#ifndef __PCI_HOTPLUG_REQUEST_H_
#define __PCI_HOTPLUG_REQUEST_H_

///
/// Global ID for EFI_PCI_HOTPLUG_REQUEST_PROTOCOL
///
#define EFI_PCI_HOTPLUG_REQUEST_PROTOCOL_GUID \
  { \
    0x19cb87ab, 0x2cb9, 0x4665, {0x83, 0x60, 0xdd, 0xcf, 0x60, 0x54, 0xf7, 0x9d} \
  }

///
/// Forward declaration for EFI_PCI_HOTPLUG_REQUEST_PROTOCOL
///
typedef struct _EFI_PCI_HOTPLUG_REQUEST_PROTOCOL EFI_PCI_HOTPLUG_REQUEST_PROTOCOL;

///
/// Enumeration of PCI hot plug operations
///
typedef enum {
  ///
  /// The PCI bus driver is requested to create handles for the specified devices.
  /// An array of EFI_HANDLE is returned, with a NULL element marking the end of
  /// the array.
  ///
  EfiPciHotPlugRequestAdd,

  ///
  /// The PCI bus driver is requested to destroy handles for the specified devices.
  ///
  EfiPciHotplugRequestRemove
} EFI_PCI_HOTPLUG_OPERATION;

/**
  This function is used to notify PCI bus driver that some events happened in a
  hot-plug controller, and the PCI bus driver is requested to start or stop
  specified PCI-like devices.

  This function allows the PCI bus driver to be notified to act as requested when
  a hot-plug event has happened on the hot-plug controller. Currently, the
  operations include add operation and remove operation.  If it is a add operation,
  the PCI bus driver will enumerate, allocate resources for devices behind the
  hot-plug controller, and create handle for the device specified by RemainingDevicePath.
  The RemainingDevicePath is an optional parameter. If it is not NULL, only the
  specified device is started; if it is NULL, all devices behind the hot-plug
  controller are started.  The newly created handles of PC Card functions are
  returned in the ChildHandleBuffer, together with the number of child handle in
  NumberOfChildren.  If it is a remove operation, when NumberOfChildren contains
  a non-zero value, child handles specified in ChildHandleBuffer are stopped and
  destroyed; otherwise, PCI bus driver is notified to stop managing the controller
  handle.

    @param[in] This                    A pointer to the EFI_PCI_HOTPLUG_REQUEST_PROTOCOL
                                       instance.
    @param[in] Operation               The operation the PCI bus driver is requested
                                       to make.
    @param[in] Controller              The handle of the hot-plug controller.
    @param[in] RemainingDevicePath     The remaining device path for the PCI-like
                                       hot-plug device.  It only contains device
                                       path nodes behind the hot-plug controller.
                                       It is an optional parameter and only valid
                                       when the Operation is a add operation. If
                                       it is NULL, all devices behind the PC Card
                                       socket are started.
    @param[in,out] NumberOfChildren    The number of child handles. For an add
                                       operation, it is an output parameter.  For
                                       a remove operation, it's an input parameter.
                                       When it contains a non-zero value, children
                                       handles specified in ChildHandleBuffer are
                                       destroyed.  Otherwise, PCI bus driver is
                                       notified to stop managing the controller
                                       handle.
    @param[in,out] ChildHandleBuffer   The buffer which contains the child handles.
                                       For an add operation, it is an output
                                       parameter and contains all newly created
                                       child handles.  For a remove operation, it
                                       contains child handles to be destroyed when
                                       NumberOfChildren contains a non-zero value.
                                       It can be NULL when NumberOfChildren is 0.
                                       It's the caller's responsibility to allocate
                                       and free memory for this buffer.

  @retval EFI_SUCCESS             The handles for the specified device have been
                                  created or destroyed as requested, and for an
                                  add operation, the new handles are returned in
                                  ChildHandleBuffer.
  @retval EFI_INVALID_PARAMETER   Operation is not a legal value.
  @retval EFI_INVALID_PARAMETER   Controller is NULL or not a valid handle.
  @retval EFI_INVALID_PARAMETER   NumberOfChildren is NULL.
  @retval EFI_INVALID_PARAMETER   ChildHandleBuffer is NULL while Operation is
                                  remove and NumberOfChildren contains a non-zero
                                  value.
  @retval EFI_INVALID_PARAMETER   ChildHandleBuffer is NULL while Operation is add.
  @retval EFI_OUT_OF_RESOURCES    There are no enough resources to start the
                                  devices.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PCI_HOTPLUG_REQUEST_NOTIFY)(
  IN     EFI_PCI_HOTPLUG_REQUEST_PROTOCOL  *This,
  IN     EFI_PCI_HOTPLUG_OPERATION         Operation,
  IN     EFI_HANDLE                        Controller,
  IN     EFI_DEVICE_PATH_PROTOCOL          *RemainingDevicePath  OPTIONAL,
  IN OUT UINT8                             *NumberOfChildren,
  IN OUT EFI_HANDLE                        *ChildHandleBuffer
  );

///
/// Provides services to notify PCI bus driver that some events have happened in
/// a hot-plug controller (for example, PC Card socket, or PHPC), and ask PCI bus
/// driver to create or destroy handles for the PCI-like devices.
///
struct _EFI_PCI_HOTPLUG_REQUEST_PROTOCOL {
  ///
  /// Notify the PCI bus driver that some events have happened in a hot-plug
  /// controller (for example, PC Card socket, or PHPC), and ask PCI bus driver
  /// to create or destroy handles for the PCI-like devices. See Section 0 for
  /// a detailed description.
  ///
  EFI_PCI_HOTPLUG_REQUEST_NOTIFY    Notify;
};

extern EFI_GUID  gEfiPciHotPlugRequestProtocolGuid;

#endif
