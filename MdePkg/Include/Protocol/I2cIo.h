/** @file
  I2C I/O Protocol as defined in the PI 1.3 specification.

  The EFI I2C I/O protocol enables the user to manipulate a single
  I2C device independent of the host controller and I2C design.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This protocol is from PI Version 1.3.

**/

#ifndef __I2C_IO_H__
#define __I2C_IO_H__

#include <Pi/PiI2c.h>

#define EFI_I2C_IO_PROTOCOL_GUID  { 0xb60a3e6b, 0x18c4, 0x46e5, { 0xa2, 0x9a, 0xc9, 0xa1, 0x06, 0x65, 0xa2, 0x8e }}

///
/// I2C I/O protocol
///
/// The I2C IO protocol enables access to a specific device on the I2C
/// bus.
///
/// Each I2C device is identified uniquely in the system by the tuple
/// DeviceGuid:DeviceIndex.  The DeviceGuid represents the manufacture
/// and part number and is provided by the silicon vendor or the third
/// party I2C device driver writer.  The DeviceIndex identifies the part
/// within the system by using a unique number and is created by the
/// board designer or the writer of the EFI_I2C_ENUMERATE_PROTOCOL.
///
/// I2C slave addressing is abstracted to validate addresses and limit
/// operation to the specified I2C device.  The third party providing
/// the I2C device support provides an ordered list of slave addresses
/// for the I2C device required to implement the EFI_I2C_ENUMERATE_PROTOCOL.
/// The order of the list must be preserved.
///
typedef struct _EFI_I2C_IO_PROTOCOL EFI_I2C_IO_PROTOCOL;

/**
  Queue an I2C transaction for execution on the I2C device.

  This routine must be called at or below TPL_NOTIFY.  For synchronous
  requests this routine must be called at or below TPL_CALLBACK.

  This routine queues an I2C transaction to the I2C controller for
  execution on the I2C bus.

  When Event is NULL, QueueRequest() operates synchronously and returns
  the I2C completion status as its return value.

  When Event is not NULL, QueueRequest() synchronously returns EFI_SUCCESS
  indicating that the asynchronous I2C transaction was queued.  The values
  above are returned in the buffer pointed to by I2cStatus upon the
  completion of the I2C transaction when I2cStatus is not NULL.

  The upper layer driver writer provides the following to the platform
  vendor:

  1.  Vendor specific GUID for the I2C part
  2.  Guidance on proper construction of the slave address array when the
      I2C device uses more than one slave address.  The I2C bus protocol
      uses the SlaveAddressIndex to perform relative to physical address
      translation to access the blocks of hardware within the I2C device.

  @param[in] This               Pointer to an EFI_I2C_IO_PROTOCOL structure.
  @param[in] SlaveAddressIndex  Index value into an array of slave addresses
                                for the I2C device.  The values in the array
                                are specified by the board designer, with the
                                third party I2C device driver writer providing
                                the slave address order.

                                For devices that have a single slave address,
                                this value must be zero.  If the I2C device
                                uses more than one slave address then the
                                third party (upper level) I2C driver writer
                                needs to specify the order of entries in the
                                slave address array.

                                \ref ThirdPartyI2cDrivers "Third Party I2C
                                Drivers" section in I2cMaster.h.
  @param[in] Event              Event to signal for asynchronous transactions,
                                NULL for synchronous transactions
  @param[in] RequestPacket      Pointer to an EFI_I2C_REQUEST_PACKET structure
                                describing the I2C transaction
  @param[out] I2cStatus         Optional buffer to receive the I2C transaction
                                completion status

  @retval EFI_SUCCESS           The asynchronous transaction was successfully
                                queued when Event is not NULL.
  @retval EFI_SUCCESS           The transaction completed successfully when
                                Event is NULL.
  @retval EFI_BAD_BUFFER_SIZE   The RequestPacket->LengthInBytes value is too
                                large.
  @retval EFI_DEVICE_ERROR      There was an I2C error (NACK) during the
                                transaction.
  @retval EFI_INVALID_PARAMETER RequestPacket is NULL.
  @retval EFI_NO_MAPPING        The EFI_I2C_HOST_PROTOCOL could not set the
                                bus configuration required to access this I2C
                                device.
  @retval EFI_NO_RESPONSE       The I2C device is not responding to the slave
                                address selected by SlaveAddressIndex.
                                EFI_DEVICE_ERROR will be returned if the
                                controller cannot distinguish when the NACK
                                occurred.
  @retval EFI_OUT_OF_RESOURCES  Insufficient memory for I2C transaction
  @retval EFI_UNSUPPORTED       The controller does not support the requested
                                transaction.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_I2C_IO_PROTOCOL_QUEUE_REQUEST)(
  IN CONST EFI_I2C_IO_PROTOCOL  *This,
  IN UINTN                      SlaveAddressIndex,
  IN EFI_EVENT                  Event      OPTIONAL,
  IN EFI_I2C_REQUEST_PACKET     *RequestPacket,
  OUT EFI_STATUS                *I2cStatus OPTIONAL
  );

///
/// I2C I/O protocol
///
struct _EFI_I2C_IO_PROTOCOL {
  ///
  /// Queue an I2C transaction for execution on the I2C device.
  ///
  EFI_I2C_IO_PROTOCOL_QUEUE_REQUEST        QueueRequest;

  ///
  /// Unique value assigned by the silicon manufacture or the third
  /// party I2C driver writer for the I2C part.  This value logically
  /// combines both the manufacture name and the I2C part number into
  /// a single value specified as a GUID.
  ///
  CONST EFI_GUID                           *DeviceGuid;

  ///
  /// Unique ID of the I2C part within the system
  ///
  UINT32                                   DeviceIndex;

  ///
  /// Hardware revision - ACPI _HRV value.  See the Advanced Configuration
  /// and Power Interface Specification, Revision 5.0  for the field format
  /// and the Plug and play support for I2C web-page for restriction on values.
  ///
  UINT32                                   HardwareRevision;

  ///
  /// Pointer to an EFI_I2C_CONTROLLER_CAPABILITIES data structure containing
  /// the capabilities of the I2C host controller.
  ///
  CONST EFI_I2C_CONTROLLER_CAPABILITIES    *I2cControllerCapabilities;
};

///
/// Reference to variable defined in the .DEC file
///
extern EFI_GUID  gEfiI2cIoProtocolGuid;

#endif //  __I2C_IO_H__
