/** @file
  Bus Specific Driver Override protocol as defined in the UEFI 2.0 specification.

  Bus drivers that have a bus specific algorithm for matching drivers to controllers are
  required to produce this protocol for each controller. For example, a PCI Bus Driver will produce an
  instance of this protocol for every PCI controller that has a PCI option ROM that contains one or
  more UEFI drivers. The protocol instance is attached to the handle of the PCI controller.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef _EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL_H_
#define _EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL_H_

///
/// Global ID for the Bus Specific Driver Override Protocol
///
#define EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL_GUID \
  { \
    0x3bc1b285, 0x8a15, 0x4a82, {0xaa, 0xbf, 0x4d, 0x7d, 0x13, 0xfb, 0x32, 0x65 } \
  }

typedef struct _EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL  EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL;

//
// Prototypes for the Bus Specific Driver Override Protocol
//

/**                                                                   
  Uses a bus specific algorithm to retrieve a driver image handle for a controller.
    
  @param  This                  A pointer to the EFI_BUS_SPECIFIC_DRIVER_
                                OVERRIDE_PROTOCOL instance.              
  @param  DriverImageHandle     On input, a pointer to the previous driver image handle returned
                                by GetDriver(). On output, a pointer to the next driver         
                                image handle. Passing in a NULL, will return the first driver   
                                image handle.

  @retval EFI_SUCCESS           A bus specific override driver is returned in DriverImageHandle.
  @retval EFI_NOT_FOUND         The end of the list of override drivers was reached.
                                A bus specific override driver is not returned in DriverImageHandle.
  @retval EFI_INVALID_PARAMETER DriverImageHandle is not a handle that was returned on a
                                previous call to GetDriver().

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_GET_DRIVER)(
  IN EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL              *This,
  IN OUT EFI_HANDLE                                         *DriverImageHandle
  );

///
/// This protocol matches one or more drivers to a controller. This protocol is produced by a bus driver,
/// and it is installed on the child handles of buses that require a bus specific algorithm for matching
/// drivers to controllers.
///
struct _EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL {
  EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_GET_DRIVER GetDriver;
};

extern EFI_GUID gEfiBusSpecificDriverOverrideProtocolGuid;

#endif
