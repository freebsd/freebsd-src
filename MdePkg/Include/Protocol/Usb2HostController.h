/** @file
  EFI_USB2_HC_PROTOCOL as defined in UEFI 2.0.
  The USB Host Controller Protocol is used by code, typically USB bus drivers,
  running in the EFI boot services environment, to perform data transactions over
  a USB bus. In addition, it provides an abstraction for the root hub of the USB bus.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _USB2_HOSTCONTROLLER_H_
#define _USB2_HOSTCONTROLLER_H_

#include <Protocol/UsbIo.h>

#define EFI_USB2_HC_PROTOCOL_GUID \
  { \
    0x3e745226, 0x9818, 0x45b6, {0xa2, 0xac, 0xd7, 0xcd, 0xe, 0x8b, 0xa2, 0xbc } \
  }

///
/// Forward reference for pure ANSI compatability
///
typedef struct _EFI_USB2_HC_PROTOCOL EFI_USB2_HC_PROTOCOL;

typedef struct {
  UINT16    PortStatus;              ///< Contains current port status bitmap.
  UINT16    PortChangeStatus;        ///< Contains current port status change bitmap.
} EFI_USB_PORT_STATUS;

///
/// EFI_USB_PORT_STATUS.PortStatus bit definition
///
#define USB_PORT_STAT_CONNECTION   0x0001
#define USB_PORT_STAT_ENABLE       0x0002
#define USB_PORT_STAT_SUSPEND      0x0004
#define USB_PORT_STAT_OVERCURRENT  0x0008
#define USB_PORT_STAT_RESET        0x0010
#define USB_PORT_STAT_POWER        0x0100
#define USB_PORT_STAT_LOW_SPEED    0x0200
#define USB_PORT_STAT_HIGH_SPEED   0x0400
#define USB_PORT_STAT_SUPER_SPEED  0x0800
#define USB_PORT_STAT_OWNER        0x2000

///
/// EFI_USB_PORT_STATUS.PortChangeStatus bit definition
///
#define USB_PORT_STAT_C_CONNECTION   0x0001
#define USB_PORT_STAT_C_ENABLE       0x0002
#define USB_PORT_STAT_C_SUSPEND      0x0004
#define USB_PORT_STAT_C_OVERCURRENT  0x0008
#define USB_PORT_STAT_C_RESET        0x0010

///
/// Usb port features value
/// Each value indicates its bit index in the port status and status change bitmaps,
/// if combines these two bitmaps into a 32-bit bitmap.
///
typedef enum {
  EfiUsbPortEnable            = 1,
  EfiUsbPortSuspend           = 2,
  EfiUsbPortReset             = 4,
  EfiUsbPortPower             = 8,
  EfiUsbPortOwner             = 13,
  EfiUsbPortConnectChange     = 16,
  EfiUsbPortEnableChange      = 17,
  EfiUsbPortSuspendChange     = 18,
  EfiUsbPortOverCurrentChange = 19,
  EfiUsbPortResetChange       = 20
} EFI_USB_PORT_FEATURE;

#define EFI_USB_SPEED_FULL   0x0000     ///< 12 Mb/s, USB 1.1 OHCI and UHCI HC.
#define EFI_USB_SPEED_LOW    0x0001     ///< 1 Mb/s, USB 1.1 OHCI and UHCI HC.
#define EFI_USB_SPEED_HIGH   0x0002     ///< 480 Mb/s, USB 2.0 EHCI HC.
#define EFI_USB_SPEED_SUPER  0x0003     ///< 4.8 Gb/s, USB 3.0 XHCI HC.

typedef struct {
  UINT8    TranslatorHubAddress;   ///< device address
  UINT8    TranslatorPortNumber;   ///< the port number of the hub that device is connected to.
} EFI_USB2_HC_TRANSACTION_TRANSLATOR;

//
// Protocol definitions
//

/**
  Retrieves the Host Controller capabilities.

  @param  This           A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  MaxSpeed       Host controller data transfer speed.
  @param  PortNumber     Number of the root hub ports.
  @param  Is64BitCapable TRUE if controller supports 64-bit memory addressing,
                         FALSE otherwise.

  @retval EFI_SUCCESS           The host controller capabilities were retrieved successfully.
  @retval EFI_INVALID_PARAMETER One of the input args was NULL.
  @retval EFI_DEVICE_ERROR      An error was encountered while attempting to
                                retrieve the capabilities.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB2_HC_PROTOCOL_GET_CAPABILITY)(
  IN  EFI_USB2_HC_PROTOCOL  *This,
  OUT UINT8                 *MaxSpeed,
  OUT UINT8                 *PortNumber,
  OUT UINT8                 *Is64BitCapable
  );

#define EFI_USB_HC_RESET_GLOBAL             0x0001
#define EFI_USB_HC_RESET_HOST_CONTROLLER    0x0002
#define EFI_USB_HC_RESET_GLOBAL_WITH_DEBUG  0x0004
#define EFI_USB_HC_RESET_HOST_WITH_DEBUG    0x0008

/**
  Provides software reset for the USB host controller.

  @param  This       A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  Attributes A bit mask of the reset operation to perform.

  @retval EFI_SUCCESS           The reset operation succeeded.
  @retval EFI_INVALID_PARAMETER Attributes is not valid.
  @retval EFI_UNSUPPORTED       The type of reset specified by Attributes is not currently
                                supported by the host controller hardware.
  @retval EFI_ACCESS_DENIED     Reset operation is rejected due to the debug port being configured
                                and active; only EFI_USB_HC_RESET_GLOBAL_WITH_DEBUG or
                                EFI_USB_HC_RESET_HOST_WITH_DEBUG reset Attributes can be used to
                                perform reset operation for this host controller.
  @retval EFI_DEVICE_ERROR      An error was encountered while attempting to
                                retrieve the capabilities.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB2_HC_PROTOCOL_RESET)(
  IN EFI_USB2_HC_PROTOCOL   *This,
  IN UINT16                 Attributes
  );

/**
  Enumration value for status of USB HC.
**/
typedef enum {
  EfiUsbHcStateHalt,                ///< The host controller is in halt
                                    ///< state. No USB transactions can occur
                                    ///< while in this state. The host
                                    ///< controller can enter this state for
                                    ///< three reasons: 1) After host
                                    ///< controller hardware reset. 2)
                                    ///< Explicitly set by software. 3)
                                    ///< Triggered by a fatal error such as
                                    ///< consistency check failure.

  EfiUsbHcStateOperational,         ///< The host controller is in an
                                    ///< operational state. When in
                                    ///< this state, the host
                                    ///< controller can execute bus
                                    ///< traffic. This state must be
                                    ///< explicitly set to enable the
                                    ///< USB bus traffic.

  EfiUsbHcStateSuspend,             ///< The host controller is in the
                                    ///< suspend state. No USB
                                    ///< transactions can occur while in
                                    ///< this state. The host controller
                                    ///< enters this state for the
                                    ///< following reasons: 1) Explicitly
                                    ///< set by software. 2) Triggered
                                    ///< when there is no bus traffic for
                                    ///< 3 microseconds.

  EfiUsbHcStateMaximum              ///< Maximum value for enumration value of HC status.
} EFI_USB_HC_STATE;

/**
  Retrieves current state of the USB host controller.

  @param  This  A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  State A pointer to the EFI_USB_HC_STATE data structure that
                indicates current state of the USB host controller.

  @retval EFI_SUCCESS           The state information of the host controller was returned in State.
  @retval EFI_INVALID_PARAMETER State is NULL.
  @retval EFI_DEVICE_ERROR      An error was encountered while attempting to retrieve the
                                host controller's current state.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB2_HC_PROTOCOL_GET_STATE)(
  IN        EFI_USB2_HC_PROTOCOL    *This,
  OUT       EFI_USB_HC_STATE        *State
  );

/**
  Sets the USB host controller to a specific state.

  @param  This  A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  State Indicates the state of the host controller that will be set.

  @retval EFI_SUCCESS           The USB host controller was successfully placed in the state
                                specified by State.
  @retval EFI_INVALID_PARAMETER State is not valid.
  @retval EFI_DEVICE_ERROR      Failed to set the state specified by State due to device error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB2_HC_PROTOCOL_SET_STATE)(
  IN EFI_USB2_HC_PROTOCOL    *This,
  IN EFI_USB_HC_STATE        State
  );

/**
  Submits control transfer to a target USB device.

  @param  This                A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  DeviceAddress       Represents the address of the target device on the USB.
  @param  DeviceSpeed         Indicates device speed.
  @param  MaximumPacketLength Indicates the maximum packet size that the default control transfer
                              endpoint is capable of sending or receiving.
  @param  Request             A pointer to the USB device request that will be sent to the USB device.
  @param  TransferDirection   Specifies the data direction for the transfer. There are three values
                              available, EfiUsbDataIn, EfiUsbDataOut and EfiUsbNoData.
  @param  Data                A pointer to the buffer of data that will be transmitted to USB device or
                              received from USB device.
  @param  DataLength          On input, indicates the size, in bytes, of the data buffer specified by Data.
                              On output, indicates the amount of data actually transferred.
  @param  TimeOut             Indicates the maximum time, in milliseconds, which the transfer is
                              allowed to complete.
  @param  Translator          A pointer to the transaction translator data.
  @param  TransferResult      A pointer to the detailed result information generated by this control
                              transfer.

  @retval EFI_SUCCESS           The control transfer was completed successfully.
  @retval EFI_INVALID_PARAMETER Some parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The control transfer could not be completed due to a lack of resources.
  @retval EFI_TIMEOUT           The control transfer failed due to timeout.
  @retval EFI_DEVICE_ERROR      The control transfer failed due to host controller or device error.
                                Caller should check TransferResult for detailed error information.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB2_HC_PROTOCOL_CONTROL_TRANSFER)(
  IN     EFI_USB2_HC_PROTOCOL               *This,
  IN     UINT8                              DeviceAddress,
  IN     UINT8                              DeviceSpeed,
  IN     UINTN                              MaximumPacketLength,
  IN     EFI_USB_DEVICE_REQUEST             *Request,
  IN     EFI_USB_DATA_DIRECTION             TransferDirection,
  IN OUT VOID                               *Data       OPTIONAL,
  IN OUT UINTN                              *DataLength OPTIONAL,
  IN     UINTN                              TimeOut,
  IN     EFI_USB2_HC_TRANSACTION_TRANSLATOR *Translator,
  OUT    UINT32                             *TransferResult
  );

#define EFI_USB_MAX_BULK_BUFFER_NUM  10

/**
  Submits bulk transfer to a bulk endpoint of a USB device.

  @param  This                A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  DeviceAddress       Represents the address of the target device on the USB.
  @param  EndPointAddress     The combination of an endpoint number and an endpoint direction of the
                              target USB device.
  @param  DeviceSpeed         Indicates device speed.
  @param  MaximumPacketLength Indicates the maximum packet size the target endpoint is capable of
                              sending or receiving.
  @param  DataBuffersNumber   Number of data buffers prepared for the transfer.
  @param  Data                Array of pointers to the buffers of data that will be transmitted to USB
                              device or received from USB device.
  @param  DataLength          When input, indicates the size, in bytes, of the data buffers specified by
                              Data. When output, indicates the actually transferred data size.
  @param  DataToggle          A pointer to the data toggle value.
  @param  TimeOut             Indicates the maximum time, in milliseconds, which the transfer is
                              allowed to complete.
  @param  Translator          A pointer to the transaction translator data.
  @param  TransferResult      A pointer to the detailed result information of the bulk transfer.

  @retval EFI_SUCCESS           The bulk transfer was completed successfully.
  @retval EFI_INVALID_PARAMETER Some parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The bulk transfer could not be submitted due to a lack of resources.
  @retval EFI_TIMEOUT           The bulk transfer failed due to timeout.
  @retval EFI_DEVICE_ERROR      The bulk transfer failed due to host controller or device error.
                                Caller should check TransferResult for detailed error information.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB2_HC_PROTOCOL_BULK_TRANSFER)(
  IN     EFI_USB2_HC_PROTOCOL               *This,
  IN     UINT8                              DeviceAddress,
  IN     UINT8                              EndPointAddress,
  IN     UINT8                              DeviceSpeed,
  IN     UINTN                              MaximumPacketLength,
  IN     UINT8                              DataBuffersNumber,
  IN OUT VOID                               *Data[EFI_USB_MAX_BULK_BUFFER_NUM],
  IN OUT UINTN                              *DataLength,
  IN OUT UINT8                              *DataToggle,
  IN     UINTN                              TimeOut,
  IN     EFI_USB2_HC_TRANSACTION_TRANSLATOR *Translator,
  OUT    UINT32                             *TransferResult
  );

/**
  Submits an asynchronous interrupt transfer to an interrupt endpoint of a USB device.
  Translator parameter doesn't exist in UEFI2.0 spec, but it will be updated in the following specification version.

  @param  This                A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  DeviceAddress       Represents the address of the target device on the USB.
  @param  EndPointAddress     The combination of an endpoint number and an endpoint direction of the
                              target USB device.
  @param  DeviceSpeed         Indicates device speed.
  @param  MaximumPacketLength Indicates the maximum packet size the target endpoint is capable of
                              sending or receiving.
  @param  IsNewTransfer       If TRUE, an asynchronous interrupt pipe is built between the host and the
                              target interrupt endpoint. If FALSE, the specified asynchronous interrupt
                              pipe is canceled. If TRUE, and an interrupt transfer exists for the target
                              end point, then EFI_INVALID_PARAMETER is returned.
  @param  DataToggle          A pointer to the data toggle value.
  @param  PollingInterval     Indicates the interval, in milliseconds, that the asynchronous interrupt
                              transfer is polled.
  @param  DataLength          Indicates the length of data to be received at the rate specified by
                              PollingInterval from the target asynchronous interrupt endpoint.
  @param  Translator          A pointr to the transaction translator data.
  @param  CallBackFunction    The Callback function. This function is called at the rate specified by
                              PollingInterval.
  @param  Context             The context that is passed to the CallBackFunction. This is an
                              optional parameter and may be NULL.

  @retval EFI_SUCCESS           The asynchronous interrupt transfer request has been successfully
                                submitted or canceled.
  @retval EFI_INVALID_PARAMETER Some parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB2_HC_PROTOCOL_ASYNC_INTERRUPT_TRANSFER)(
  IN     EFI_USB2_HC_PROTOCOL                                *This,
  IN     UINT8                                               DeviceAddress,
  IN     UINT8                                               EndPointAddress,
  IN     UINT8                                               DeviceSpeed,
  IN     UINTN                                               MaxiumPacketLength,
  IN     BOOLEAN                                             IsNewTransfer,
  IN OUT UINT8                                               *DataToggle,
  IN     UINTN                                               PollingInterval  OPTIONAL,
  IN     UINTN                                               DataLength       OPTIONAL,
  IN     EFI_USB2_HC_TRANSACTION_TRANSLATOR                  *Translator      OPTIONAL,
  IN     EFI_ASYNC_USB_TRANSFER_CALLBACK                     CallBackFunction OPTIONAL,
  IN     VOID                                                *Context         OPTIONAL
  );

/**
  Submits synchronous interrupt transfer to an interrupt endpoint of a USB device.
  Translator parameter doesn't exist in UEFI2.0 spec, but it will be updated in the following specification version.

  @param  This                  A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  DeviceAddress         Represents the address of the target device on the USB.
  @param  EndPointAddress       The combination of an endpoint number and an endpoint direction of the
                                target USB device.
  @param  DeviceSpeed           Indicates device speed.
  @param  MaximumPacketLength   Indicates the maximum packet size the target endpoint is capable of
                                sending or receiving.
  @param  Data                  A pointer to the buffer of data that will be transmitted to USB device or
                                received from USB device.
  @param  DataLength            On input, the size, in bytes, of the data buffer specified by Data. On
                                output, the number of bytes transferred.
  @param  DataToggle            A pointer to the data toggle value.
  @param  TimeOut               Indicates the maximum time, in milliseconds, which the transfer is
                                allowed to complete.
  @param  Translator            A pointr to the transaction translator data.
  @param  TransferResult        A pointer to the detailed result information from the synchronous
                                interrupt transfer.

  @retval EFI_SUCCESS           The synchronous interrupt transfer was completed successfully.
  @retval EFI_INVALID_PARAMETER Some parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The synchronous interrupt transfer could not be submitted due to a lack of resources.
  @retval EFI_TIMEOUT           The synchronous interrupt transfer failed due to timeout.
  @retval EFI_DEVICE_ERROR      The synchronous interrupt transfer failed due to host controller or device error.
                                Caller should check TransferResult for detailed error information.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB2_HC_PROTOCOL_SYNC_INTERRUPT_TRANSFER)(
  IN     EFI_USB2_HC_PROTOCOL                        *This,
  IN     UINT8                                       DeviceAddress,
  IN     UINT8                                       EndPointAddress,
  IN     UINT8                                       DeviceSpeed,
  IN     UINTN                                       MaximumPacketLength,
  IN OUT VOID                                        *Data,
  IN OUT UINTN                                       *DataLength,
  IN OUT UINT8                                       *DataToggle,
  IN     UINTN                                       TimeOut,
  IN     EFI_USB2_HC_TRANSACTION_TRANSLATOR          *Translator,
  OUT    UINT32                                      *TransferResult
  );

#define EFI_USB_MAX_ISO_BUFFER_NUM   7
#define EFI_USB_MAX_ISO_BUFFER_NUM1  2

/**
  Submits isochronous transfer to an isochronous endpoint of a USB device.

  This function is used to submit isochronous transfer to a target endpoint of a USB device.
  The target endpoint is specified by DeviceAddressand EndpointAddress. Isochronous transfers are
  used when working with isochronous date. It provides periodic, continuous communication between
  the host and a device. Isochronous transfers can beused only by full-speed, high-speed, and
  super-speed devices.

  High-speed isochronous transfers can be performed using multiple data buffers. The number of
  buffers that are actually prepared for the transfer is specified by DataBuffersNumber. For
  full-speed isochronous transfers this value is ignored.

  Data represents a list of pointers to the data buffers. For full-speed isochronous transfers
  only the data pointed by Data[0]shall be used. For high-speed isochronous transfers and for
  the split transactions depending on DataLengththere several data buffers canbe used. For the
  high-speed isochronous transfers the total number of buffers must not exceed EFI_USB_MAX_ISO_BUFFER_NUM.

  For split transactions performed on full-speed device by high-speed host controller the total
  number of buffers is limited to EFI_USB_MAX_ISO_BUFFER_NUM1.
  If the isochronous transfer is successful, then EFI_SUCCESSis returned. The isochronous transfer
  is designed to be completed within one USB frame time, if it cannot be completed, EFI_TIMEOUT
  is returned. If an error other than timeout occurs during the USB transfer, then EFI_DEVICE_ERROR
  is returned and the detailed status code will be returned in TransferResult.

  EFI_INVALID_PARAMETERis returned if one of the following conditionsis satisfied:
    - Data is NULL.
    - DataLength is 0.
    - DeviceSpeed is not one of the supported values listed above.
    - MaximumPacketLength is invalid. MaximumPacketLength must be 1023 or less for full-speed devices,
      and 1024 or less for high-speed and super-speed devices.
    - TransferResult is NULL.

  @param  This                  A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  DeviceAddress         Represents the address of the target device on the USB.
  @param  EndPointAddress       The combination of an endpoint number and an endpoint direction of the
                                target USB device.
  @param  DeviceSpeed           Indicates device speed. The supported values are EFI_USB_SPEED_FULL,
                                EFI_USB_SPEED_HIGH, or EFI_USB_SPEED_SUPER.
  @param  MaximumPacketLength   Indicates the maximum packet size the target endpoint is capable of
                                sending or receiving.
  @param  DataBuffersNumber     Number of data buffers prepared for the transfer.
  @param  Data                  Array of pointers to the buffers of data that will be transmitted to USB
                                device or received from USB device.
  @param  DataLength            Specifies the length, in bytes, of the data to be sent to or received from
                                the USB device.
  @param  Translator            A pointer to the transaction translator data.
  @param  TransferResult        A pointer to the detailed result information of the isochronous transfer.

  @retval EFI_SUCCESS           The isochronous transfer was completed successfully.
  @retval EFI_INVALID_PARAMETER Some parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The isochronous transfer could not be submitted due to a lack of resources.
  @retval EFI_TIMEOUT           The isochronous transfer cannot be completed within the one USB frame time.
  @retval EFI_DEVICE_ERROR      The isochronous transfer failed due to host controller or device error.
                                Caller should check TransferResult for detailed error information.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB2_HC_PROTOCOL_ISOCHRONOUS_TRANSFER)(
  IN     EFI_USB2_HC_PROTOCOL               *This,
  IN     UINT8                              DeviceAddress,
  IN     UINT8                              EndPointAddress,
  IN     UINT8                              DeviceSpeed,
  IN     UINTN                              MaximumPacketLength,
  IN     UINT8                              DataBuffersNumber,
  IN OUT VOID                               *Data[EFI_USB_MAX_ISO_BUFFER_NUM],
  IN     UINTN                              DataLength,
  IN     EFI_USB2_HC_TRANSACTION_TRANSLATOR *Translator,
  OUT    UINT32                             *TransferResult
  );

/**
  Submits nonblocking isochronous transfer to an isochronous endpoint of a USB device.

  This is an asynchronous type of USB isochronous transfer. If the caller submits a USB
  isochronous transfer request through this function, this function will return immediately.

  When the isochronous transfer completes, the IsochronousCallbackfunction will be triggered,
  the caller can know the transfer results. If the transfer is successful, the caller can get
  the data received or sent in this callback function.

  The target endpoint is specified by DeviceAddressand EndpointAddress. Isochronous transfers
  are used when working with isochronous date. It provides periodic, continuous communication
  between the host and a device. Isochronous transfers can be used only by full-speed, high-speed,
  and super-speed devices.

  High-speed isochronous transfers can be performed using multiple data buffers. The number of
  buffers that are actually prepared for the transfer is specified by DataBuffersNumber. For
  full-speed isochronous transfers this value is ignored.

  Data represents a list of pointers to the data buffers. For full-speed isochronous transfers
  only the data pointed by Data[0] shall be used. For high-speed isochronous transfers and for
  the split transactions depending on DataLength there several data buffers can be used. For
  the high-speed isochronous transfers the total number of buffers must not exceed EFI_USB_MAX_ISO_BUFFER_NUM.

  For split transactions performed on full-speed device by high-speed host controller the total
  number of buffers is limited to EFI_USB_MAX_ISO_BUFFER_NUM1.

  EFI_INVALID_PARAMETER is returned if one of the following conditionsis satisfied:
    - Data is NULL.
    - DataLength is 0.
    - DeviceSpeed is not one of the supported values listed above.
    - MaximumPacketLength is invalid. MaximumPacketLength must be 1023 or less for full-speed
      devices and 1024 or less for high-speed and super-speed devices.

  @param  This                  A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  DeviceAddress         Represents the address of the target device on the USB.
  @param  EndPointAddress       The combination of an endpoint number and an endpoint direction of the
                                target USB device.
  @param  DeviceSpeed           Indicates device speed. The supported values are EFI_USB_SPEED_FULL,
                                EFI_USB_SPEED_HIGH, or EFI_USB_SPEED_SUPER.
  @param  MaximumPacketLength   Indicates the maximum packet size the target endpoint is capable of
                                sending or receiving.
  @param  DataBuffersNumber     Number of data buffers prepared for the transfer.
  @param  Data                  Array of pointers to the buffers of data that will be transmitted to USB
                                device or received from USB device.
  @param  DataLength            Specifies the length, in bytes, of the data to be sent to or received from
                                the USB device.
  @param  Translator            A pointer to the transaction translator data.
  @param  IsochronousCallback   The Callback function. This function is called if the requested
                                isochronous transfer is completed.
  @param  Context               Data passed to the IsochronousCallback function. This is an
                                optional parameter and may be NULL.

  @retval EFI_SUCCESS           The asynchronous isochronous transfer request has been successfully
                                submitted or canceled.
  @retval EFI_INVALID_PARAMETER Some parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The asynchronous isochronous transfer could not be submitted due to
                                a lack of resources.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB2_HC_PROTOCOL_ASYNC_ISOCHRONOUS_TRANSFER)(
  IN     EFI_USB2_HC_PROTOCOL               *This,
  IN     UINT8                              DeviceAddress,
  IN     UINT8                              EndPointAddress,
  IN     UINT8                              DeviceSpeed,
  IN     UINTN                              MaximumPacketLength,
  IN     UINT8                              DataBuffersNumber,
  IN OUT VOID                               *Data[EFI_USB_MAX_ISO_BUFFER_NUM],
  IN     UINTN                              DataLength,
  IN     EFI_USB2_HC_TRANSACTION_TRANSLATOR *Translator,
  IN     EFI_ASYNC_USB_TRANSFER_CALLBACK    IsochronousCallBack,
  IN     VOID                               *Context OPTIONAL
  );

/**
  Retrieves the current status of a USB root hub port.

  @param  This       A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  PortNumber Specifies the root hub port from which the status is to be retrieved.
                     This value is zero based.
  @param  PortStatus A pointer to the current port status bits and port status change bits.

  @retval EFI_SUCCESS           The status of the USB root hub port specified by PortNumber
                                was returned in PortStatus.
  @retval EFI_INVALID_PARAMETER PortNumber is invalid.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB2_HC_PROTOCOL_GET_ROOTHUB_PORT_STATUS)(
  IN        EFI_USB2_HC_PROTOCOL    *This,
  IN        UINT8                   PortNumber,
  OUT       EFI_USB_PORT_STATUS     *PortStatus
  );

/**
  Sets a feature for the specified root hub port.

  @param  This        A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  PortNumber  Specifies the root hub port whose feature is requested to be set. This
                      value is zero based.
  @param  PortFeature Indicates the feature selector associated with the feature set request.

  @retval EFI_SUCCESS           The feature specified by PortFeature was set for the USB
                                root hub port specified by PortNumber.
  @retval EFI_INVALID_PARAMETER PortNumber is invalid or PortFeature is invalid for this function.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB2_HC_PROTOCOL_SET_ROOTHUB_PORT_FEATURE)(
  IN EFI_USB2_HC_PROTOCOL    *This,
  IN UINT8                   PortNumber,
  IN EFI_USB_PORT_FEATURE    PortFeature
  );

/**
  Clears a feature for the specified root hub port.

  @param  This        A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  PortNumber  Specifies the root hub port whose feature is requested to be cleared. This
                      value is zero based.
  @param  PortFeature Indicates the feature selector associated with the feature clear request.

  @retval EFI_SUCCESS           The feature specified by PortFeature was cleared for the USB
                                root hub port specified by PortNumber.
  @retval EFI_INVALID_PARAMETER PortNumber is invalid or PortFeature is invalid for this function.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_USB2_HC_PROTOCOL_CLEAR_ROOTHUB_PORT_FEATURE)(
  IN EFI_USB2_HC_PROTOCOL    *This,
  IN UINT8                   PortNumber,
  IN EFI_USB_PORT_FEATURE    PortFeature
  );

///
/// The EFI_USB2_HC_PROTOCOL provides USB host controller management, basic
/// data transactions over a USB bus, and USB root hub access. A device driver
/// that wishes to manage a USB bus in a system retrieves the EFI_USB2_HC_PROTOCOL
/// instance that is associated with the USB bus to be managed. A device handle
/// for a USB host controller will minimally contain an EFI_DEVICE_PATH_PROTOCOL
/// instance, and an EFI_USB2_HC_PROTOCOL instance.
///
struct _EFI_USB2_HC_PROTOCOL {
  EFI_USB2_HC_PROTOCOL_GET_CAPABILITY                GetCapability;
  EFI_USB2_HC_PROTOCOL_RESET                         Reset;
  EFI_USB2_HC_PROTOCOL_GET_STATE                     GetState;
  EFI_USB2_HC_PROTOCOL_SET_STATE                     SetState;
  EFI_USB2_HC_PROTOCOL_CONTROL_TRANSFER              ControlTransfer;
  EFI_USB2_HC_PROTOCOL_BULK_TRANSFER                 BulkTransfer;
  EFI_USB2_HC_PROTOCOL_ASYNC_INTERRUPT_TRANSFER      AsyncInterruptTransfer;
  EFI_USB2_HC_PROTOCOL_SYNC_INTERRUPT_TRANSFER       SyncInterruptTransfer;
  EFI_USB2_HC_PROTOCOL_ISOCHRONOUS_TRANSFER          IsochronousTransfer;
  EFI_USB2_HC_PROTOCOL_ASYNC_ISOCHRONOUS_TRANSFER    AsyncIsochronousTransfer;
  EFI_USB2_HC_PROTOCOL_GET_ROOTHUB_PORT_STATUS       GetRootHubPortStatus;
  EFI_USB2_HC_PROTOCOL_SET_ROOTHUB_PORT_FEATURE      SetRootHubPortFeature;
  EFI_USB2_HC_PROTOCOL_CLEAR_ROOTHUB_PORT_FEATURE    ClearRootHubPortFeature;

  ///
  /// The major revision number of the USB host controller. The revision information
  /// indicates the release of the Universal Serial Bus Specification with which the
  /// host controller is compliant.
  ///
  UINT16                                             MajorRevision;

  ///
  /// The minor revision number of the USB host controller. The revision information
  /// indicates the release of the Universal Serial Bus Specification with which the
  /// host controller is compliant.
  ///
  UINT16                                             MinorRevision;
};

extern EFI_GUID  gEfiUsb2HcProtocolGuid;

#endif
