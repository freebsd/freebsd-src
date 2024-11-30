/** @file

  The library provides the USB Standard Device Requests defined
  in Usb specification 9.4 section.

  Copyright (c) 2004 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UefiUsbLibInternal.h"

/**
  Get the descriptor of the specified USB device.

  Submit a USB get descriptor request for the USB device specified by UsbIo, Value,
  and Index, and return the descriptor in the buffer specified by Descriptor.
  The status of the transfer is returned in Status.
  If UsbIo is NULL, then ASSERT().
  If Descriptor is NULL, then ASSERT().
  If Status is NULL, then ASSERT().

  @param  UsbIo             A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Value             The device request value.
  @param  Index             The device request index.
  @param  DescriptorLength  The size, in bytes, of Descriptor.
  @param  Descriptor        A pointer to the descriptor buffer to get.
  @param  Status            A pointer to the status of the transfer.

  @retval EFI_SUCCESS           The request executed successfully.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed because the
                                buffer specified by DescriptorLength and Descriptor
                                is not large enough to hold the result of the request.
  @retval EFI_TIMEOUT           A timeout occurred executing the request.
  @retval EFI_DEVICE_ERROR      The request failed due to a device error. The transfer
                                status is returned in Status.

**/
EFI_STATUS
EFIAPI
UsbGetDescriptor (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  UINT16               Value,
  IN  UINT16               Index,
  IN  UINT16               DescriptorLength,
  OUT VOID                 *Descriptor,
  OUT UINT32               *Status
  )
{
  EFI_USB_DEVICE_REQUEST  DevReq;

  ASSERT (UsbIo != NULL);
  ASSERT (Descriptor != NULL);
  ASSERT (Status != NULL);

  ZeroMem (&DevReq, sizeof (EFI_USB_DEVICE_REQUEST));

  DevReq.RequestType = USB_DEV_GET_DESCRIPTOR_REQ_TYPE;
  DevReq.Request     = USB_REQ_GET_DESCRIPTOR;
  DevReq.Value       = Value;
  DevReq.Index       = Index;
  DevReq.Length      = DescriptorLength;

  return UsbIo->UsbControlTransfer (
                  UsbIo,
                  &DevReq,
                  EfiUsbDataIn,
                  PcdGet32 (PcdUsbTransferTimeoutValue),
                  Descriptor,
                  DescriptorLength,
                  Status
                  );
}

/**
  Set the descriptor of the specified USB device.

  Submit a USB set descriptor request for the USB device specified by UsbIo,
  Value, and Index, and set the descriptor using the buffer specified by DesriptorLength
  and Descriptor.  The status of the transfer is returned in Status.
  If UsbIo is NULL, then ASSERT().
  If Descriptor is NULL, then ASSERT().
  If Status is NULL, then ASSERT().

  @param  UsbIo             A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Value             The device request value.
  @param  Index             The device request index.
  @param  DescriptorLength  The size, in bytes, of Descriptor.
  @param  Descriptor        A pointer to the descriptor buffer to set.
  @param  Status            A pointer to the status of the transfer.

  @retval  EFI_SUCCESS       The request executed successfully.
  @retval  EFI_TIMEOUT       A timeout occurred executing the request.
  @retval  EFI_DEVICE_ERROR  The request failed due to a device error.
                             The transfer status is returned in Status.

**/
EFI_STATUS
EFIAPI
UsbSetDescriptor (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  UINT16               Value,
  IN  UINT16               Index,
  IN  UINT16               DescriptorLength,
  IN  VOID                 *Descriptor,
  OUT UINT32               *Status
  )
{
  EFI_USB_DEVICE_REQUEST  DevReq;

  ASSERT (UsbIo != NULL);
  ASSERT (Descriptor != NULL);
  ASSERT (Status != NULL);

  ZeroMem (&DevReq, sizeof (EFI_USB_DEVICE_REQUEST));

  DevReq.RequestType = USB_DEV_SET_DESCRIPTOR_REQ_TYPE;
  DevReq.Request     = USB_REQ_SET_DESCRIPTOR;
  DevReq.Value       = Value;
  DevReq.Index       = Index;
  DevReq.Length      = DescriptorLength;

  return UsbIo->UsbControlTransfer (
                  UsbIo,
                  &DevReq,
                  EfiUsbDataOut,
                  PcdGet32 (PcdUsbTransferTimeoutValue),
                  Descriptor,
                  DescriptorLength,
                  Status
                  );
}

/**
  Get the interface setting of the specified USB device.

  Submit a USB get interface request for the USB device specified by UsbIo,
  and Interface, and place the result in the buffer specified by AlternateSetting.
  The status of the transfer is returned in Status.
  If UsbIo is NULL, then ASSERT().
  If AlternateSetting is NULL, then ASSERT().
  If Status is NULL, then ASSERT().

  @param  UsbIo             A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Interface         The interface index value.
  @param  AlternateSetting  A pointer to the alternate setting to be retrieved.
  @param  Status            A pointer to the status of the transfer.

  @retval EFI_SUCCESS       The request executed successfully.
  @retval EFI_TIMEOUT       A timeout occurred executing the request.
  @retval EFI_DEVICE_ERROR  The request failed due to a device error.
                            The transfer status is returned in Status.

**/
EFI_STATUS
EFIAPI
UsbGetInterface (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  UINT16               Interface,
  OUT UINT16               *AlternateSetting,
  OUT UINT32               *Status
  )
{
  EFI_USB_DEVICE_REQUEST  DevReq;

  ASSERT (UsbIo != NULL);
  ASSERT (AlternateSetting != NULL);
  ASSERT (Status != NULL);

  *AlternateSetting = 0;

  ZeroMem (&DevReq, sizeof (EFI_USB_DEVICE_REQUEST));

  DevReq.RequestType = USB_DEV_GET_INTERFACE_REQ_TYPE;
  DevReq.Request     = USB_REQ_GET_INTERFACE;
  DevReq.Index       = Interface;
  DevReq.Length      = 1;

  return UsbIo->UsbControlTransfer (
                  UsbIo,
                  &DevReq,
                  EfiUsbDataIn,
                  PcdGet32 (PcdUsbTransferTimeoutValue),
                  AlternateSetting,
                  1,
                  Status
                  );
}

/**
  Set the interface setting of the specified USB device.

  Submit a USB set interface request for the USB device specified by UsbIo, and
  Interface, and set the alternate setting to the value specified by AlternateSetting.
  The status of the transfer is returned in Status.
  If UsbIo is NULL, then ASSERT().
  If Status is NULL, then ASSERT().

  @param  UsbIo             A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Interface         The interface index value.
  @param  AlternateSetting  The alternate setting to be set.
  @param  Status            A pointer to the status of the transfer.

  @retval EFI_SUCCESS  The request executed successfully.
  @retval EFI_TIMEOUT  A timeout occurred executing the request.
  @retval EFI_SUCCESS  The request failed due to a device error.
                       The transfer status is returned in Status.

**/
EFI_STATUS
EFIAPI
UsbSetInterface (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  UINT16               Interface,
  IN  UINT16               AlternateSetting,
  OUT UINT32               *Status
  )
{
  EFI_USB_DEVICE_REQUEST  DevReq;

  ASSERT (UsbIo != NULL);
  ASSERT (Status != NULL);

  ZeroMem (&DevReq, sizeof (EFI_USB_DEVICE_REQUEST));

  DevReq.RequestType = USB_DEV_SET_INTERFACE_REQ_TYPE;
  DevReq.Request     = USB_REQ_SET_INTERFACE;
  DevReq.Value       = AlternateSetting;
  DevReq.Index       = Interface;

  return UsbIo->UsbControlTransfer (
                  UsbIo,
                  &DevReq,
                  EfiUsbNoData,
                  PcdGet32 (PcdUsbTransferTimeoutValue),
                  NULL,
                  0,
                  Status
                  );
}

/**
  Get the device configuration.

  Submit a USB get configuration request for the USB device specified by UsbIo
  and place the result in the buffer specified by ConfigurationValue. The status
  of the transfer is returned in Status.
  If UsbIo is NULL, then ASSERT().
  If ConfigurationValue is NULL, then ASSERT().
  If Status is NULL, then ASSERT().

  @param  UsbIo               A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  ConfigurationValue  A pointer to the device configuration to be retrieved.
  @param  Status              A pointer to the status of the transfer.

  @retval EFI_SUCCESS        The request executed successfully.
  @retval EFI_TIMEOUT        A timeout occurred executing the request.
  @retval EFI_DEVICE_ERROR   The request failed due to a device error.
                             The transfer status is returned in Status.

**/
EFI_STATUS
EFIAPI
UsbGetConfiguration (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  OUT UINT16               *ConfigurationValue,
  OUT UINT32               *Status
  )
{
  EFI_USB_DEVICE_REQUEST  DevReq;

  ASSERT (UsbIo != NULL);
  ASSERT (ConfigurationValue != NULL);
  ASSERT (Status != NULL);

  *ConfigurationValue = 0;

  ZeroMem (&DevReq, sizeof (EFI_USB_DEVICE_REQUEST));

  DevReq.RequestType = USB_DEV_GET_CONFIGURATION_REQ_TYPE;
  DevReq.Request     = USB_REQ_GET_CONFIG;
  DevReq.Length      = 1;

  return UsbIo->UsbControlTransfer (
                  UsbIo,
                  &DevReq,
                  EfiUsbDataIn,
                  PcdGet32 (PcdUsbTransferTimeoutValue),
                  ConfigurationValue,
                  1,
                  Status
                  );
}

/**
  Set the device configuration.

  Submit a USB set configuration request for the USB device specified by UsbIo
  and set the device configuration to the value specified by ConfigurationValue.
  The status of the transfer is returned in Status.
  If UsbIo is NULL, then ASSERT().
  If Status is NULL, then ASSERT().

  @param  UsbIo               A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  ConfigurationValue  The device configuration value to be set.
  @param  Status              A pointer to the status of the transfer.

  @retval EFI_SUCCESS       The request executed successfully.
  @retval EFI_TIMEOUT       A timeout occurred executing the request.
  @retval EFI_DEVICE_ERROR  The request failed due to a device error.
                            The transfer status is returned in Status.

**/
EFI_STATUS
EFIAPI
UsbSetConfiguration (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  UINT16               ConfigurationValue,
  OUT UINT32               *Status
  )
{
  EFI_USB_DEVICE_REQUEST  DevReq;

  ASSERT (UsbIo != NULL);
  ASSERT (Status != NULL);

  ZeroMem (&DevReq, sizeof (EFI_USB_DEVICE_REQUEST));

  DevReq.RequestType = USB_DEV_SET_CONFIGURATION_REQ_TYPE;
  DevReq.Request     = USB_REQ_SET_CONFIG;
  DevReq.Value       = ConfigurationValue;

  return UsbIo->UsbControlTransfer (
                  UsbIo,
                  &DevReq,
                  EfiUsbNoData,
                  PcdGet32 (PcdUsbTransferTimeoutValue),
                  NULL,
                  0,
                  Status
                  );
}

/**
  Set the specified feature of the specified device.

  Submit a USB set device feature request for the USB device specified by UsbIo,
  Recipient, and Target to the value specified by Value.  The status of the
  transfer is returned in Status.
  If UsbIo is NULL, then ASSERT().
  If Status is NULL, then ASSERT().

  @param  UsbIo      A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Recipient  The USB data recipient type (i.e. Device, Interface, Endpoint).
                     Type USB_TYPES_DEFINITION is defined in the MDE Package Industry
                     Standard include file Usb.h.
  @param  Value      The value of the feature to be set.
  @param  Target     The index of the device to be set.
  @param  Status     A pointer to the status of the transfer.

  @retval EFI_SUCCESS       The request executed successfully.
  @retval EFI_TIMEOUT       A timeout occurred executing the request.
  @retval EFI_DEVICE_ERROR  The request failed due to a device error.
                            The transfer status is returned in Status.

**/
EFI_STATUS
EFIAPI
UsbSetFeature (
  IN  EFI_USB_IO_PROTOCOL   *UsbIo,
  IN  USB_TYPES_DEFINITION  Recipient,
  IN  UINT16                Value,
  IN  UINT16                Target,
  OUT UINT32                *Status
  )
{
  EFI_USB_DEVICE_REQUEST  DevReq;

  ASSERT (UsbIo != NULL);
  ASSERT (Status != NULL);

  ZeroMem (&DevReq, sizeof (EFI_USB_DEVICE_REQUEST));

  switch (Recipient) {
    case USB_TARGET_DEVICE:
      DevReq.RequestType = USB_DEV_SET_FEATURE_REQ_TYPE_D;
      break;

    case USB_TARGET_INTERFACE:
      DevReq.RequestType = USB_DEV_SET_FEATURE_REQ_TYPE_I;
      break;

    case USB_TARGET_ENDPOINT:
      DevReq.RequestType = USB_DEV_SET_FEATURE_REQ_TYPE_E;
      break;

    default:
      break;
  }

  //
  // Fill device request, see USB1.1 spec
  //
  DevReq.Request = USB_REQ_SET_FEATURE;
  DevReq.Value   = Value;
  DevReq.Index   = Target;

  return UsbIo->UsbControlTransfer (
                  UsbIo,
                  &DevReq,
                  EfiUsbNoData,
                  PcdGet32 (PcdUsbTransferTimeoutValue),
                  NULL,
                  0,
                  Status
                  );
}

/**
  Clear the specified feature of the specified device.

  Submit a USB clear device feature request for the USB device specified by UsbIo,
  Recipient, and Target to the value specified by Value.  The status of the transfer
  is returned in Status.
  If UsbIo is NULL, then ASSERT().
  If Status is NULL, then ASSERT().

  @param  UsbIo      A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Recipient  The USB data recipient type (i.e. Device, Interface, Endpoint).
                     Type USB_TYPES_DEFINITION is defined in the MDE Package Industry Standard
                     include file Usb.h.
  @param  Value      The value of the feature to be cleared.
  @param  Target     The index of the device to be cleared.
  @param  Status     A pointer to the status of the transfer.

  @retval EFI_SUCCESS       The request executed successfully.
  @retval EFI_TIMEOUT       A timeout occurred executing the request.
  @retval EFI_DEVICE_ERROR  The request failed due to a device error.
                            The transfer status is returned in Status.

**/
EFI_STATUS
EFIAPI
UsbClearFeature (
  IN  EFI_USB_IO_PROTOCOL   *UsbIo,
  IN  USB_TYPES_DEFINITION  Recipient,
  IN  UINT16                Value,
  IN  UINT16                Target,
  OUT UINT32                *Status
  )
{
  EFI_USB_DEVICE_REQUEST  DevReq;

  ASSERT (UsbIo != NULL);
  ASSERT (Status != NULL);

  ZeroMem (&DevReq, sizeof (EFI_USB_DEVICE_REQUEST));

  switch (Recipient) {
    case USB_TARGET_DEVICE:
      DevReq.RequestType = USB_DEV_CLEAR_FEATURE_REQ_TYPE_D;
      break;

    case USB_TARGET_INTERFACE:
      DevReq.RequestType = USB_DEV_CLEAR_FEATURE_REQ_TYPE_I;
      break;

    case USB_TARGET_ENDPOINT:
      DevReq.RequestType = USB_DEV_CLEAR_FEATURE_REQ_TYPE_E;
      break;

    default:
      break;
  }

  //
  // Fill device request, see USB1.1 spec
  //
  DevReq.Request = USB_REQ_CLEAR_FEATURE;
  DevReq.Value   = Value;
  DevReq.Index   = Target;

  return UsbIo->UsbControlTransfer (
                  UsbIo,
                  &DevReq,
                  EfiUsbNoData,
                  PcdGet32 (PcdUsbTransferTimeoutValue),
                  NULL,
                  0,
                  Status
                  );
}

/**
  Get the status of the specified device.

  Submit a USB device get status request for the USB device specified by UsbIo,
  Recipient, and Target and place the result in the buffer specified by DeviceStatus.
  The status of the transfer is returned in Status.
  If UsbIo is NULL, then ASSERT().
  If DeviceStatus is NULL, then ASSERT().
  If Status is NULL, then ASSERT().

  @param  UsbIo         A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Recipient     The USB data recipient type (i.e. Device, Interface, Endpoint).
                        Type USB_TYPES_DEFINITION is defined in the MDE Package Industry Standard
                        include file Usb.h.
  @param  Target        The index of the device to be get the status of.
  @param  DeviceStatus  A pointer to the device status to be retrieved.
  @param  Status        A pointer to the status of the transfer.

  @retval EFI_SUCCESS       The request executed successfully.
  @retval EFI_TIMEOUT       A timeout occurred executing the request.
  @retval EFI_DEVICE_ERROR  The request failed due to a device error.
                            The transfer status is returned in Status.

**/
EFI_STATUS
EFIAPI
UsbGetStatus (
  IN  EFI_USB_IO_PROTOCOL   *UsbIo,
  IN  USB_TYPES_DEFINITION  Recipient,
  IN  UINT16                Target,
  OUT UINT16                *DeviceStatus,
  OUT UINT32                *Status
  )
{
  EFI_USB_DEVICE_REQUEST  DevReq;

  ASSERT (UsbIo != NULL);
  ASSERT (DeviceStatus != NULL);
  ASSERT (Status != NULL);

  ZeroMem (&DevReq, sizeof (EFI_USB_DEVICE_REQUEST));

  switch (Recipient) {
    case USB_TARGET_DEVICE:
      DevReq.RequestType = USB_DEV_GET_STATUS_REQ_TYPE_D;
      break;

    case USB_TARGET_INTERFACE:
      DevReq.RequestType = USB_DEV_GET_STATUS_REQ_TYPE_I;
      break;

    case USB_TARGET_ENDPOINT:
      DevReq.RequestType = USB_DEV_GET_STATUS_REQ_TYPE_E;
      break;

    default:
      break;
  }

  //
  // Fill device request, see USB1.1 spec
  //
  DevReq.Request = USB_REQ_GET_STATUS;
  DevReq.Value   = 0;
  DevReq.Index   = Target;
  DevReq.Length  = 2;

  return UsbIo->UsbControlTransfer (
                  UsbIo,
                  &DevReq,
                  EfiUsbDataIn,
                  PcdGet32 (PcdUsbTransferTimeoutValue),
                  DeviceStatus,
                  2,
                  Status
                  );
}

/**
  Clear halt feature of the specified usb endpoint.

  Retrieve the USB endpoint descriptor specified by UsbIo and EndPoint.
  If the USB endpoint descriptor can not be retrieved, then return EFI_NOT_FOUND.
  If the endpoint descriptor is found, then clear the halt feature of this USB endpoint.
  The status of the transfer is returned in Status.
  If UsbIo is NULL, then ASSERT().
  If Status is NULL, then ASSERT().

  @param  UsbIo     A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Endpoint  The endpoint address.
  @param  Status    A pointer to the status of the transfer.

  @retval EFI_SUCCESS       The request executed successfully.
  @retval EFI_TIMEOUT       A timeout occurred executing the request.
  @retval EFI_DEVICE_ERROR  The request failed due to a device error.
                            The transfer status is returned in Status.
  @retval EFI_NOT_FOUND     The specified USB endpoint descriptor can not be found

**/
EFI_STATUS
EFIAPI
UsbClearEndpointHalt (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  UINT8                Endpoint,
  OUT UINT32               *Status
  )
{
  EFI_STATUS                    Result;
  EFI_USB_ENDPOINT_DESCRIPTOR   EndpointDescriptor;
  EFI_USB_INTERFACE_DESCRIPTOR  InterfaceDescriptor;
  UINT8                         Index;

  ASSERT (UsbIo != NULL);
  ASSERT (Status != NULL);

  ZeroMem (&EndpointDescriptor, sizeof (EFI_USB_ENDPOINT_DESCRIPTOR));
  //
  // First search the endpoint descriptor for that endpoint addr
  //
  Result = UsbIo->UsbGetInterfaceDescriptor (
                    UsbIo,
                    &InterfaceDescriptor
                    );
  if (EFI_ERROR (Result)) {
    return Result;
  }

  for (Index = 0; Index < InterfaceDescriptor.NumEndpoints; Index++) {
    Result = UsbIo->UsbGetEndpointDescriptor (
                      UsbIo,
                      Index,
                      &EndpointDescriptor
                      );
    if (EFI_ERROR (Result)) {
      continue;
    }

    if (EndpointDescriptor.EndpointAddress == Endpoint) {
      break;
    }
  }

  if (Index == InterfaceDescriptor.NumEndpoints) {
    //
    // No such endpoint
    //
    return EFI_NOT_FOUND;
  }

  Result = UsbClearFeature (
             UsbIo,
             USB_TARGET_ENDPOINT,
             USB_FEATURE_ENDPOINT_HALT,
             EndpointDescriptor.EndpointAddress,
             Status
             );

  return Result;
}
