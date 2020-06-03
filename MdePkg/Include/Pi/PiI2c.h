/** @file
  Include file matches things in PI.

Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  PI Version 1.3

**/

#ifndef __PI_I2C_H__
#define __PI_I2C_H__

///
/// A 10-bit slave address is or'ed with the following value enabling the
/// I2C protocol stack to address the duplicated address space between 0
//  and 127 in 10-bit mode.
///
#define I2C_ADDRESSING_10_BIT     0x80000000

///
/// I2C controller capabilities
///
/// The EFI_I2C_CONTROLLER_CAPABILITIES specifies the capabilities of the
/// I2C host controller.  The StructureSizeInBytes enables variations of
/// this structure to be identified if there is need to extend this
/// structure in the future.
///
typedef struct {
  ///
  /// Length of this data structure in bytes
  ///
  UINT32 StructureSizeInBytes;

  ///
  /// The maximum number of bytes the I2C host controller is able to
  /// receive from the I2C bus.
  ///
  UINT32 MaximumReceiveBytes;

  ///
  /// The maximum number of bytes the I2C host controller is able to send
  /// on the I2C  bus.
  ///
  UINT32 MaximumTransmitBytes;

  ///
  /// The maximum number of bytes in the I2C bus transaction.
  ///
  UINT32 MaximumTotalBytes;
} EFI_I2C_CONTROLLER_CAPABILITIES;

///
/// I2C device description
///
/// The EFI_I2C_ENUMERATE_PROTOCOL uses the EFI_I2C_DEVICE to describe
/// the platform specific details associated with an I2C device.  This
/// description is passed to the I2C bus driver during enumeration where
/// it is made available to the third party I2C device driver via the
/// EFI_I2C_IO_PROTOCOL.
///
typedef struct {
  ///
  /// Unique value assigned by the silicon manufacture or the third
  /// party I2C driver writer for the I2C part.  This value logically
  /// combines both the manufacture name and the I2C part number into
  /// a single value specified as a GUID.
  ///
  CONST EFI_GUID *DeviceGuid;

  ///
  /// Unique ID of the I2C part within the system
  ///
  UINT32 DeviceIndex;

  ///
  /// Hardware revision - ACPI _HRV value.  See the Advanced
  /// Configuration and Power Interface Specification, Revision 5.0
  /// for the field format and the Plug and play support for I2C
  /// web-page for restriction on values.
  ///
  /// http://www.acpi.info/spec.htm
  /// http://msdn.microsoft.com/en-us/library/windows/hardware/jj131711(v=vs.85).aspx
  ///
  UINT32 HardwareRevision;

  ///
  /// I2C bus configuration for the I2C device
  ///
  UINT32 I2cBusConfiguration;

  ///
  /// Number of slave addresses for the I2C device.
  ///
  UINT32 SlaveAddressCount;

  ///
  /// Pointer to the array of slave addresses for the I2C device.
  ///
  CONST UINT32 *SlaveAddressArray;
} EFI_I2C_DEVICE;

///
/// Define the I2C flags
///
/// I2C read operation when set
#define I2C_FLAG_READ               0x00000001

///
/// Define the flags for SMBus operation
///
/// The following flags are also present in only the first I2C operation
/// and are ignored when present in other operations.  These flags
/// describe a particular SMB transaction as shown in the following table.
///

/// SMBus operation
#define I2C_FLAG_SMBUS_OPERATION    0x00010000

/// SMBus block operation
///   The flag I2C_FLAG_SMBUS_BLOCK causes the I2C master protocol to update
///   the LengthInBytes field of the operation in the request packet with
///   the actual number of bytes read or written.  These values are only
///   valid when the entire I2C transaction is successful.
///   This flag also changes the LengthInBytes meaning to be: A maximum
///   of LengthInBytes is to be read from the device.  The first byte
///   read contains the number of bytes remaining to be read, plus an
///   optional PEC value.
#define I2C_FLAG_SMBUS_BLOCK        0x00020000

/// SMBus process call operation
#define I2C_FLAG_SMBUS_PROCESS_CALL 0x00040000

/// SMBus use packet error code (PEC)
///   Note that the I2C master protocol may clear the I2C_FLAG_SMBUS_PEC bit
///   to indicate that the PEC value was checked by the hardware and is
///   not appended to the returned read data.
///
#define I2C_FLAG_SMBUS_PEC          0x00080000

//----------------------------------------------------------------------
///
/// QuickRead:          OperationCount=1,
///                     LengthInBytes=0,   Flags=I2C_FLAG_READ
/// QuickWrite:         OperationCount=1,
///                     LengthInBytes=0,   Flags=0
///
///
/// ReceiveByte:        OperationCount=1,
///                     LengthInBytes=1,   Flags=I2C_FLAG_SMBUS_OPERATION
///                                            | I2C_FLAG_READ
/// ReceiveByte+PEC:    OperationCount=1,
///                     LengthInBytes=2,   Flags=I2C_FLAG_SMBUS_OPERATION
///                                            | I2C_FLAG_READ
///                                            | I2C_FLAG_SMBUS_PEC
///
///
/// SendByte:           OperationCount=1,
///                     LengthInBytes=1,   Flags=I2C_FLAG_SMBUS_OPERATION
/// SendByte+PEC:       OperationCount=1,
///                     LengthInBytes=2,   Flags=I2C_FLAG_SMBUS_OPERATION
///                                            | I2C_FLAG_SMBUS_PEC
///
///
/// ReadDataByte:       OperationCount=2,
///                     LengthInBytes=1,   Flags=I2C_FLAG_SMBUS_OPERATION
///                     LengthInBytes=1,   Flags=I2C_FLAG_READ
/// ReadDataByte+PEC:   OperationCount=2,
///                     LengthInBytes=1,   Flags=I2C_FLAG_SMBUS_OPERATION
///                                            | I2C_FLAG_SMBUS_PEC
///                     LengthInBytes=2,   Flags=I2C_FLAG_READ
///
///
/// WriteDataByte:      OperationCount=1,
///                     LengthInBytes=2,   Flags=I2C_FLAG_SMBUS_OPERATION
/// WriteDataByte+PEC:  OperationCount=1,
///                     LengthInBytes=3,   Flags=I2C_FLAG_SMBUS_OPERATION
///                                            | I2C_FLAG_SMBUS_PEC
///
///
/// ReadDataWord:       OperationCount=2,
///                     LengthInBytes=1,   Flags=I2C_FLAG_SMBUS_OPERATION
///                     LengthInBytes=2,   Flags=I2C_FLAG_READ
/// ReadDataWord+PEC:   OperationCount=2,
///                     LengthInBytes=1,   Flags=I2C_FLAG_SMBUS_OPERATION
///                                            | I2C_FLAG_SMBUS_PEC
///                     LengthInBytes=3,   Flags=I2C_FLAG_READ
///
///
/// WriteDataWord:      OperationCount=1,
///                     LengthInBytes=3,   Flags=I2C_FLAG_SMBUS_OPERATION
/// WriteDataWord+PEC:  OperationCount=1,
///                     LengthInBytes=4,   Flags=I2C_FLAG_SMBUS_OPERATION
///                                            | I2C_FLAG_SMBUS_PEC
///
///
/// ReadBlock:          OperationCount=2,
///                     LengthInBytes=1,   Flags=I2C_FLAG_SMBUS_OPERATION
///                                            | I2C_FLAG_SMBUS_BLOCK
///                     LengthInBytes=33,  Flags=I2C_FLAG_READ
/// ReadBlock+PEC:      OperationCount=2,
///                     LengthInBytes=1,   Flags=I2C_FLAG_SMBUS_OPERATION
///                                            | I2C_FLAG_SMBUS_BLOCK
///                                            | I2C_FLAG_SMBUS_PEC
///                     LengthInBytes=34,  Flags=I2C_FLAG_READ
///
///
/// WriteBlock:         OperationCount=1,
///                     LengthInBytes=N+2, Flags=I2C_FLAG_SMBUS_OPERATION
///                                            | I2C_FLAG_SMBUS_BLOCK
/// WriteBlock+PEC:     OperationCount=1,
///                     LengthInBytes=N+3, Flags=I2C_FLAG_SMBUS_OPERATION
///                                            | I2C_FLAG_SMBUS_BLOCK
///                                            | I2C_FLAG_SMBUS_PEC
///
///
/// ProcessCall:        OperationCount=2,
///                     LengthInBytes=3,   Flags=I2C_FLAG_SMBUS_OPERATION
///                                            | I2C_FLAG_SMBUS_PROCESS_CALL
///                     LengthInBytes=2,   Flags=I2C_FLAG_READ
/// ProcessCall+PEC:    OperationCount=2,
///                     LengthInBytes=3,   Flags=I2C_FLAG_SMBUS_OPERATION
///                                            | I2C_FLAG_SMBUS_PROCESS_CALL
///                                            | I2C_FLAG_SMBUS_PEC
///                     LengthInBytes=3,   Flags=I2C_FLAG_READ
///
///
/// BlkProcessCall:     OperationCount=2,
///                     LengthInBytes=N+2, Flags=I2C_FLAG_SMBUS_OPERATION
///                                            | I2C_FLAG_SMBUS_PROCESS_CALL
///                                            | I2C_FLAG_SMBUS_BLOCK
///                     LengthInBytes=33,  Flags=I2C_FLAG_READ
/// BlkProcessCall+PEC: OperationCount=2,
///                     LengthInBytes=N+2, Flags=I2C_FLAG_SMBUS_OPERATION
///                                            | I2C_FLAG_SMBUS_PROCESS_CALL
///                                            | I2C_FLAG_SMBUS_BLOCK
///                                            | I2C_FLAG_SMBUS_PEC
///                     LengthInBytes=34,  Flags=I2C_FLAG_READ
///
//----------------------------------------------------------------------

///
/// I2C device operation
///
/// The EFI_I2C_OPERATION describes a subset of an I2C transaction in which
/// the I2C controller is either sending or receiving bytes from the bus.
/// Some transactions will consist of a single operation while others will
/// be two or more.
///
/// Note: Some I2C controllers do not support read or write ping (address
/// only) operation and will return EFI_UNSUPPORTED status when these
/// operations are requested.
///
/// Note: I2C controllers which do not support complex transactions requiring
/// multiple repeated start bits return EFI_UNSUPPORTED without processing
/// any of the transaction.
///
typedef struct {
  ///
  /// Flags to qualify the I2C operation.
  ///
  UINT32 Flags;

  ///
  /// Number of bytes to send to or receive from the I2C device.  A ping
  /// (address only byte/bytes)  is indicated by setting the LengthInBytes
  /// to zero.
  ///
  UINT32 LengthInBytes;

  ///
  /// Pointer to a buffer containing the data to send or to receive from
  /// the I2C device.  The Buffer must be at least LengthInBytes in size.
  ///
  UINT8 *Buffer;
} EFI_I2C_OPERATION;

///
/// I2C device request
///
/// The EFI_I2C_REQUEST_PACKET describes a single I2C transaction.  The
/// transaction starts with a start bit followed by the first operation
/// in the operation array.  Subsequent operations are separated with
/// repeated start bits and the last operation is followed by a stop bit
/// which concludes the transaction.  Each operation is described by one
/// of the elements in the Operation array.
///
typedef struct {
  ///
  /// Number of elements in the operation array
  ///
  UINTN OperationCount;

  ///
  /// Description of the I2C operation
  ///
  EFI_I2C_OPERATION Operation [1];
} EFI_I2C_REQUEST_PACKET;

#endif  //  __PI_I2C_H__
