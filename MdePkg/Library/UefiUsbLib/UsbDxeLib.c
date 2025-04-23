/** @file

  The library provides the USB Standard Device Requests defined
  in Usb specification 9.4 section.

  Copyright (c) 2004 - 2018, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2024, American Megatrends International LLC. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UefiUsbLibInternal.h"

static UINT8                      *mConfigData = NULL;
static EFI_USB_DEVICE_DESCRIPTOR  mDeviceDescriptor;

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

/**
  Global library data initialization.

  Library public functions' input is the instance of UsbIo protocol. Check if the global
  data relevant to the UsbIo. If not, read the device and update the global data.

  @param  UsbIo           The instance of EFI_USB_IO_PROTOCOL.

  @retval EFI_SUCCESS     The global data is updated.
  @retval EFI_NOT_FOUND   The UsbIo configuration was not found.

**/
static
EFI_STATUS
InitUsbConfigDescriptorData (
  EFI_USB_IO_PROTOCOL  *UsbIo
  )
{
  EFI_STATUS                 Status;
  EFI_USB_DEVICE_DESCRIPTOR  DevDesc;
  EFI_USB_CONFIG_DESCRIPTOR  CnfDesc;
  UINT8                      ConfigNum;
  UINT8                      ConfigValue;
  UINT32                     UsbStatus;

  //
  // Get UsbIo device and configuration descriptors.
  //
  Status = UsbIo->UsbGetDeviceDescriptor (UsbIo, &DevDesc);
  ASSERT_EFI_ERROR (Status);

  Status = UsbIo->UsbGetConfigDescriptor (UsbIo, &CnfDesc);
  ASSERT_EFI_ERROR (Status);

  if (mConfigData != NULL) {
    if (  (CompareMem (&DevDesc, &mDeviceDescriptor, sizeof (EFI_USB_DEVICE_DESCRIPTOR)) == 0)
       && (CompareMem (&CnfDesc, mConfigData, sizeof (EFI_USB_CONFIG_DESCRIPTOR)) == 0))
    {
      return EFI_SUCCESS;
    }

    gBS->FreePool (mConfigData);
    mConfigData = NULL;
  }

  CopyMem (&mDeviceDescriptor, &DevDesc, sizeof (EFI_USB_DEVICE_DESCRIPTOR));

  //
  // Examine device with multiple configurations: find configuration index of UsbIo config descriptor.
  //
  // Use EFI_USB_DEVICE_DESCRIPTOR.NumConfigurations to loop through configuration descriptors, match
  // EFI_USB_CONFIG_DESCRIPTOR.ConfigurationValue to the configuration value reported by UsbIo->UsbGetConfigDescriptor.
  // The index of the matched configuration is used in wValue of the following GET_DESCRIPTOR request.
  //
  ConfigValue = CnfDesc.ConfigurationValue;
  for (ConfigNum = 0; ConfigNum < DevDesc.NumConfigurations; ConfigNum++) {
    Status = UsbGetDescriptor (
               UsbIo,
               (USB_DESC_TYPE_CONFIG << 8) | ConfigNum,
               0,
               sizeof (EFI_USB_CONFIG_DESCRIPTOR),
               &CnfDesc,
               &UsbStatus
               );
    ASSERT_EFI_ERROR (Status);

    if (CnfDesc.ConfigurationValue == ConfigValue) {
      break;
    }
  }

  ASSERT (ConfigNum < DevDesc.NumConfigurations);
  if (ConfigNum == DevDesc.NumConfigurations) {
    return EFI_NOT_FOUND;
  }

  //
  // ConfigNum has zero based index of the configuration that UsbIo belongs to. Use this index to retrieve
  // full configuration descriptor data.
  //
  Status = gBS->AllocatePool (EfiBootServicesData, CnfDesc.TotalLength, (VOID **)&mConfigData);
  ASSERT_EFI_ERROR (Status);

  Status = UsbGetDescriptor (
             UsbIo,
             (USB_DESC_TYPE_CONFIG << 8) | ConfigNum,
             0,
             CnfDesc.TotalLength,
             mConfigData,
             &UsbStatus
             );
  ASSERT_EFI_ERROR (Status);

  return Status;
}

/**
  Find descriptor of a given type within data area pointed by mConfigData.

  The following are the assumptions of the configuration descriptor layout:
  - mConfigData is populated with the configuration data that contains USB interface referenced by UsbIo.
  - Endpoint may have only one class specific descriptor that immediately follows the endpoint descriptor.

  @param[in]  UsbIo             A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param[in]  DescType          Type of descriptor to look for.
  @param[in]  Setting           Interface alternate setting.
  @param[in]  Index             Index of the descriptor. This descriptor index is used to find a specific
                                descriptor (only for endpoint descriptors and class specific interface descriptors)
                                when several descriptors of the same type are implemented in a device. For other
                                descriptor types, a descriptor index of zero must be used.
  @param[out]  Data             A pointer to the caller allocated Descriptor.

  @retval EFI_SUCCESS           Output parameters were updated successfully.
  @retval EFI_UNSUPPORTED       Setting is greater than the number of alternate settings in this interface.
  @retval EFI_NOT_FOUND         Index is greater than the number of descriptors of the requested type in this
                                interface.
**/
static
EFI_STATUS
FindUsbDescriptor (
  EFI_USB_IO_PROTOCOL  *UsbIo,
  UINT8                DescType,
  UINT16               Setting,
  UINTN                Index,
  VOID                 **Data
  )
{
  EFI_USB_INTERFACE_DESCRIPTOR  IntfDesc;
  EFI_STATUS                    Status;
  UINT8                         *BufferPtr;
  UINT8                         *BufferEnd;
  UINT8                         *ConfigEnd;
  UINTN                         Idx;

  //
  // Find the interface descriptor referenced by UsbIo in the current configuration
  //
  Status = UsbIo->UsbGetInterfaceDescriptor (UsbIo, &IntfDesc);
  ASSERT_EFI_ERROR (Status);

  ConfigEnd = mConfigData + ((EFI_USB_CONFIG_DESCRIPTOR *)mConfigData)->TotalLength;

  for (BufferPtr = mConfigData; BufferPtr < ConfigEnd; BufferPtr += BufferPtr[0]) {
    if (BufferPtr[1] == USB_DESC_TYPE_INTERFACE) {
      if ((BufferPtr[2] == IntfDesc.InterfaceNumber) && (BufferPtr[3] == (UINT8)Setting)) {
        break;
      }
    }
  }

  if (BufferPtr >= ConfigEnd) {
    return EFI_UNSUPPORTED;
  }

  //
  // Found the beginning of the interface, find the ending
  //
  for (BufferEnd = BufferPtr + BufferPtr[0]; BufferEnd < ConfigEnd; BufferEnd += BufferEnd[0]) {
    if (BufferEnd[1] == USB_DESC_TYPE_INTERFACE) {
      break;
    }
  }

  Idx = 0;

  if (DescType == USB_DESC_TYPE_INTERFACE) {
    *Data = BufferPtr;
    return EFI_SUCCESS;
  }

  if ((DescType == USB_DESC_TYPE_ENDPOINT) || (DescType == USB_DESC_TYPE_CS_ENDPOINT)) {
    while (BufferPtr < BufferEnd) {
      BufferPtr += BufferPtr[0];
      if (BufferPtr[1] == USB_DESC_TYPE_ENDPOINT) {
        if (Idx == Index) {
          if (DescType == USB_DESC_TYPE_CS_ENDPOINT) {
            BufferPtr += BufferPtr[0];
            if (BufferPtr[1] != USB_DESC_TYPE_CS_ENDPOINT) {
              break;
            }
          }

          *Data = BufferPtr;
          return EFI_SUCCESS;
        }

        Idx++;
      }
    }
  }

  if (DescType  == USB_DESC_TYPE_CS_INTERFACE) {
    while (BufferPtr < BufferEnd) {
      BufferPtr += BufferPtr[0];
      if (BufferPtr[1] == USB_DESC_TYPE_CS_INTERFACE) {
        if (Idx == Index) {
          *Data = BufferPtr;
          return EFI_SUCCESS;
        }

        Idx++;
      }
    }
  }

  return EFI_NOT_FOUND;
}

/**
  Retrieve the number of class specific interface descriptors.

  @param[in]  Data    A pointer to the USB interface descriptor that may contain class code descriptors.

  @retval UINT8       Number of the class code interface descriptors.

**/
static
UINT8
FindNumberOfCsInterfaces (
  VOID  *Data
  )
{
  UINT8  *Buffer;
  UINT8  *ConfigEnd;
  UINT8  Index;

  Buffer    = Data;
  ConfigEnd = mConfigData + ((EFI_USB_CONFIG_DESCRIPTOR *)mConfigData)->TotalLength;

  Index = 0;

  for (Buffer += Buffer[0]; Buffer < ConfigEnd; Buffer += Buffer[0]) {
    if (Buffer[1] == USB_DESC_TYPE_INTERFACE) {
      break;
    }

    if (Buffer[1] == USB_DESC_TYPE_CS_INTERFACE) {
      Index++;
    }
  }

  return Index;
}

/**
  Retrieve the interface descriptor details from the interface setting.

  This is an extended version of UsbIo->GetInterfaceDescriptor. It returns the interface
  descriptor for an alternate setting of the interface without executing SET_INTERFACE
  transfer. It also returns the number of class specific interfaces.
  AlternateSetting parameter is the zero-based interface descriptor index that is used in USB
  interface descriptor as USB_INTERFACE_DESCRIPTOR.AlternateSetting.


  @param[in]  This              A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param[in]  AlternateSetting  Interface alternate setting.
  @param[out]  Descriptor       The caller allocated buffer to return the contents of the Interface descriptor.
  @param[out]  CsInterfaceNumber  Number of class specific interfaces for this interface setting.

  @retval EFI_SUCCESS           Output parameters were updated successfully.
  @retval EFI_INVALID_PARAMETER Descriptor or CsInterfaceNumber is NULL.
  @retval EFI_UNSUPPORTED       AlternateSetting is greater than the number of alternate settings in this interface.
  @retval EFI_DEVICE_ERROR      Error reading device data.

**/
EFI_STATUS
EFIAPI
UsbGetInterfaceDescriptorSetting (
  IN  EFI_USB_IO_PROTOCOL           *This,
  IN  UINT16                        AlternateSetting,
  OUT EFI_USB_INTERFACE_DESCRIPTOR  *Descriptor,
  OUT UINTN                         *CsInterfacesNumber
  )
{
  EFI_STATUS  Status;
  VOID        *Data;
  EFI_TPL     OldTpl;

  if ((Descriptor == NULL) || (CsInterfacesNumber == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);

  Status = InitUsbConfigDescriptorData (This);
  if (EFI_ERROR (Status)) {
    Status = EFI_DEVICE_ERROR;
    goto ON_EXIT;
  }

  Status = FindUsbDescriptor (This, USB_DESC_TYPE_INTERFACE, AlternateSetting, 0, &Data);
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  *CsInterfacesNumber = FindNumberOfCsInterfaces (Data);
  CopyMem (Descriptor, Data, sizeof (EFI_USB_INTERFACE_DESCRIPTOR));

ON_EXIT:
  gBS->RestoreTPL (OldTpl);
  return Status;
}

/**
  Retrieve the endpoint descriptor from the interface setting.

  This is an extended version of UsbIo->GetEndpointDescriptor. It returns the endpoint
  descriptor for an alternate setting of a given interface.
  AlternateSetting parameter is the zero-based interface descriptor index that is used in USB
  interface descriptor as USB_INTERFACE_DESCRIPTOR.AlternateSetting.

  Note: The total number of endpoints can be retrieved from the interface descriptor
  returned by EDKII_USBIO_EXT_GET_INTERFACE_DESCRIPTOR function.

  @param[in]  This              A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param[in]  AlternateSetting  Interface alternate setting.
  @param[in]  Index             Index of the endpoint to retrieve. The valid range is 0..15.
  @param[out]  Descriptor       A pointer to the caller allocated USB Interface Descriptor.

  @retval EFI_SUCCESS           Output parameters were updated successfully.
  @retval EFI_INVALID_PARAMETER Descriptor is NULL.
  @retval EFI_UNSUPPORTED       AlternateSetting is greater than the number of alternate settings in this interface.
  @retval EFI_NOT_FOUND         Index is greater than the number of endpoints in this interface.
  @retval EFI_DEVICE_ERROR      Error reading device data.

**/
EFI_STATUS
EFIAPI
UsbGetEndpointDescriptorSetting (
  IN  EFI_USB_IO_PROTOCOL          *This,
  IN  UINT16                       AlternateSetting,
  IN  UINTN                        Index,
  OUT EFI_USB_ENDPOINT_DESCRIPTOR  *Descriptor
  )
{
  EFI_STATUS  Status;
  VOID        *Data;
  EFI_TPL     OldTpl;

  if (Descriptor == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);

  Status = InitUsbConfigDescriptorData (This);
  if (EFI_ERROR (Status)) {
    Status = EFI_DEVICE_ERROR;
    goto ON_EXIT;
  }

  Status = FindUsbDescriptor (This, USB_DESC_TYPE_ENDPOINT, AlternateSetting, Index, &Data);
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  CopyMem (Descriptor, Data, sizeof (EFI_USB_ENDPOINT_DESCRIPTOR));

ON_EXIT:
  gBS->RestoreTPL (OldTpl);
  return Status;
}

/**
  Retrieve class specific interface descriptor.

  AlternateSetting parameter is the zero-based interface descriptor index that is used in USB
  interface descriptor as USB_INTERFACE_DESCRIPTOR.AlternateSetting.

  @param[in]  This              A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param[in]  AlternateSetting  Interface alternate setting.
  @param[in]  Index             Zero-based index of the class specific interface.
  @param[in][out]  BufferSize   On input, the size in bytes of the return Descriptor buffer.
                                On output the size of data returned in Descriptor.
  @param[out]  Descriptor       The buffer to return the contents of the class specific interface descriptor. May
                                be NULL with a zero BufferSize in order to determine the size buffer needed.

  @retval EFI_SUCCESS           Output parameters were updated successfully.
  @retval EFI_INVALID_PARAMETER BufferSize is NULL.
                                Buffer is NULL and *BufferSize is not zero.
  @retval EFI_UNSUPPORTED       AlternateSetting is greater than the number of alternate settings in this interface.
  @retval EFI_NOT_FOUND         Index is greater than the number of class specific interfaces.
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize is too small for the result. BufferSize has been updated with the size
                                needed to complete the request.
  @retval EFI_DEVICE_ERROR      Error reading device data.

**/
EFI_STATUS
EFIAPI
UsbGetCsInterfaceDescriptor (
  IN  EFI_USB_IO_PROTOCOL  *This,
  IN  UINT16               AlternateSetting,
  IN  UINTN                Index,
  IN OUT UINTN             *BufferSize,
  OUT VOID                 *Buffer
  )
{
  EFI_STATUS  Status;
  VOID        *Data;
  UINT8       DescLength;
  EFI_TPL     OldTpl;

  if ((BufferSize == NULL) || ((Buffer == NULL) && (*BufferSize != 0))) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);

  Status = InitUsbConfigDescriptorData (This);
  if (EFI_ERROR (Status)) {
    Status = EFI_DEVICE_ERROR;
    goto ON_EXIT;
  }

  Status = FindUsbDescriptor (This, USB_DESC_TYPE_CS_INTERFACE, AlternateSetting, Index, &Data);
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  DescLength = ((UINT8 *)Data)[0];

  if ((Buffer == NULL) || (DescLength > *BufferSize)) {
    *BufferSize = DescLength;
    Status      = EFI_BUFFER_TOO_SMALL;
    goto ON_EXIT;
  }

  CopyMem (Buffer, Data, DescLength);

ON_EXIT:
  gBS->RestoreTPL (OldTpl);
  return Status;
}

/**
  Retrieve class specific endpoint descriptor.

  AlternateSetting parameter is the zero-based interface descriptor index that is used in USB
  interface descriptor as USB_INTERFACE_DESCRIPTOR.AlternateSetting.

  @param[in]  This              A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param[in]  AlternateSetting  Interface alternate setting.
  @param[in]  Index             Zero-based index of the non-zero endpoint.
  @param[in][out]  BufferSize   On input, the size in bytes of the return Descriptor buffer.
                                On output the size of data returned in Descriptor.
  @param[out]  Descriptor       The buffer to return the contents of the class specific endpoint descriptor. May
                                be NULL with a zero BufferSize in order to determine the size buffer needed.

  @retval EFI_SUCCESS           Output parameters were updated successfully.
  @retval EFI_INVALID_PARAMETER BufferSize is NULL.
                                Buffer is NULL and *BufferSize is not zero.
  @retval EFI_UNSUPPORTED       AlternateSetting is greater than the number of alternate settings in this interface.
  @retval EFI_NOT_FOUND         Index is greater than the number of endpoints in this interface.
                                Endpoint does not have class specific endpoint descriptor.
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize is too small for the result. BufferSize has been updated with the size
                                needed to complete the request.
  @retval EFI_DEVICE_ERROR      Error reading device data.

**/
EFI_STATUS
EFIAPI
UsbGetCsEndpointDescriptor (
  IN  EFI_USB_IO_PROTOCOL  *This,
  IN  UINT16               AlternateSetting,
  IN  UINTN                Index,
  IN OUT UINTN             *BufferSize,
  OUT VOID                 *Buffer
  )
{
  EFI_STATUS  Status;
  VOID        *Data;
  UINT8       DescLength;
  EFI_TPL     OldTpl;

  if ((BufferSize == NULL) || ((Buffer == NULL) && (*BufferSize != 0))) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);

  Status = InitUsbConfigDescriptorData (This);
  if (EFI_ERROR (Status)) {
    Status = EFI_DEVICE_ERROR;
    goto ON_EXIT;
  }

  Status = FindUsbDescriptor (This, USB_DESC_TYPE_CS_ENDPOINT, AlternateSetting, Index, &Data);
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  DescLength = ((UINT8 *)Data)[0];

  if ((Buffer == NULL) || (DescLength > *BufferSize)) {
    *BufferSize = DescLength;
    Status      = EFI_BUFFER_TOO_SMALL;
    goto ON_EXIT;
  }

  CopyMem (Buffer, Data, DescLength);

ON_EXIT:
  gBS->RestoreTPL (OldTpl);
  return Status;
}

/**
  Destructor frees memory which was allocated by the library functions.

  @param ImageHandle       Handle that identifies the image to be unloaded.
  @param  SystemTable      The system table.

  @retval EFI_SUCCESS      The image has been unloaded.

**/
EFI_STATUS
EFIAPI
UefiUsbLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  if (mConfigData != NULL) {
    gBS->FreePool (mConfigData);
  }

  return EFI_SUCCESS;
}
