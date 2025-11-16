/** @file
  Serial IO protocol as defined in the UEFI 2.0 specification.

  Abstraction of a basic serial device. Targeted at 16550 UART, but
  could be much more generic.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SERIAL_IO_PROTOCOL_H__
#define __SERIAL_IO_PROTOCOL_H__

#define EFI_SERIAL_IO_PROTOCOL_GUID \
  { \
    0xBB25CF6F, 0xF1D4, 0x11D2, {0x9A, 0x0C, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0xFD } \
  }

#define EFI_SERIAL_TERMINAL_DEVICE_TYPE_GUID \
  { \
    0X6AD9A60F, 0X5815, 0X4C7C, { 0X8A, 0X10, 0X50, 0X53, 0XD2, 0XBF, 0X7A, 0X1B } \
  }

///
/// Protocol GUID defined in EFI1.1.
///
#define SERIAL_IO_PROTOCOL  EFI_SERIAL_IO_PROTOCOL_GUID

typedef struct _EFI_SERIAL_IO_PROTOCOL EFI_SERIAL_IO_PROTOCOL;

///
/// Backward-compatible with EFI1.1.
///
typedef EFI_SERIAL_IO_PROTOCOL SERIAL_IO_INTERFACE;

///
/// Parity type that is computed or checked as each character is transmitted or received. If the
/// device does not support parity, the value is the default parity value.
///
typedef enum {
  DefaultParity,
  NoParity,
  EvenParity,
  OddParity,
  MarkParity,
  SpaceParity
} EFI_PARITY_TYPE;

///
/// Stop bits type
///
typedef enum {
  DefaultStopBits,
  OneStopBit,
  OneFiveStopBits,
  TwoStopBits
} EFI_STOP_BITS_TYPE;

//
// define for Control bits, grouped by read only, write only, and read write
//
//
// Read Only
//
#define EFI_SERIAL_CLEAR_TO_SEND        0x00000010
#define EFI_SERIAL_DATA_SET_READY       0x00000020
#define EFI_SERIAL_RING_INDICATE        0x00000040
#define EFI_SERIAL_CARRIER_DETECT       0x00000080
#define EFI_SERIAL_INPUT_BUFFER_EMPTY   0x00000100
#define EFI_SERIAL_OUTPUT_BUFFER_EMPTY  0x00000200

//
// Write Only
//
#define EFI_SERIAL_REQUEST_TO_SEND      0x00000002
#define EFI_SERIAL_DATA_TERMINAL_READY  0x00000001

//
// Read Write
//
#define EFI_SERIAL_HARDWARE_LOOPBACK_ENABLE      0x00001000
#define EFI_SERIAL_SOFTWARE_LOOPBACK_ENABLE      0x00002000
#define EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE  0x00004000

//
// Serial IO Member Functions
//

/**
  Reset the serial device.

  @param  This              Protocol instance pointer.

  @retval EFI_SUCCESS       The device was reset.
  @retval EFI_DEVICE_ERROR  The serial device could not be reset.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SERIAL_RESET)(
  IN EFI_SERIAL_IO_PROTOCOL *This
  );

/**
  Sets the baud rate, receive FIFO depth, transmit/receice time out, parity,
  data bits, and stop bits on a serial device.

  @param  This             Protocol instance pointer.
  @param  BaudRate         The requested baud rate. A BaudRate value of 0 will use the
                           device's default interface speed.
  @param  ReveiveFifoDepth The requested depth of the FIFO on the receive side of the
                           serial interface. A ReceiveFifoDepth value of 0 will use
                           the device's default FIFO depth.
  @param  Timeout          The requested time out for a single character in microseconds.
                           This timeout applies to both the transmit and receive side of the
                           interface. A Timeout value of 0 will use the device's default time
                           out value.
  @param  Parity           The type of parity to use on this serial device. A Parity value of
                           DefaultParity will use the device's default parity value.
  @param  DataBits         The number of data bits to use on the serial device. A DataBits
                           vaule of 0 will use the device's default data bit setting.
  @param  StopBits         The number of stop bits to use on this serial device. A StopBits
                           value of DefaultStopBits will use the device's default number of
                           stop bits.

  @retval EFI_SUCCESS           The device was reset.
  @retval EFI_INVALID_PARAMETER One or more attributes has an unsupported value.
  @retval EFI_DEVICE_ERROR      The serial device is not functioning correctly.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SERIAL_SET_ATTRIBUTES)(
  IN EFI_SERIAL_IO_PROTOCOL         *This,
  IN UINT64                         BaudRate,
  IN UINT32                         ReceiveFifoDepth,
  IN UINT32                         Timeout,
  IN EFI_PARITY_TYPE                Parity,
  IN UINT8                          DataBits,
  IN EFI_STOP_BITS_TYPE             StopBits
  );

/**
  Set the control bits on a serial device

  @param  This             Protocol instance pointer.
  @param  Control          Set the bits of Control that are settable.

  @retval EFI_SUCCESS      The new control bits were set on the serial device.
  @retval EFI_UNSUPPORTED  The serial device does not support this operation.
  @retval EFI_DEVICE_ERROR The serial device is not functioning correctly.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SERIAL_SET_CONTROL_BITS)(
  IN EFI_SERIAL_IO_PROTOCOL         *This,
  IN UINT32                         Control
  );

/**
  Retrieves the status of thecontrol bits on a serial device

  @param  This              Protocol instance pointer.
  @param  Control           A pointer to return the current Control signals from the serial device.

  @retval EFI_SUCCESS       The control bits were read from the serial device.
  @retval EFI_DEVICE_ERROR  The serial device is not functioning correctly.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SERIAL_GET_CONTROL_BITS)(
  IN EFI_SERIAL_IO_PROTOCOL         *This,
  OUT UINT32                        *Control
  );

/**
  Writes data to a serial device.

  @param  This              Protocol instance pointer.
  @param  BufferSize        On input, the size of the Buffer. On output, the amount of
                            data actually written.
  @param  Buffer            The buffer of data to write

  @retval EFI_SUCCESS       The data was written.
  @retval EFI_DEVICE_ERROR  The device reported an error.
  @retval EFI_TIMEOUT       The data write was stopped due to a timeout.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SERIAL_WRITE)(
  IN EFI_SERIAL_IO_PROTOCOL         *This,
  IN OUT UINTN                      *BufferSize,
  IN VOID                           *Buffer
  );

/**
  Writes data to a serial device.

  @param  This              Protocol instance pointer.
  @param  BufferSize        On input, the size of the Buffer. On output, the amount of
                            data returned in Buffer.
  @param  Buffer            The buffer to return the data into.

  @retval EFI_SUCCESS       The data was read.
  @retval EFI_DEVICE_ERROR  The device reported an error.
  @retval EFI_TIMEOUT       The data write was stopped due to a timeout.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SERIAL_READ)(
  IN EFI_SERIAL_IO_PROTOCOL         *This,
  IN OUT UINTN                      *BufferSize,
  OUT VOID                          *Buffer
  );

/**
  @par Data Structure Description:
  The data values in SERIAL_IO_MODE are read-only and are updated by the code
  that produces the SERIAL_IO_PROTOCOL member functions.

  @param ControlMask
  A mask for the Control bits that the device supports. The device
  must always support the Input Buffer Empty control bit.

  @param TimeOut
  If applicable, the number of microseconds to wait before timing out
  a Read or Write operation.

  @param BaudRate
  If applicable, the current baud rate setting of the device; otherwise,
  baud rate has the value of zero to indicate that device runs at the
  device's designed speed.

  @param ReceiveFifoDepth
  The number of characters the device will buffer on input

  @param DataBits
  The number of characters the device will buffer on input

  @param Parity
  If applicable, this is the EFI_PARITY_TYPE that is computed or
  checked as each character is transmitted or reveived. If the device
  does not support parity the value is the default parity value.

  @param StopBits
  If applicable, the EFI_STOP_BITS_TYPE number of stop bits per
  character. If the device does not support stop bits the value is
  the default stop bit values.

**/
typedef struct {
  UINT32    ControlMask;

  //
  // current Attributes
  //
  UINT32    Timeout;
  UINT64    BaudRate;
  UINT32    ReceiveFifoDepth;
  UINT32    DataBits;
  UINT32    Parity;
  UINT32    StopBits;
} EFI_SERIAL_IO_MODE;

#define EFI_SERIAL_IO_PROTOCOL_REVISION     0x00010000
#define EFI_SERIAL_IO_PROTOCOL_REVISION1p1  0x00010001
#define SERIAL_IO_INTERFACE_REVISION        EFI_SERIAL_IO_PROTOCOL_REVISION

///
/// The Serial I/O protocol is used to communicate with UART-style serial devices.
/// These can be standard UART serial ports in PC-AT systems, serial ports attached
/// to a USB interface, or potentially any character-based I/O device.
///
struct _EFI_SERIAL_IO_PROTOCOL {
  ///
  /// The revision to which the EFI_SERIAL_IO_PROTOCOL adheres. All future revisions
  /// must be backwards compatible. If a future version is not backwards compatible,
  /// it is not the same GUID.
  ///
  UINT32                         Revision;
  EFI_SERIAL_RESET               Reset;
  EFI_SERIAL_SET_ATTRIBUTES      SetAttributes;
  EFI_SERIAL_SET_CONTROL_BITS    SetControl;
  EFI_SERIAL_GET_CONTROL_BITS    GetControl;
  EFI_SERIAL_WRITE               Write;
  EFI_SERIAL_READ                Read;
  ///
  /// Pointer to SERIAL_IO_MODE data.
  ///
  EFI_SERIAL_IO_MODE             *Mode;
  ///
  /// Pointer to a GUID identifying the device connected to the serial port.
  /// This field is NULL when the protocol is installed by the serial port
  /// driver and may be populated by a platform driver for a serial port
  /// with a known device attached. The field will remain NULL if there is
  /// no platform serial device identification information available.
  ///
  CONST EFI_GUID                 *DeviceTypeGuid; // Revision 1.1
};

extern EFI_GUID  gEfiSerialIoProtocolGuid;
extern EFI_GUID  gEfiSerialTerminalDeviceTypeGuid;

#endif
