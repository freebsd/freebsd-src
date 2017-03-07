/** @file

  The library provides USB HID Class standard and specific requests defined
  in USB HID Firmware Specification 7 section : Requests.
  
  Copyright (c) 2004 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.
  
  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "UefiUsbLibInternal.h"

//  
//  Hid RequestType Bits specifying characteristics of request.
//  Valid values are 10100001b (0xa1) or 00100001b (0x21).
//  The following description:
//    7 Data transfer direction
//        0 = Host to device
//        1 = Device to host
//    6..5 Type
//        1 = Class
//    4..0 Recipient
//        1 = Interface
//

/**
  Get the descriptor of the specified USB HID interface.

  Submit a USB get HID descriptor request for the USB device specified by UsbIo
  and Interface and return the HID descriptor in HidDescriptor.
  If UsbIo is NULL, then ASSERT().
  If HidDescriptor is NULL, then ASSERT().

  @param  UsbIo          A pointer to the USB I/O Protocol instance for the specific USB target.
  @param  Interface      The index of the HID interface on the USB target.
  @param  HidDescriptor  The pointer to the USB HID descriptor that was retrieved from
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
  )
{
  UINT32                  Status;
  EFI_STATUS              Result;
  EFI_USB_DEVICE_REQUEST  Request;

  ASSERT(UsbIo != NULL);
  ASSERT(HidDescriptor != NULL);

  Request.RequestType = USB_HID_GET_DESCRIPTOR_REQ_TYPE;
  Request.Request     = USB_REQ_GET_DESCRIPTOR;
  Request.Value       = (UINT16) (USB_DESC_TYPE_HID << 8);
  Request.Index       = Interface;
  Request.Length      = (UINT16) sizeof (EFI_USB_HID_DESCRIPTOR);

  Result = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Request,
                    EfiUsbDataIn,
                    PcdGet32 (PcdUsbTransferTimeoutValue),
                    HidDescriptor,
                    sizeof (EFI_USB_HID_DESCRIPTOR),
                    &Status
                    );

  return Result;

}

/**
  Get the report descriptor of the specified USB HID interface.

  Submit a USB get HID report descriptor request for the USB device specified by
  UsbIo and Interface and return the report descriptor in DescriptorBuffer.
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
  )
{
  UINT32                  Status;
  EFI_STATUS              Result;
  EFI_USB_DEVICE_REQUEST  Request;

  ASSERT (UsbIo != NULL);
  ASSERT (DescriptorBuffer != NULL);

  //
  // Fill Device request packet
  //
  Request.RequestType = USB_HID_GET_DESCRIPTOR_REQ_TYPE;
  Request.Request     = USB_REQ_GET_DESCRIPTOR;
  Request.Value       = (UINT16) (USB_DESC_TYPE_REPORT << 8);
  Request.Index       = Interface;
  Request.Length      = DescriptorLength;

  Result = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Request,
                    EfiUsbDataIn,
                    PcdGet32 (PcdUsbTransferTimeoutValue),
                    DescriptorBuffer,
                    DescriptorLength,
                    &Status
                    );

  return Result;

}

/**
  Get the HID protocol of the specified USB HID interface.

  Submit a USB get HID protocol request for the USB device specified by UsbIo
  and Interface and return the protocol retrieved in Protocol.
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
  )
{
  UINT32                  Status;
  EFI_STATUS              Result;
  EFI_USB_DEVICE_REQUEST  Request;

  ASSERT (UsbIo != NULL);
  ASSERT (Protocol != NULL);

  //
  // Fill Device request packet
  //
  Request.RequestType = USB_HID_CLASS_GET_REQ_TYPE;
  Request.Request = EFI_USB_GET_PROTOCOL_REQUEST;
  Request.Value   = 0;
  Request.Index   = Interface;
  Request.Length  = 1;

  Result = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Request,
                    EfiUsbDataIn,
                    PcdGet32 (PcdUsbTransferTimeoutValue),
                    Protocol,
                    sizeof (UINT8),
                    &Status
                    );

  return Result;
}



/**
  Set the HID protocol of the specified USB HID interface.

  Submit a USB set HID protocol request for the USB device specified by UsbIo
  and Interface and set the protocol to the value specified by Protocol.
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
  )
{
  UINT32                  Status;
  EFI_STATUS              Result;
  EFI_USB_DEVICE_REQUEST  Request;

  ASSERT (UsbIo != NULL);
  
  //
  // Fill Device request packet
  //
  Request.RequestType = USB_HID_CLASS_SET_REQ_TYPE;
  Request.Request = EFI_USB_SET_PROTOCOL_REQUEST;
  Request.Value   = Protocol;
  Request.Index   = Interface;
  Request.Length  = 0;

  Result = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Request,
                    EfiUsbNoData,
                    PcdGet32 (PcdUsbTransferTimeoutValue),
                    NULL,
                    0,
                    &Status
                    );
  return Result;
}


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
  )
{
  UINT32                  Status;
  EFI_STATUS              Result;
  EFI_USB_DEVICE_REQUEST  Request;

  ASSERT (UsbIo != NULL);
  //
  // Fill Device request packet
  //
  Request.RequestType = USB_HID_CLASS_SET_REQ_TYPE;
  Request.Request = EFI_USB_SET_IDLE_REQUEST;
  Request.Value   = (UINT16) ((Duration << 8) | ReportId);
  Request.Index   = Interface;
  Request.Length  = 0;

  Result = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Request,
                    EfiUsbNoData,
                    PcdGet32 (PcdUsbTransferTimeoutValue),
                    NULL,
                    0,
                    &Status
                    );
  return Result;
}


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
  )
{
  UINT32                  Status;
  EFI_STATUS              Result;
  EFI_USB_DEVICE_REQUEST  Request;
  
  ASSERT (UsbIo != NULL);
  ASSERT (Duration != NULL);
  //
  // Fill Device request packet
  //
  Request.RequestType = USB_HID_CLASS_GET_REQ_TYPE;
  Request.Request = EFI_USB_GET_IDLE_REQUEST;
  Request.Value   = ReportId;
  Request.Index   = Interface;
  Request.Length  = 1;

  Result = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Request,
                    EfiUsbDataIn,
                    PcdGet32 (PcdUsbTransferTimeoutValue),
                    Duration,
                    1,
                    &Status
                    );

  return Result;
}



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
  )
{
  UINT32                  Status;
  EFI_STATUS              Result;
  EFI_USB_DEVICE_REQUEST  Request;

  ASSERT (UsbIo != NULL);
  ASSERT (Report != NULL);

  //
  // Fill Device request packet
  //
  Request.RequestType = USB_HID_CLASS_SET_REQ_TYPE;
  Request.Request = EFI_USB_SET_REPORT_REQUEST;
  Request.Value   = (UINT16) ((ReportType << 8) | ReportId);
  Request.Index   = Interface;
  Request.Length  = ReportLen;

  Result = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Request,
                    EfiUsbDataOut,
                    PcdGet32 (PcdUsbTransferTimeoutValue),
                    Report,
                    ReportLen,
                    &Status
                    );

  return Result;
}


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
  )
{
  UINT32                  Status;
  EFI_STATUS              Result;
  EFI_USB_DEVICE_REQUEST  Request;

  ASSERT (UsbIo != NULL);
  ASSERT (Report != NULL);

  //
  // Fill Device request packet
  //
  Request.RequestType = USB_HID_CLASS_GET_REQ_TYPE;
  Request.Request = EFI_USB_GET_REPORT_REQUEST;
  Request.Value   = (UINT16) ((ReportType << 8) | ReportId);
  Request.Index   = Interface;
  Request.Length  = ReportLen;

  Result = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Request,
                    EfiUsbDataIn,
                    PcdGet32 (PcdUsbTransferTimeoutValue),
                    Report,
                    ReportLen,
                    &Status
                    );

  return Result;
}
