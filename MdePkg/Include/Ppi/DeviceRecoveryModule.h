/** @file
  This file declares the Device Recovery Module PPI.

  The interface of this PPI does the following:
    - Reports the number of recovery DXE capsules that exist on the associated device(s)
    - Finds the requested firmware binary capsule
    - Loads that capsule into memory

  A device can be either a group of devices, such as a block device, or an individual device.
  The module determines the internal search order, with capsule number 1 as the highest load
  priority and number N as the lowest priority.

  Copyright (c) 2007 - 2011, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  @par Revision Reference:
  This PPI is defined in UEFI Platform Initialization Specification 1.2 Volume 1: 
  Pre-EFI Initalization Core Interface

**/

#ifndef _PEI_DEVICE_RECOVERY_MODULE_PPI_H_
#define _PEI_DEVICE_RECOVERY_MODULE_PPI_H_

#define EFI_PEI_DEVICE_RECOVERY_MODULE_PPI_GUID \
  { \
    0x0DE2CE25, 0x446A, 0x45a7, {0xBF, 0xC9, 0x37, 0xDA, 0x26, 0x34, 0x4B, 0x37 } \
  }

typedef struct _EFI_PEI_DEVICE_RECOVERY_MODULE_PPI EFI_PEI_DEVICE_RECOVERY_MODULE_PPI;

/**
  Returns the number of DXE capsules residing on the device.

  This function searches for DXE capsules from the associated device and returns
  the number and maximum size in bytes of the capsules discovered. Entry 1 is 
  assumed to be the highest load priority and entry N is assumed to be the lowest 
  priority.

  @param[in]  PeiServices              General-purpose services that are available 
                                       to every PEIM
  @param[in]  This                     Indicates the EFI_PEI_DEVICE_RECOVERY_MODULE_PPI
                                       instance.
  @param[out] NumberRecoveryCapsules   Pointer to a caller-allocated UINTN. On 
                                       output, *NumberRecoveryCapsules contains 
                                       the number of recovery capsule images 
                                       available for retrieval from this PEIM 
                                       instance.

  @retval EFI_SUCCESS        One or more capsules were discovered.
  @retval EFI_DEVICE_ERROR   A device error occurred.
  @retval EFI_NOT_FOUND      A recovery DXE capsule cannot be found.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_DEVICE_GET_NUMBER_RECOVERY_CAPSULE)(
  IN  EFI_PEI_SERVICES                    **PeiServices,
  IN  EFI_PEI_DEVICE_RECOVERY_MODULE_PPI  *This,
  OUT UINTN                               *NumberRecoveryCapsules
  );

/**
  Returns the size and type of the requested recovery capsule.

  This function gets the size and type of the capsule specified by CapsuleInstance.

  @param[in]  PeiServices       General-purpose services that are available to every PEIM
  @param[in]  This              Indicates the EFI_PEI_DEVICE_RECOVERY_MODULE_PPI 
                                instance.
  @param[in]  CapsuleInstance   Specifies for which capsule instance to retrieve 
                                the information.  This parameter must be between 
                                one and the value returned by GetNumberRecoveryCapsules() 
                                in NumberRecoveryCapsules.
  @param[out] Size              A pointer to a caller-allocated UINTN in which 
                                the size of the requested recovery module is 
                                returned.
  @param[out] CapsuleType       A pointer to a caller-allocated EFI_GUID in which 
                                the type of the requested recovery capsule is 
                                returned.  The semantic meaning of the value 
                                returned is defined by the implementation.

  @retval EFI_SUCCESS        One or more capsules were discovered.
  @retval EFI_DEVICE_ERROR   A device error occurred.
  @retval EFI_NOT_FOUND      A recovery DXE capsule cannot be found.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_DEVICE_GET_RECOVERY_CAPSULE_INFO)(
  IN  EFI_PEI_SERVICES                    **PeiServices,
  IN  EFI_PEI_DEVICE_RECOVERY_MODULE_PPI  *This,
  IN  UINTN                               CapsuleInstance,
  OUT UINTN                               *Size,
  OUT EFI_GUID                            *CapsuleType
  );

/**
  Loads a DXE capsule from some media into memory.

  This function, by whatever mechanism, retrieves a DXE capsule from some device
  and loads it into memory. Note that the published interface is device neutral.

  @param[in]     PeiServices       General-purpose services that are available 
                                   to every PEIM
  @param[in]     This              Indicates the EFI_PEI_DEVICE_RECOVERY_MODULE_PPI
                                   instance.
  @param[in]     CapsuleInstance   Specifies which capsule instance to retrieve.
  @param[out]    Buffer            Specifies a caller-allocated buffer in which 
                                   the requested recovery capsule will be returned.

  @retval EFI_SUCCESS        The capsule was loaded correctly.
  @retval EFI_DEVICE_ERROR   A device error occurred.
  @retval EFI_NOT_FOUND      A requested recovery DXE capsule cannot be found.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_DEVICE_LOAD_RECOVERY_CAPSULE)(
  IN     EFI_PEI_SERVICES                    **PeiServices,
  IN     EFI_PEI_DEVICE_RECOVERY_MODULE_PPI  *This,
  IN     UINTN                               CapsuleInstance,
  OUT    VOID                                *Buffer
  );

///
/// Presents a standard interface to EFI_PEI_DEVICE_RECOVERY_MODULE_PPI,
/// regardless of the underlying device(s).
///
struct _EFI_PEI_DEVICE_RECOVERY_MODULE_PPI {
  EFI_PEI_DEVICE_GET_NUMBER_RECOVERY_CAPSULE  GetNumberRecoveryCapsules;    ///< Returns the number of DXE capsules residing on the device.
  EFI_PEI_DEVICE_GET_RECOVERY_CAPSULE_INFO    GetRecoveryCapsuleInfo;       ///< Returns the size and type of the requested recovery capsule.
  EFI_PEI_DEVICE_LOAD_RECOVERY_CAPSULE        LoadRecoveryCapsule;          ///< Loads a DXE capsule from some media into memory.
};

extern EFI_GUID gEfiPeiDeviceRecoveryModulePpiGuid;

#endif  /* _PEI_DEVICE_RECOVERY_MODULE_PPI_H_ */
