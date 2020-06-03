/** @file
  EFI Usb I/O Protocol as defined in UEFI specification.
  This protocol is used by code, typically drivers, running in the EFI
  boot services environment to access USB devices like USB keyboards,
  mice and mass storage devices. In particular, functions for managing devices
  on USB buses are defined here.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __USB_IO_H__
#define __USB_IO_H__

#include <IndustryStandard/Usb.h>

//
// Global ID for the USB I/O Protocol
//
#define EFI_USB_IO_PROTOCOL_GUID \
  { \
    0x2B2F68D6, 0x0CD2, 0x44cf, {0x8E, 0x8B, 0xBB, 0xA2, 0x0B, 0x1B, 0x5B, 0x75 } \
  }

typedef struct _EFI_USB_IO_PROTOCOL   EFI_USB_IO_PROTOCOL;

//
// Related Definition for EFI USB I/O protocol
//

//
// USB standard descriptors and reqeust
//
typedef USB_DEVICE_REQUEST        EFI_USB_DEVICE_REQUEST;
typedef USB_DEVICE_DESCRIPTOR     EFI_USB_DEVICE_DESCRIPTOR;
typedef USB_CONFIG_DESCRIPTOR     EFI_USB_CONFIG_DESCRIPTOR;
typedef USB_INTERFACE_DESCRIPTOR  EFI_USB_INTERFACE_DESCRIPTOR;
typedef USB_ENDPOINT_DESCRIPTOR   EFI_USB_ENDPOINT_DESCRIPTOR;

///
/// USB data transfer direction
///
typedef enum {
  EfiUsbDataIn,
  EfiUsbDataOut,
  EfiUsbNoData
} EFI_USB_DATA_DIRECTION;

//
// USB Transfer Results
//
#define EFI_USB_NOERROR             0x00
#define EFI_USB_ERR_NOTEXECUTE      0x01
#define EFI_USB_ERR_STALL           0x02
#define EFI_USB_ERR_BUFFER          0x04
#define EFI_USB_ERR_BABBLE          0x08
#define EFI_USB_ERR_NAK             0x10
#define EFI_USB_ERR_CRC             0x20
#define EFI_USB_ERR_TIMEOUT         0x40
#define EFI_USB_ERR_BITSTUFF        0x80
#define EFI_USB_ERR_SYSTEM          0x100

/**
  Async USB transfer callback routine.

  @param  Data                  Data received or sent via the USB Asynchronous Transfer, if the
                                transfer completed successfully.
  @param  DataLength            The length of Data received or sent via the Asynchronous
                                Transfer, if transfer successfully completes.
  @param  Context               Data passed from UsbAsyncInterruptTransfer() request.
  @param  Status                Indicates the result of the asynchronous transfer.

  @retval EFI_SUCCESS           The asynchronous USB transfer request has been successfully executed.
  @retval EFI_DEVICE_ERROR      The asynchronous USB transfer request failed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_ASYNC_USB_TRANSFER_CALLBACK)(
  IN VOID         *Data,
  IN UINTN        DataLength,
  IN VOID         *Context,
  IN UINT32       Status
  );

//
// Prototype for EFI USB I/O protocol
//


/**
  This function is used to manage a USB device with a control transfer pipe. A control transfer is
  typically used to perform device initialization and configuration.

  @param  This                  A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param  Request               A pointer to the USB device request that will be sent to the USB
                                device.
  @param  Direction             Indicates the data direction.
  @param  Timeout               Indicating the transfer should be completed within this time frame.
                                The units are in milliseconds.
  @param  Data                  A pointer to the buffer of data that will be transmitted to USB
                                device or received from USB device.
  @param  DataLength            The size, in bytes, of the data buffer specified by Data.
  @param  Status                A pointer to the result of the USB transfer.

  @retval EFI_SUCCESS           The control transfer has been successfully executed.
  @retval EFI_DEVICE_ERROR      The transfer failed. The transfer status is returned in Status.
  @retval EFI_INVALID_PARAMETE  One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.
  @retval EFI_TIMEOUT           The control transfer fails due to timeout.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB_IO_CONTROL_TRANSFER)(
  IN EFI_USB_IO_PROTOCOL                        *This,
  IN EFI_USB_DEVICE_REQUEST                     *Request,
  IN EFI_USB_DATA_DIRECTION                     Direction,
  IN UINT32                                     Timeout,
  IN OUT VOID                                   *Data OPTIONAL,
  IN UINTN                                      DataLength  OPTIONAL,
  OUT UINT32                                    *Status
  );

/**
  This function is used to manage a USB device with the bulk transfer pipe. Bulk Transfers are
  typically used to transfer large amounts of data to/from USB devices.

  @param  This                  A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param  DeviceEndpoint        The destination USB device endpoint to which the
                                device request is being sent. DeviceEndpoint must
                                be between 0x01 and 0x0F or between 0x81 and 0x8F,
                                otherwise EFI_INVALID_PARAMETER is returned. If
                                the endpoint is not a BULK endpoint, EFI_INVALID_PARAMETER
                                is returned. The MSB of this parameter indicates
                                the endpoint direction. The number "1" stands for
                                an IN endpoint, and "0" stands for an OUT endpoint.
  @param  Data                  A pointer to the buffer of data that will be transmitted to USB
                                device or received from USB device.
  @param  DataLength            The size, in bytes, of the data buffer specified by Data.
                                On input, the size, in bytes, of the data buffer specified by Data.
                                On output, the number of bytes that were actually transferred.
  @param  Timeout               Indicating the transfer should be completed within this time frame.
                                The units are in milliseconds. If Timeout is 0, then the
                                caller must wait for the function to be completed until
                                EFI_SUCCESS or EFI_DEVICE_ERROR is returned.
  @param  Status                This parameter indicates the USB transfer status.

  @retval EFI_SUCCESS           The bulk transfer has been successfully executed.
  @retval EFI_DEVICE_ERROR      The transfer failed. The transfer status is returned in Status.
  @retval EFI_INVALID_PARAMETE  One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The request could not be submitted due to a lack of resources.
  @retval EFI_TIMEOUT           The control transfer fails due to timeout.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB_IO_BULK_TRANSFER)(
  IN EFI_USB_IO_PROTOCOL            *This,
  IN UINT8                          DeviceEndpoint,
  IN OUT VOID                       *Data,
  IN OUT UINTN                      *DataLength,
  IN UINTN                          Timeout,
  OUT UINT32                        *Status
  );

/**
  This function is used to manage a USB device with an interrupt transfer pipe. An Asynchronous
  Interrupt Transfer is typically used to query a device's status at a fixed rate. For example,
  keyboard, mouse, and hub devices use this type of transfer to query their interrupt endpoints at
  a fixed rate.

  @param  This                  A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param  DeviceEndpoint        The destination USB device endpoint to which the
                                device request is being sent. DeviceEndpoint must
                                be between 0x01 and 0x0F or between 0x81 and 0x8F,
                                otherwise EFI_INVALID_PARAMETER is returned. If
                                the endpoint is not a BULK endpoint, EFI_INVALID_PARAMETER
                                is returned. The MSB of this parameter indicates
                                the endpoint direction. The number "1" stands for
                                an IN endpoint, and "0" stands for an OUT endpoint.
  @param  IsNewTransfer         If TRUE, a new transfer will be submitted to USB controller. If
                                FALSE, the interrupt transfer is deleted from the device's interrupt
                                transfer queue.
  @param  PollingInterval       Indicates the periodic rate, in milliseconds, that the transfer is to be
                                executed.This parameter is required when IsNewTransfer is TRUE. The
                                value must be between 1 to 255, otherwise EFI_INVALID_PARAMETER is returned.
                                The units are in milliseconds.
  @param  DataLength            Specifies the length, in bytes, of the data to be received from the
                                USB device. This parameter is only required when IsNewTransfer is TRUE.
  @param  InterruptCallback     The Callback function. This function is called if the asynchronous
                                interrupt transfer is completed. This parameter is required
                                when IsNewTransfer is TRUE.
  @param  Context               Data passed to the InterruptCallback function. This is an optional
                                parameter and may be NULL.

  @retval EFI_SUCCESS           The asynchronous USB transfer request transfer has been successfully executed.
  @retval EFI_DEVICE_ERROR      The asynchronous USB transfer request failed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB_IO_ASYNC_INTERRUPT_TRANSFER)(
  IN EFI_USB_IO_PROTOCOL                                 *This,
  IN UINT8                                               DeviceEndpoint,
  IN BOOLEAN                                             IsNewTransfer,
  IN UINTN                                               PollingInterval    OPTIONAL,
  IN UINTN                                               DataLength         OPTIONAL,
  IN EFI_ASYNC_USB_TRANSFER_CALLBACK                     InterruptCallBack  OPTIONAL,
  IN VOID                                                *Context OPTIONAL
  );

/**
  This function is used to manage a USB device with an interrupt transfer pipe.

  @param  This                  A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param  DeviceEndpoint        The destination USB device endpoint to which the
                                device request is being sent. DeviceEndpoint must
                                be between 0x01 and 0x0F or between 0x81 and 0x8F,
                                otherwise EFI_INVALID_PARAMETER is returned. If
                                the endpoint is not a BULK endpoint, EFI_INVALID_PARAMETER
                                is returned. The MSB of this parameter indicates
                                the endpoint direction. The number "1" stands for
                                an IN endpoint, and "0" stands for an OUT endpoint.
  @param  Data                  A pointer to the buffer of data that will be transmitted to USB
                                device or received from USB device.
  @param  DataLength            On input, then size, in bytes, of the buffer Data. On output, the
                                amount of data actually transferred.
  @param  Timeout               The time out, in seconds, for this transfer. If Timeout is 0,
                                then the caller must wait for the function to be completed
                                until EFI_SUCCESS or EFI_DEVICE_ERROR is returned. If the
                                transfer is not completed in this time frame, then EFI_TIMEOUT is returned.
  @param  Status                This parameter indicates the USB transfer status.

  @retval EFI_SUCCESS           The sync interrupt transfer has been successfully executed.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_DEVICE_ERROR      The sync interrupt transfer request failed.
  @retval EFI_OUT_OF_RESOURCES  The request could not be submitted due to a lack of resources.
  @retval EFI_TIMEOUT           The transfer fails due to timeout.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB_IO_SYNC_INTERRUPT_TRANSFER)(
  IN EFI_USB_IO_PROTOCOL            *This,
  IN     UINT8                      DeviceEndpoint,
  IN OUT VOID                       *Data,
  IN OUT UINTN                      *DataLength,
  IN     UINTN                      Timeout,
  OUT    UINT32                     *Status
  );

/**
  This function is used to manage a USB device with an isochronous transfer pipe. An Isochronous
  transfer is typically used to transfer streaming data.

  @param  This                  A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param  DeviceEndpoint        The destination USB device endpoint to which the
                                device request is being sent. DeviceEndpoint must
                                be between 0x01 and 0x0F or between 0x81 and 0x8F,
                                otherwise EFI_INVALID_PARAMETER is returned. If
                                the endpoint is not a BULK endpoint, EFI_INVALID_PARAMETER
                                is returned. The MSB of this parameter indicates
                                the endpoint direction. The number "1" stands for
                                an IN endpoint, and "0" stands for an OUT endpoint.
  @param  Data                  A pointer to the buffer of data that will be transmitted to USB
                                device or received from USB device.
  @param  DataLength            The size, in bytes, of the data buffer specified by Data.
  @param  Status                This parameter indicates the USB transfer status.

  @retval EFI_SUCCESS           The isochronous transfer has been successfully executed.
  @retval EFI_INVALID_PARAMETER The parameter DeviceEndpoint is not valid.
  @retval EFI_DEVICE_ERROR      The transfer failed due to the reason other than timeout, The error status
                                is returned in Status.
  @retval EFI_OUT_OF_RESOURCES  The request could not be submitted due to a lack of resources.
  @retval EFI_TIMEOUT           The transfer fails due to timeout.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB_IO_ISOCHRONOUS_TRANSFER)(
  IN EFI_USB_IO_PROTOCOL            *This,
  IN     UINT8                      DeviceEndpoint,
  IN OUT VOID                       *Data,
  IN     UINTN                      DataLength,
  OUT    UINT32                     *Status
  );

/**
  This function is used to manage a USB device with an isochronous transfer pipe. An Isochronous
  transfer is typically used to transfer streaming data.

  @param  This                  A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param  DeviceEndpoint        The destination USB device endpoint to which the
                                device request is being sent. DeviceEndpoint must
                                be between 0x01 and 0x0F or between 0x81 and 0x8F,
                                otherwise EFI_INVALID_PARAMETER is returned. If
                                the endpoint is not a BULK endpoint, EFI_INVALID_PARAMETER
                                is returned. The MSB of this parameter indicates
                                the endpoint direction. The number "1" stands for
                                an IN endpoint, and "0" stands for an OUT endpoint.
  @param  Data                  A pointer to the buffer of data that will be transmitted to USB
                                device or received from USB device.
  @param  DataLength            The size, in bytes, of the data buffer specified by Data.
                                This is an optional parameter and may be NULL.
  @param  IsochronousCallback   The IsochronousCallback() function.This function is
                                called if the requested isochronous transfer is completed.
  @param  Context               Data passed to the IsochronousCallback() function.

  @retval EFI_SUCCESS           The asynchronous isochronous transfer has been successfully submitted
                                to the system.
  @retval EFI_INVALID_PARAMETER The parameter DeviceEndpoint is not valid.
  @retval EFI_OUT_OF_RESOURCES  The request could not be submitted due to a lack of resources.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB_IO_ASYNC_ISOCHRONOUS_TRANSFER)(
  IN EFI_USB_IO_PROTOCOL              *This,
  IN UINT8                            DeviceEndpoint,
  IN OUT VOID                         *Data,
  IN     UINTN                        DataLength,
  IN EFI_ASYNC_USB_TRANSFER_CALLBACK  IsochronousCallBack,
  IN VOID                             *Context OPTIONAL
  );

/**
  Resets and reconfigures the USB controller. This function will work for all USB devices except
  USB Hub Controllers.

  @param  This                  A pointer to the EFI_USB_IO_PROTOCOL instance.

  @retval EFI_SUCCESS           The USB controller was reset.
  @retval EFI_INVALID_PARAMETER If the controller specified by This is a USB hub.
  @retval EFI_DEVICE_ERROR      An error occurred during the reconfiguration process.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB_IO_PORT_RESET)(
  IN EFI_USB_IO_PROTOCOL    *This
  );

/**
  Retrieves the USB Device Descriptor.

  @param  This                  A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param  DeviceDescriptor      A pointer to the caller allocated USB Device Descriptor.

  @retval EFI_SUCCESS           The device descriptor was retrieved successfully.
  @retval EFI_INVALID_PARAMETER DeviceDescriptor is NULL.
  @retval EFI_NOT_FOUND         The device descriptor was not found. The device may not be configured.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB_IO_GET_DEVICE_DESCRIPTOR)(
  IN EFI_USB_IO_PROTOCOL            *This,
  OUT EFI_USB_DEVICE_DESCRIPTOR     *DeviceDescriptor
  );

/**
  Retrieves the USB Device Descriptor.

  @param  This                    A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param  ConfigurationDescriptor A pointer to the caller allocated USB Active Configuration
                                  Descriptor.
  @retval EFI_SUCCESS             The active configuration descriptor was retrieved successfully.
  @retval EFI_INVALID_PARAMETER   ConfigurationDescriptor is NULL.
  @retval EFI_NOT_FOUND           An active configuration descriptor cannot be found. The device may not
                                  be configured.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB_IO_GET_CONFIG_DESCRIPTOR)(
  IN EFI_USB_IO_PROTOCOL            *This,
  OUT EFI_USB_CONFIG_DESCRIPTOR     *ConfigurationDescriptor
  );

/**
  Retrieves the Interface Descriptor for a USB Device Controller. As stated earlier, an interface
  within a USB device is equivalently to a USB Controller within the current configuration.

  @param  This                    A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param  InterfaceDescriptor     A pointer to the caller allocated USB Interface Descriptor within
                                  the configuration setting.
  @retval EFI_SUCCESS             The interface descriptor retrieved successfully.
  @retval EFI_INVALID_PARAMETER   InterfaceDescriptor is NULL.
  @retval EFI_NOT_FOUND           The interface descriptor cannot be found. The device may not be
                                  correctly configured.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB_IO_GET_INTERFACE_DESCRIPTOR)(
  IN EFI_USB_IO_PROTOCOL            *This,
  OUT EFI_USB_INTERFACE_DESCRIPTOR  *InterfaceDescriptor
  );

/**
  Retrieves an Endpoint Descriptor within a USB Controller.

  @param  This                    A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param  EndpointIndex           Indicates which endpoint descriptor to retrieve.
  @param  EndpointDescriptor      A pointer to the caller allocated USB Endpoint Descriptor of
                                  a USB controller.

  @retval EFI_SUCCESS             The endpoint descriptor was retrieved successfully.
  @retval EFI_INVALID_PARAMETER   One or more parameters are invalid.
  @retval EFI_NOT_FOUND           The endpoint descriptor cannot be found. The device may not be
                                  correctly configured.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB_IO_GET_ENDPOINT_DESCRIPTOR)(
  IN EFI_USB_IO_PROTOCOL            *This,
  IN  UINT8                         EndpointIndex,
  OUT EFI_USB_ENDPOINT_DESCRIPTOR   *EndpointDescriptor
  );

/**
  Retrieves a string stored in a USB Device.

  @param  This                    A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param  LangID                  The Language ID for the string being retrieved.
  @param  StringID                The ID of the string being retrieved.
  @param  String                  A pointer to a buffer allocated by this function with
                                  AllocatePool() to store the string.If this function
                                  returns EFI_SUCCESS, it stores the string the caller
                                  wants to get. The caller should release the string
                                  buffer with FreePool() after the string is not used any more.

  @retval EFI_SUCCESS             The string was retrieved successfully.
  @retval EFI_NOT_FOUND           The string specified by LangID and StringID was not found.
  @retval EFI_OUT_OF_RESOURCES    There are not enough resources to allocate the return buffer String.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB_IO_GET_STRING_DESCRIPTOR)(
  IN EFI_USB_IO_PROTOCOL            *This,
  IN  UINT16                        LangID,
  IN  UINT8                         StringID,
  OUT CHAR16                        **String
  );

/**
  Retrieves all the language ID codes that the USB device supports.

  @param  This                    A pointer to the EFI_USB_IO_PROTOCOL instance.
  @param  LangIDTable             Language ID for the string the caller wants to get.
                                  This is a 16-bit ID defined by Microsoft. This
                                  buffer pointer is allocated and maintained by
                                  the USB Bus Driver, the caller should not modify
                                  its contents.
  @param  TableSize               The size, in bytes, of the table LangIDTable.

  @retval EFI_SUCCESS             The support languages were retrieved successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB_IO_GET_SUPPORTED_LANGUAGE)(
  IN EFI_USB_IO_PROTOCOL            *This,
  OUT UINT16                        **LangIDTable,
  OUT UINT16                        *TableSize
  );

///
/// The EFI_USB_IO_PROTOCOL provides four basic transfers types described
/// in the USB 1.1 Specification. These include control transfer, interrupt
/// transfer, bulk transfer and isochronous transfer. The EFI_USB_IO_PROTOCOL
/// also provides some basic USB device/controller management and configuration
/// interfaces. A USB device driver uses the services of this protocol to manage USB devices.
///
struct _EFI_USB_IO_PROTOCOL {
  //
  // IO transfer
  //
  EFI_USB_IO_CONTROL_TRANSFER           UsbControlTransfer;
  EFI_USB_IO_BULK_TRANSFER              UsbBulkTransfer;
  EFI_USB_IO_ASYNC_INTERRUPT_TRANSFER   UsbAsyncInterruptTransfer;
  EFI_USB_IO_SYNC_INTERRUPT_TRANSFER    UsbSyncInterruptTransfer;
  EFI_USB_IO_ISOCHRONOUS_TRANSFER       UsbIsochronousTransfer;
  EFI_USB_IO_ASYNC_ISOCHRONOUS_TRANSFER UsbAsyncIsochronousTransfer;

  //
  // Common device request
  //
  EFI_USB_IO_GET_DEVICE_DESCRIPTOR      UsbGetDeviceDescriptor;
  EFI_USB_IO_GET_CONFIG_DESCRIPTOR      UsbGetConfigDescriptor;
  EFI_USB_IO_GET_INTERFACE_DESCRIPTOR   UsbGetInterfaceDescriptor;
  EFI_USB_IO_GET_ENDPOINT_DESCRIPTOR    UsbGetEndpointDescriptor;
  EFI_USB_IO_GET_STRING_DESCRIPTOR      UsbGetStringDescriptor;
  EFI_USB_IO_GET_SUPPORTED_LANGUAGE     UsbGetSupportedLanguages;

  //
  // Reset controller's parent port
  //
  EFI_USB_IO_PORT_RESET                 UsbPortReset;
};

extern EFI_GUID gEfiUsbIoProtocolGuid;

#endif
