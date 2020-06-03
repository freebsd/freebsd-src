/** @file
  This file declares Smbus2 PPI.
  This PPI provides the basic I/O interfaces that a PEIM uses to access its
  SMBus controller and the slave devices attached to it.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.0.

**/

#ifndef __PEI_SMBUS2_PPI_H__
#define __PEI_SMBUS2_PPI_H__

#include <IndustryStandard/SmBus.h>

#define EFI_PEI_SMBUS2_PPI_GUID \
  { 0x9ca93627, 0xb65b, 0x4324, { 0xa2, 0x2, 0xc0, 0xb4, 0x61, 0x76, 0x45, 0x43 } }


typedef struct _EFI_PEI_SMBUS2_PPI EFI_PEI_SMBUS2_PPI;

/**
  Executes an SMBus operation to an SMBus controller. Returns when either
  the command has been executed or an error is encountered in doing the operation.

  @param  This            A pointer to the EFI_PEI_SMBUS2_PPI instance.
  @param  SlaveAddress    The SMBUS hardware address to which the SMBUS device is preassigned or
                          allocated.
  @param  Command         This command is transmitted by the SMBus host controller to the SMBus slave
                          device and the interpretation is SMBus slave device specific.
                          It can mean the offset to a list of functions inside
                          an SMBus slave device. Not all operations or slave devices support
                          this command's registers.
  @param  Operation       Signifies which particular SMBus hardware protocol instance that it
                          will use to execute the SMBus transactions.
                          This SMBus hardware protocol is defined by the System Management Bus (SMBus)
                          Specification and is not related to UEFI.
  @param  PecCheck        Defines if Packet Error Code (PEC) checking is required for this operation.
  @param  Length          Signifies the number of bytes that this operation will do.
                          The maximum number of bytes can be revision specific and operation specific.
                          This parameter will contain the actual number of bytes that are executed
                          for this operation. Not all operations require this argument.
  @param  Buffer          Contains the value of data to execute to the SMBus slave device.
                          Not all operations require this argument.
                          The length of this buffer is identified by Length.


  @retval EFI_SUCCESS           The last data that was returned from the access
                                matched the poll exit criteria.
  @retval EFI_CRC_ERROR         The checksum is not correct (PEC is incorrect)
  @retval EFI_TIMEOUT           Timeout expired before the operation was completed.
                                Timeout is determined by the SMBus host controller device.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed
                                due to a lack of resources.
  @retval EFI_DEVICE_ERROR      The request was not completed because
                                a failure reflected in the Host Status Register bit.
  @retval EFI_INVALID_PARAMETER Operation is not defined in EFI_SMBUS_OPERATION.
                                Or Length/Buffer is NULL for operations except for EfiSmbusQuickRead and
                                EfiSmbusQuickWrite. Or Length is outside the range of valid values.
  @retval EFI_UNSUPPORTED       The SMBus operation or PEC is not supported.
  @retval EFI_BUFFER_TOO_SMALL  Buffer is not sufficient for this operation.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_SMBUS2_PPI_EXECUTE_OPERATION)(
  IN CONST  EFI_PEI_SMBUS2_PPI        *This,
  IN        EFI_SMBUS_DEVICE_ADDRESS  SlaveAddress,
  IN        EFI_SMBUS_DEVICE_COMMAND  Command,
  IN        EFI_SMBUS_OPERATION       Operation,
  IN        BOOLEAN                   PecCheck,
  IN OUT    UINTN                     *Length,
  IN OUT    VOID                      *Buffer
);

/**
  The ArpDevice() function enumerates the entire bus or enumerates a specific
  device that is identified by SmbusUdid.

  @param  This           A pointer to the EFI_PEI_SMBUS2_PPI instance.
  @param  ArpAll         A Boolean expression that indicates if the host drivers need
                         to enumerate all the devices or enumerate only the device that is identified
                         by SmbusUdid. If ArpAll is TRUE, SmbusUdid and SlaveAddress are optional.
                         If ArpAll is FALSE, ArpDevice will enumerate SmbusUdid and the address
                         will be at SlaveAddress.
  @param  SmbusUdid      The targeted SMBus Unique Device Identifier (UDID).
                         The UDID may not exist for SMBus devices with fixed addresses.
  @param  SlaveAddress   The new SMBus address for the slave device for
                         which the operation is targeted.

  @retval EFI_SUCCESS           The SMBus slave device address was set.
  @retval EFI_INVALID_PARAMETER SlaveAddress is NULL.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed
                                due to a lack of resources.
  @retval EFI_TIMEOUT           The SMBus slave device did not respond.
  @retval EFI_DEVICE_ERROR      The request was not completed because the transaction failed.
  @retval EFI_UNSUPPORTED       ArpDevice, GetArpMap, and Notify are not implemented by this PEIM.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_SMBUS2_PPI_ARP_DEVICE)(
  IN CONST  EFI_PEI_SMBUS2_PPI        *This,
  IN        BOOLEAN                   ArpAll,
  IN        EFI_SMBUS_UDID            *SmbusUdid,   OPTIONAL
  IN OUT    EFI_SMBUS_DEVICE_ADDRESS  *SlaveAddress OPTIONAL
);

/**
  The GetArpMap() function returns the mapping of all the SMBus devices
  that are enumerated by the SMBus host driver.

  @param  This           A pointer to the EFI_PEI_SMBUS2_PPI instance.
  @param  Length         Size of the buffer that contains the SMBus device map.
  @param  SmbusDeviceMap The pointer to the device map as enumerated
                         by the SMBus controller driver.

  @retval EFI_SUCCESS           The device map was returned correctly in the buffer.
  @retval EFI_UNSUPPORTED       ArpDevice, GetArpMap, and Notify are not implemented by this PEIM.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_SMBUS2_PPI_GET_ARP_MAP)(
  IN CONST  EFI_PEI_SMBUS2_PPI    *This,
  IN OUT    UINTN                 *Length,
  IN OUT    EFI_SMBUS_DEVICE_MAP  **SmbusDeviceMap
);

/**
  CallBack function can be registered in EFI_PEI_SMBUS2_PPI_NOTIFY.

  @param  This           A pointer to the EFI_PEI_SMBUS2_PPI instance.
  @param  SlaveAddress   The SMBUS hardware address to which the SMBUS
                         device is preassigned or allocated.
  @param  Data           Data of the SMBus host notify command that
                         the caller wants to be called.

  @retval EFI_SUCCESS           NotifyFunction has been registered.
  @retval EFI_UNSUPPORTED       ArpDevice, GetArpMap, and Notify are not
                                implemented by this PEIM.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_SMBUS_NOTIFY2_FUNCTION)(
  IN CONST  EFI_PEI_SMBUS2_PPI        *SmbusPpi,
  IN        EFI_SMBUS_DEVICE_ADDRESS  SlaveAddress,
  IN        UINTN                     Data
);

/**
  The Notify() function registers all the callback functions to allow the
  bus driver to call these functions when the SlaveAddress/Data pair happens.

  @param  This           A pointer to the EFI_PEI_SMBUS2_PPI instance.
  @param  SlaveAddress   Address that the host controller detects as
                         sending a message and calls all the registered functions.
  @param  Data           Data that the host controller detects as sending a message
                         and calls all the registered functions.
  @param  NotifyFunction The function to call when the bus driver
                         detects the SlaveAddress and Data pair.

  @retval EFI_SUCCESS     NotifyFunction has been registered.
  @retval EFI_UNSUPPORTED ArpDevice, GetArpMap, and Notify are not
                          implemented by this PEIM.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_SMBUS2_PPI_NOTIFY)(
  IN CONST EFI_PEI_SMBUS2_PPI              *This,
  IN       EFI_SMBUS_DEVICE_ADDRESS        SlaveAddress,
  IN       UINTN                           Data,
  IN       EFI_PEI_SMBUS_NOTIFY2_FUNCTION  NotifyFunction
);

///
///  Provides the basic I/O interfaces that a PEIM uses to access
///  its SMBus controller and the slave devices attached to it.
///
struct _EFI_PEI_SMBUS2_PPI {
  EFI_PEI_SMBUS2_PPI_EXECUTE_OPERATION  Execute;
  EFI_PEI_SMBUS2_PPI_ARP_DEVICE         ArpDevice;
  EFI_PEI_SMBUS2_PPI_GET_ARP_MAP        GetArpMap;
  EFI_PEI_SMBUS2_PPI_NOTIFY             Notify;
  ///
  /// Identifier which uniquely identifies this SMBus controller in a system.
  ///
  EFI_GUID                              Identifier;
};

extern EFI_GUID gEfiPeiSmbus2PpiGuid;

#endif
