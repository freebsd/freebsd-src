/** @file
  This file declares Recovery Module PPI.  This PPI is used to find and load the
  recovery files.

  A module that produces this PPI has many roles and is responsible for the following:
    -# Calling the driver recovery PPI EFI_PEI_DEVICE_RECOVERY_MODULE_PPI.
       GetNumberRecoveryCapsules() to determine if one or more DXE recovery 
       entities exist.
    -# If no capsules exist, then performing appropriate error handling.
    -# Allocating a buffer of MaxRecoveryCapsuleSize as determined by
       EFI_PEI_DEVICE_RECOVERY_MODULE_PPI.GetRecoveryCapsuleInfo() or
       larger.
    -# Determining the policy in which DXE recovery capsules are loaded.
    -# Calling the driver recovery PPI EFI_PEI_DEVICE_RECOVERY_MODULE_PPI.
       LoadRecoveryCapsule() for capsule number x.
    -# If the load failed, performing appropriate error handling.
    -# Performing security checks for a loaded DXE recovery capsule.
    -# If the security checks failed, then logging the failure in a data HOB.
    -# If the security checks failed, then determining the next 
       EFI_PEI_DEVICE_RECOVERY_MODULE_PPI.LoadRecoveryCapsule()capsule number; 
       otherwise, go to step 11.
    -# If more DXE recovery capsules exist, then go to step 5; otherwise, perform 
       error handling.
    -# Decomposing the capsule loaded by EFI_PEI_DEVICE_RECOVERY_MODULE_PPI.
       LoadRecoveryCapsule() into its components. It is assumed that the path 
       parameters are redundant for recovery and Setup parameters are either 
       redundant or canned.
    -# Invalidating all HOB entries for updateable firmware volume entries. 
       This invalidation prevents possible errant drivers from being executed.
    -# Updating the HOB table with the recovery DXE firmware volume information 
       generated from the capsule decomposition.
    -# Returning to the PEI Dispatcher.  
  
  Copyright (c) 2007 - 2011, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  @par Revision Reference:
  This PPI is defined in UEFI Platform Initialization Specification 1.2 Errata B Volume 1: 
  Pre-EFI Initalization Core Interface

**/

#ifndef __PEI_RECOVERY_MODULE_PPI_H__
#define __PEI_RECOVERY_MODULE_PPI_H__

#define EFI_PEI_RECOVERY_MODULE_PPI_GUID \
  { \
    0xFB6D9542, 0x612D, 0x4f45, {0x87, 0x2F, 0x5C, 0xFF, 0x52, 0xE9, 0x3D, 0xCF } \
  }

typedef struct _EFI_PEI_RECOVERY_MODULE_PPI EFI_PEI_RECOVERY_MODULE_PPI;

/**
  Loads a DXE capsule from some media into memory and updates the HOB table
  with the DXE firmware volume information.

  @param  PeiServices   General-purpose services that are available to every PEIM.
  @param  This          Indicates the EFI_PEI_RECOVERY_MODULE_PPI instance.

  @retval EFI_SUCCESS        The capsule was loaded correctly.
  @retval EFI_DEVICE_ERROR   A device error occurred.
  @retval EFI_NOT_FOUND      A recovery DXE capsule cannot be found.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_LOAD_RECOVERY_CAPSULE)(
  IN EFI_PEI_SERVICES             **PeiServices,
  IN EFI_PEI_RECOVERY_MODULE_PPI  *This
  );

///
///  Finds and loads the recovery files.
///
struct _EFI_PEI_RECOVERY_MODULE_PPI {
  EFI_PEI_LOAD_RECOVERY_CAPSULE LoadRecoveryCapsule;  ///< Loads a DXE binary capsule into memory.
};

extern EFI_GUID gEfiPeiRecoveryModulePpiGuid;

#endif
