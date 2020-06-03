/** @file
  Provides most USB APIs to support the Hid requests defined in USB Hid 1.1 spec
  and the standard requests defined in USB 1.1 spec.

Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#ifndef __USB_DXE_LIB_H__
#define __USB_DXE_LIB_H__

#include <Protocol/UsbIo.h>

/**
  Get the descriptor of the specified USB HID interface.

  Submit a UsbGetHidDescriptor() request for the USB device specified by UsbIo
  and Interface, and return the HID descriptor in HidDescriptor.
  If UsbIo is NULL, then ASSERT().
  If HidDescriptor is NULL, then ASSERT().

  @param  UsbIo          A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Interface      The index of the HID interface on the USB target.
  @param  HidDescriptor  Pointer to the USB HID descriptor that was retrieved from
                         the specified USB target and interface. Type EFI_USB_HID_DESCRIPTOR
                         is defined in the MDE Package Industry Standard include file Usb.h.

  @retval EFI_SUCCESS       The request executed successfully.
  @retval EFI_TIMEOUT       A timeout occurred executing the request.
  @retval EFI_DEVICE_ERROR  The request failed due to a device error.

**/
EFI_STATUS
EFIAPI
UsbGetHidDescriptor (
  IN  EFI_USB_IO_PROTOCOL        *UsbIo,
  IN  UINT8                      Interface,
  OUT EFI_USB_HID_DESCRIPTOR     *HidDescriptor
  );


/**
  Get the report descriptor of the specified USB HID interface.

  Submit a USB get HID report descriptor request for the USB device specified by
  UsbIo and Interface, and return the report descriptor in DescriptorBuffer.
  If UsbIo is NULL, then ASSERT().
  If DescriptorBuffer is NULL, then ASSERT().

  @param  UsbIo             A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Interface         The index of the report interface on the USB target.
  @param  DescriptorLength  The size, in bytes, of DescriptorBuffer.
  @param  DescriptorBuffer  A pointer to the buffer to store the report class descriptor.

  @retval  EFI_SUCCESS           The request executed successfully.
  @retval  EFI_OUT_OF_RESOURCES  The request could not be completed because the
                                 buffer specified by DescriptorLength and DescriptorBuffer
                                 is not large enough to hold the result of the request.
  @retval  EFI_TIMEOUT           A timeout occurred executing the request.
  @retval  EFI_DEVICE_ERROR      The request failed due to a device error.

**/
EFI_STATUS
EFIAPI
UsbGetReportDescriptor (
  IN  EFI_USB_IO_PROTOCOL     *UsbIo,
  IN  UINT8                   Interface,
  IN  UINT16                  DescriptorLength,
  OUT UINT8                   *DescriptorBuffer
  );

/**
  Get the HID protocol of the specified USB HID interface.

  Submit a USB get HID protocol request for the USB device specified by UsbIo
  and Interface, and return the protocol retrieved in Protocol.
  If UsbIo is NULL, then ASSERT().
  If Protocol is NULL, then ASSERT().

  @param  UsbIo      A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Interface  The index of the report interface on the USB target.
  @param  Protocol   A pointer to the protocol for the specified USB target.

  @retval  EFI_SUCCESS       The request executed successfully.
  @retval  EFI_TIMEOUT       A timeout occurred executing the request.
  @retval  EFI_DEVICE_ERROR  The request failed due to a device error.

**/
EFI_STATUS
EFIAPI
UsbGetProtocolRequest (
  IN EFI_USB_IO_PROTOCOL     *UsbIo,
  IN UINT8                   Interface,
  OUT UINT8                   *Protocol
  );

/**
  Set the HID protocol of the specified USB HID interface.

  Submit a USB set HID protocol request for the USB device specified by UsbIo
  and Interface, and set the protocol to the value specified by Protocol.
  If UsbIo is NULL, then ASSERT().

  @param  UsbIo      A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Interface  The index of the report interface on the USB target.
  @param  Protocol   The protocol value to set for the specified USB target.

  @retval  EFI_SUCCESS       The request executed successfully.
  @retval  EFI_TIMEOUT       A timeout occurred executing the request.
  @retval  EFI_DEVICE_ERROR  The request failed due to a device error.

**/
EFI_STATUS
EFIAPI
UsbSetProtocolRequest (
  IN EFI_USB_IO_PROTOCOL     *UsbIo,
  IN UINT8                   Interface,
  IN UINT8                   Protocol
  );

/**
  Set the idle rate of the specified USB HID report.

  Submit a USB set HID report idle request for the USB device specified by UsbIo,
  Interface, and ReportId, and set the idle rate to the value specified by Duration.
  If UsbIo is NULL, then ASSERT().

  @param  UsbIo      A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Interface  The index of the report interface on the USB target.
  @param  ReportId   The identifier of the report to retrieve.
  @param  Duration   The idle rate to set for the specified USB target.

  @retval  EFI_SUCCESS       The request executed successfully.
  @retval  EFI_TIMEOUT       A timeout occurred executing the request.
  @retval  EFI_DEVICE_ERROR  The request failed due to a device error.

**/
EFI_STATUS
EFIAPI
UsbSetIdleRequest (
  IN EFI_USB_IO_PROTOCOL     *UsbIo,
  IN UINT8                   Interface,
  IN UINT8                   ReportId,
  IN UINT8                   Duration
  );

/**
  Get the idle rate of the specified USB HID report.

  Submit a USB get HID report idle request for the USB device specified by UsbIo,
  Interface, and ReportId, and return the ide rate in Duration.
  If UsbIo is NULL, then ASSERT().
  If Duration is NULL, then ASSERT().

  @param  UsbIo      A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Interface  The index of the report interface on the USB target.
  @param  ReportId   The identifier of the report to retrieve.
  @param  Duration   A pointer to the idle rate retrieved from the specified USB target.

  @retval  EFI_SUCCESS       The request executed successfully.
  @retval  EFI_TIMEOUT       A timeout occurred executing the request.
  @retval  EFI_DEVICE_ERROR  The request failed due to a device error.

**/
EFI_STATUS
EFIAPI
UsbGetIdleRequest (
  IN  EFI_USB_IO_PROTOCOL     *UsbIo,
  IN  UINT8                   Interface,
  IN  UINT8                   ReportId,
  OUT UINT8                   *Duration
  );

/**
  Set the report descriptor of the specified USB HID interface.

  Submit a USB set HID report request for the USB device specified by UsbIo,
  Interface, ReportId, and ReportType, and set the report descriptor using the
  buffer specified by ReportLength and Report.
  If UsbIo is NULL, then ASSERT().
  If Report is NULL, then ASSERT().

  @param  UsbIo         A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Interface     The index of the report interface on the USB target.
  @param  ReportId      The identifier of the report to retrieve.
  @param  ReportType    The type of report to retrieve.
  @param  ReportLength  The size, in bytes, of Report.
  @param  Report        A pointer to the report descriptor buffer to set.

  @retval  EFI_SUCCESS       The request executed successfully.
  @retval  EFI_TIMEOUT       A timeout occurred executing the request.
  @retval  EFI_DEVICE_ERROR  The request failed due to a device error.

**/
EFI_STATUS
EFIAPI
UsbSetReportRequest (
  IN EFI_USB_IO_PROTOCOL     *UsbIo,
  IN UINT8                   Interface,
  IN UINT8                   ReportId,
  IN UINT8                   ReportType,
  IN UINT16                  ReportLen,
  IN UINT8                   *Report
  );

/**
  Get the report descriptor of the specified USB HID interface.

  Submit a USB get HID report request for the USB device specified by UsbIo,
  Interface, ReportId, and ReportType, and return the report in the buffer
  specified by Report.
  If UsbIo is NULL, then ASSERT().
  If Report is NULL, then ASSERT().

  @param  UsbIo         A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Interface     The index of the report interface on the USB target.
  @param  ReportId      The identifier of the report to retrieve.
  @param  ReportType    The type of report to retrieve.
  @param  ReportLength  The size, in bytes, of Report.
  @param  Report        A pointer to the buffer to store the report descriptor.

  @retval  EFI_SUCCESS           The request executed successfully.
  @retval  EFI_OUT_OF_RESOURCES  The request could not be completed because the
                                 buffer specified by ReportLength and Report is not
                                 large enough to hold the result of the request.
  @retval  EFI_TIMEOUT           A timeout occurred executing the request.
  @retval  EFI_DEVICE_ERROR      The request failed due to a device error.

**/
EFI_STATUS
EFIAPI
UsbGetReportRequest (
  IN  EFI_USB_IO_PROTOCOL     *UsbIo,
  IN  UINT8                   Interface,
  IN  UINT8                   ReportId,
  IN  UINT8                   ReportType,
  IN  UINT16                  ReportLen,
  OUT UINT8                   *Report
  );

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
  IN  EFI_USB_IO_PROTOCOL     *UsbIo,
  IN  UINT16                  Value,
  IN  UINT16                  Index,
  IN  UINT16                  DescriptorLength,
  OUT VOID                    *Descriptor,
  OUT UINT32                  *Status
  );

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
  IN  EFI_USB_IO_PROTOCOL     *UsbIo,
  IN  UINT16                  Value,
  IN  UINT16                  Index,
  IN  UINT16                  DescriptorLength,
  IN  VOID                    *Descriptor,
  OUT UINT32                  *Status
  );

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
  IN  EFI_USB_IO_PROTOCOL     *UsbIo,
  IN  UINT16                  Interface,
  OUT UINT16                  *AlternateSetting,
  OUT UINT32                  *Status
  );

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
  IN  EFI_USB_IO_PROTOCOL     *UsbIo,
  IN  UINT16                  Interface,
  IN  UINT16                  AlternateSetting,
  OUT UINT32                  *Status
  );

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
  IN  EFI_USB_IO_PROTOCOL     *UsbIo,
  OUT UINT16                  *ConfigurationValue,
  OUT UINT32                  *Status
  );

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
  IN  EFI_USB_IO_PROTOCOL     *UsbIo,
  IN  UINT16                  ConfigurationValue,
  OUT UINT32                  *Status
  );

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
  IN  EFI_USB_IO_PROTOCOL     *UsbIo,
  IN  USB_TYPES_DEFINITION    Recipient,
  IN  UINT16                  Value,
  IN  UINT16                  Target,
  OUT UINT32                  *Status
  );

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
  IN  EFI_USB_IO_PROTOCOL     *UsbIo,
  IN  USB_TYPES_DEFINITION    Recipient,
  IN  UINT16                  Value,
  IN  UINT16                  Target,
  OUT UINT32                  *Status
  );

/**
  Get the status of the specified device.

  Submit a USB device get status request for the USB device specified by UsbIo,
  Recipient, and Target, and place the result in the buffer specified by DeviceStatus.
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
  IN  EFI_USB_IO_PROTOCOL     *UsbIo,
  IN  USB_TYPES_DEFINITION    Recipient,
  IN  UINT16                  Target,
  OUT UINT16                  *DeviceStatus,
  OUT UINT32                  *Status
  );

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
  IN  EFI_USB_IO_PROTOCOL     *UsbIo,
  IN  UINT8                   Endpoint,
  OUT UINT32                  *Status
  );

#endif
