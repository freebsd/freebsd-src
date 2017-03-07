/** @file
  The file provides basic SMBus host controller management 
  and basic data transactions over the SMBus.

  Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

  @par Revision Reference: PI
  Version 1.00.

**/

#ifndef __SMBUS_HC_H__
#define __SMBUS_HC_H__

#include <IndustryStandard/SmBus.h>

#define EFI_SMBUS_HC_PROTOCOL_GUID \
  {0xe49d33ed, 0x513d, 0x4634, { 0xb6, 0x98, 0x6f, 0x55, 0xaa, 0x75, 0x1c, 0x1b} }

typedef struct _EFI_SMBUS_HC_PROTOCOL EFI_SMBUS_HC_PROTOCOL;

/**
     
  The Execute() function provides a standard way to execute an
  operation as defined in the System Management Bus (SMBus)
  Specification. The resulting transaction will be either that
  the SMBus slave devices accept this transaction or that this
  function returns with error. 
  
  @param This     A pointer to the EFI_SMBUS_HC_PROTOCOL instance.
                  SlaveAddress The SMBus slave address of the device
                  with which to communicate. Type
                  EFI_SMBUS_DEVICE_ADDRESS is defined in
                  EFI_PEI_SMBUS_PPI.Execute() in the Platform
                  Initialization SMBus PPI Specification.

  @param Command  This command is transmitted by the SMBus host
                  controller to the SMBus slave device and the
                  interpretation is SMBus slave device specific.
                  It can mean the offset to a list of functions
                  inside an SMBus slave device. Not all
                  operations or slave devices support this
                  command's registers. Type
                  EFI_SMBUS_DEVICE_COMMAND is defined in
                  EFI_PEI_SMBUS_PPI.Execute() in the Platform
                  Initialization SMBus PPI Specification.

 @param Operation Signifies the particular SMBus
                  hardware protocol instance it will use to
                  execute the SMBus transactions. This SMBus
                  hardware protocol is defined by the SMBus
                  Specification and is not related to PI
                  Architecture. Type EFI_SMBUS_OPERATION is
                  defined in EFI_PEI_SMBUS_PPI.Execute() in the
                  Platform Initialization SMBus PPI
                  Specification.

  @param PecCheck Defines if Packet Error Code (PEC) checking
                  is required for this operation. SMBus Host
                  Controller Code Definitions Version 1.0
                  August 21, 2006 13 
                  
 @param Length    Signifies the number of bytes that this operation will do.
                  The maximum number of bytes can be revision
                  specific and operation specific. This field
                  will contain the actual number of bytes that
                  are executed for this operation. Not all
                  operations require this argument.

  @param Buffer   Contains the value of data to execute to the
                  SMBus slave device. Not all operations require
                  this argument. The length of this buffer is
                  identified by Length.
  
  
  @retval EFI_SUCCESS           The last data that was returned from the
                                access matched the poll exit criteria.

  @retval EFI_CRC_ERROR         Checksum is not correct (PEC is incorrect).

  @retval EFI_TIMEOUT           Timeout expired before the operation was
                                completed. Timeout is determined by the
                                SMBus host controller device.

  @retval EFI_OUT_OF_RESOURCES  The request could not be
                                completed due to a lack of
                                resources.

  @retval EFI_DEVICE_ERROR      The request was not completed
                                because a failure that was reflected
                                in the Host Status Register bit.
                                Device errors are a result of a
                                transaction collision, illegal
                                command field, unclaimed cycle (host
                                initiated), or bus errors
                                (collisions).

  @retval EFI_INVALID_PARAMETER Operation is not defined in
                                EFI_SMBUS_OPERATION.

  @retval EFI_INVALID_PARAMETER Length/Buffer is NULL for
                                operations except for
                                EfiSmbusQuickRead and
                                EfiSmbusQuickWrite. Length is
                                outside the range of valid
                                values.

  @retval EFI_UNSUPPORTED       The SMBus operation or PEC is not
                                supported. 

  @retval EFI_BUFFER_TOO_SMALL  Buffer is not sufficient for
                                this operation.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMBUS_HC_EXECUTE_OPERATION)(
  IN CONST  EFI_SMBUS_HC_PROTOCOL     *This,
  IN        EFI_SMBUS_DEVICE_ADDRESS  SlaveAddress,
  IN        EFI_SMBUS_DEVICE_COMMAND  Command,
  IN        EFI_SMBUS_OPERATION       Operation,
  IN        BOOLEAN                   PecCheck,
  IN OUT    UINTN                     *Length,
  IN OUT    VOID                      *Buffer
);



/**
   
  The ArpDevice() function provides a standard way for a device driver to 
  enumerate the entire SMBus or specific devices on the bus.
  
  @param This           A pointer to the EFI_SMBUS_HC_PROTOCOL instance.

  @param ArpAll         A Boolean expression that indicates if the
                        host drivers need to enumerate all the devices
                        or enumerate only the device that is
                        identified by SmbusUdid. If ArpAll is TRUE,
                        SmbusUdid and SlaveAddress are optional. If
                        ArpAll is FALSE, ArpDevice will enumerate
                        SmbusUdid and the address will be at
                        SlaveAddress.

  @param SmbusUdid      The Unique Device Identifier (UDID) that is
                        associated with this device. Type
                        EFI_SMBUS_UDID is defined in
                        EFI_PEI_SMBUS_PPI.ArpDevice() in the
                        Platform Initialization SMBus PPI
                        Specification.

  @param SlaveAddress   The SMBus slave address that is
                        associated with an SMBus UDID.

  @retval EFI_SUCCESS           The last data that was returned from the
                                access matched the poll exit criteria.

  @retval EFI_CRC_ERROR         Checksum is not correct (PEC is
                                incorrect).

  @retval EFI_TIMEOUT           Timeout expired before the operation was
                                completed. Timeout is determined by the
                                SMBus host controller device.

  @retval EFI_OUT_OF_RESOURCES  The request could not be
                                completed due to a lack of
                                resources.

  @retval EFI_DEVICE_ERROR      The request was not completed
                                because a failure was reflected in
                                the Host Status Register bit. Device
                                Errors are a result of a transaction
                                collision, illegal command field,
                                unclaimed cycle (host initiated), or
                                bus errors (collisions).

  @retval EFI_UNSUPPORTED       ArpDevice, GetArpMap, and Notify are
                                not implemented by this driver.
   
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMBUS_HC_PROTOCOL_ARP_DEVICE)(
  IN CONST  EFI_SMBUS_HC_PROTOCOL     *This,
  IN        BOOLEAN                   ArpAll,
  IN        EFI_SMBUS_UDID            *SmbusUdid,   OPTIONAL
  IN OUT    EFI_SMBUS_DEVICE_ADDRESS  *SlaveAddress OPTIONAL
);


/**
  The GetArpMap() function returns the mapping of all the SMBus devices 
  that were enumerated by the SMBus host driver.
  
  @param This           A pointer to the EFI_SMBUS_HC_PROTOCOL instance.
  
  @param Length         Size of the buffer that contains the SMBus
                        device map.
  
  @param SmbusDeviceMap The pointer to the device map as
                        enumerated by the SMBus controller
                        driver.
  
  @retval EFI_SUCCESS       The SMBus returned the current device map.
  
  @retval EFI_UNSUPPORTED   ArpDevice, GetArpMap, and Notify are
                            not implemented by this driver.
  
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMBUS_HC_PROTOCOL_GET_ARP_MAP)(
  IN CONST  EFI_SMBUS_HC_PROTOCOL   *This,
  IN OUT    UINTN                   *Length,
  IN OUT    EFI_SMBUS_DEVICE_MAP    **SmbusDeviceMap
);

/**
  The notify function does some actions.
  
  @param SlaveAddress
  The SMBUS hardware address to which the SMBUS device is preassigned or allocated.

  @param Data
  Data of the SMBus host notify command that the caller wants to be called.
  
  @return EFI_STATUS
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMBUS_NOTIFY_FUNCTION)(
  IN        EFI_SMBUS_DEVICE_ADDRESS  SlaveAddress,
  IN        UINTN                     Data
);


/**
   
  The Notify() function registers all the callback functions to
  allow the bus driver to call these functions when the 
  SlaveAddress/Data pair happens.
  
  @param  This            A pointer to the EFI_SMBUS_HC_PROTOCOL instance.
  
  @param  SlaveAddress    Address that the host controller detects
                          as sending a message and calls all the registered function.

  @param  Data            Data that the host controller detects as sending
                          message and calls all the registered function.


  @param  NotifyFunction  The function to call when the bus
                          driver detects the SlaveAddress and
                          Data pair.

  @retval EFI_SUCCESS       NotifyFunction was registered.
  
  @retval EFI_UNSUPPORTED   ArpDevice, GetArpMap, and Notify are
                            not implemented by this driver.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMBUS_HC_PROTOCOL_NOTIFY)(
  IN CONST  EFI_SMBUS_HC_PROTOCOL     *This,
  IN        EFI_SMBUS_DEVICE_ADDRESS  SlaveAddress,
  IN        UINTN                     Data,
  IN        EFI_SMBUS_NOTIFY_FUNCTION NotifyFunction
);


///
/// The EFI_SMBUS_HC_PROTOCOL provides SMBus host controller management and basic data
/// transactions over SMBus. There is one EFI_SMBUS_HC_PROTOCOL instance for each SMBus
/// host controller.
///
struct _EFI_SMBUS_HC_PROTOCOL {
  EFI_SMBUS_HC_EXECUTE_OPERATION    Execute;
  EFI_SMBUS_HC_PROTOCOL_ARP_DEVICE  ArpDevice;
  EFI_SMBUS_HC_PROTOCOL_GET_ARP_MAP GetArpMap;
  EFI_SMBUS_HC_PROTOCOL_NOTIFY      Notify;
};


extern EFI_GUID gEfiSmbusHcProtocolGuid;

#endif

