/** @file
  The Super I/O Protocol is installed by the Super I/O driver. The Super I/O driver is a UEFI driver
  model compliant driver. In the Start() routine of the Super I/O driver, a handle with an instance
  of EFI_SIO_PROTOCOL is created for each device within the Super I/O. The device within the
  Super I/O is powered up, enabled, and assigned with the default set of resources. In the Stop()
  routine of the Super I/O driver, the device is disabled and Super I/O protocol is uninstalled.

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef __EFI_SUPER_IO_PROTOCOL_H__
#define __EFI_SUPER_IO_PROTOCOL_H__
#include <IndustryStandard/Acpi.h>

#define EFI_SIO_PROTOCOL_GUID \
  { 0x215fdd18, 0xbd50, 0x4feb, { 0x89, 0xb, 0x58, 0xca, 0xb, 0x47, 0x39, 0xe9 } }

typedef union {
  ACPI_SMALL_RESOURCE_HEADER *SmallHeader;
  ACPI_LARGE_RESOURCE_HEADER *LargeHeader;
} ACPI_RESOURCE_HEADER_PTR;

typedef struct {
  UINT8 Register;       ///< Register number.
  UINT8 AndMask;        ///< Bitwise AND mask.
  UINT8 OrMask;         ///< Bitwise OR mask.
} EFI_SIO_REGISTER_MODIFY;
  
typedef struct _EFI_SIO_PROTOCOL  EFI_SIO_PROTOCOL;
  
/**
  Provides a low level access to the registers for the Super I/O.
  
  @param[in]        This        Indicates a pointer to the calling context.
  @param[in]        Write       Specifies the type of the register operation. If this parameter is TRUE, Value is
                                interpreted as an input parameter and the operation is a register write. If this parameter
                                is FALSE, Value is interpreted as an output parameter and the operation is a register
                                read.
  @param[in]        ExitCfgMode Exit Configuration Mode Indicator. If this parameter is set to TRUE, the Super I/O
                                driver will turn off configuration mode of the Super I/O prior to returning from this
                                function. If this parameter is set to FALSE, the Super I/O driver will leave Super I/O
                                in the configuration mode.
                                The Super I/O driver must track the current state of the Super I/O and enable the
                                configuration mode of Super I/O if necessary prior to register access.
  @param[in]        Register    Register number.
  @param[in, out]   Value       If Write is TRUE, Value is a pointer to the buffer containing the byte of data to be
                                written to the Super I/O register. If Write is FALSE, Value is a pointer to the
                                destination buffer for the byte of data to be read from the Super I/O register. 

  @retval EFI_SUCCESS           The operation completed successfully
  @retval EFI_INVALID_PARAMETER The Value is NULL
  @retval EFI_INVALID_PARAMETER Invalid Register number
  
**/  
typedef
EFI_STATUS
(EFIAPI *EFI_SIO_REGISTER_ACCESS)(
  IN   CONST  EFI_SIO_PROTOCOL  *This,
  IN          BOOLEAN           Write,
  IN          BOOLEAN           ExitCfgMode,
  IN          UINT8             Register,
  IN OUT      UINT8             *Value
);
  
/**
  Provides an interface to get a list of the current resources consumed by the device in the ACPI
  Resource Descriptor format.
  
  GetResources() returns a list of resources currently consumed by the device. The
  ResourceList is a pointer to the buffer containing resource descriptors for the device. The
  descriptors are in the format of Small or Large ACPI resource descriptor as defined by ACPI
  specification (2.0 & 3.0). The buffer of resource descriptors is terminated with the 'End tag'
  resource descriptor.
  
  @param[in]    This            Indicates a pointer to the calling context.
  @param[out]   ResourceList    A pointer to an ACPI resource descriptor list that defines the current resources used by
                                the device. Type ACPI_RESOURCE_HEADER_PTR is defined in the "Related
                                Definitions" below.
                                
  @retval EFI_SUCCESS           The operation completed successfully
  @retval EFI_INVALID_PARAMETER ResourceList is NULL
                                
**/  
typedef
EFI_STATUS
(EFIAPI *EFI_SIO_GET_RESOURCES)(
  IN  CONST EFI_SIO_PROTOCOL            *This,
  OUT       ACPI_RESOURCE_HEADER_PTR    *ResourceList
);
  
/**
  Sets the resources for the device.
  
  @param[in]  This          Indicates a pointer to the calling context.
  @param[in]  ResourceList  Pointer to the ACPI resource descriptor list. Type ACPI_RESOURCE_HEADER_PTR
                            is defined in the "Related Definitions" section of
                            EFI_SIO_PROTOCOL.GetResources().

  @retval EFI_SUCCESS           The operation completed successfully
  @retval EFI_INVALID_PARAMETER ResourceList is invalid
  @retval EFI_ACCESS_DENIED     Some of the resources in ResourceList are in use
                            
**/  
typedef
EFI_STATUS
(EFIAPI *EFI_SIO_SET_RESOURCES)(
  IN CONST  EFI_SIO_PROTOCOL        *This,
  IN        ACPI_RESOURCE_HEADER_PTR ResourceList
);
  
/**
  Provides a collection of resource descriptor lists. Each resource descriptor list in the collection
  defines a combination of resources that can potentially be used by the device.
  
  @param[in]  This                  Indicates a pointer to the calling context.
  @param[out] ResourceCollection    Collection of the resource descriptor lists.
  
  @retval EFI_SUCCESS               The operation completed successfully
  @retval EFI_INVALID_PARAMETER     ResourceCollection is NULL
**/  
typedef
EFI_STATUS
(EFIAPI *EFI_SIO_POSSIBLE_RESOURCES)(
  IN  CONST EFI_SIO_PROTOCOL         *This,
  OUT       ACPI_RESOURCE_HEADER_PTR *ResourceCollection
);
  
/**
  Provides an interface for a table based programming of the Super I/O registers.
  
  The Modify() function provides an interface for table based programming of the Super I/O
  registers. This function can be used to perform programming of multiple Super I/O registers with a
  single function call. For each table entry, the Register is read, its content is bitwise ANDed with
  AndMask, and then ORed with OrMask before being written back to the Register. The Super
  I/O driver must track the current state of the Super I/O and enable the configuration mode of Super I/
  O if necessary prior to table processing. Once the table is processed, the Super I/O device has to be
  returned to the original state.

  @param[in] This       Indicates a pointer to the calling context.
  @param[in] Command    A pointer to an array of NumberOfCommands EFI_SIO_REGISTER_MODIFY
                        structures. Each structure specifies a single Super I/O register modify operation. Type
                        EFI_SIO_REGISTER_MODIFY is defined in the "Related Definitions" below.
  @param[in] NumberOfCommands Number of elements in the Command array.  
  
  @retval EFI_SUCCESS           The operation completed successfully
  @retval EFI_INVALID_PARAMETER Command is NULL
  
**/  
typedef
EFI_STATUS
(EFIAPI *EFI_SIO_MODIFY)(
  IN CONST EFI_SIO_PROTOCOL         *This,
  IN CONST EFI_SIO_REGISTER_MODIFY  *Command,
  IN       UINTN                    NumberOfCommands
);
  
struct _EFI_SIO_PROTOCOL {
  EFI_SIO_REGISTER_ACCESS       RegisterAccess;
  EFI_SIO_GET_RESOURCES         GetResources;
  EFI_SIO_SET_RESOURCES         SetResources;
  EFI_SIO_POSSIBLE_RESOURCES    PossibleResources;
  EFI_SIO_MODIFY Modify;
};  

extern EFI_GUID gEfiSioProtocolGuid;

#endif // __EFI_SUPER_IO_PROTOCOL_H__
