/** @file
  Defines the APIs that enable PEI services to work with
  the underlying capsule capabilities of the platform.

Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.4.

**/

#ifndef _PEI_CAPSULE_PPI_H_
#define _PEI_CAPSULE_PPI_H_

///
/// Global ID for the EFI_PEI_CAPSULE_PPI.
///
#define EFI_PEI_CAPSULE_PPI_GUID \
  { \
    0x3acf33ee, 0xd892, 0x40f4, {0xa2, 0xfc, 0x38, 0x54, 0xd2, 0xe1, 0x32, 0x3d } \
  }

///
/// Forward declaration for the EFI_PEI_CAPSULE_PPI.
///
typedef struct _EFI_PEI_CAPSULE_PPI EFI_PEI_CAPSULE_PPI;

///
/// Keep name backwards compatible before PI Version 1.4
///
typedef struct _EFI_PEI_CAPSULE_PPI PEI_CAPSULE_PPI;

/**
  Upon determining that there is a capsule to operate on, this service
  will use a series of EFI_CAPSULE_BLOCK_DESCRIPTOR entries to determine
  the current location of the various capsule fragments and coalesce them
  into a contiguous region of system memory.

  @param[in]  PeiServices   Pointer to the PEI Services Table.
  @param[out] MemoryBase    Pointer to the base of a block of memory into which the buffers will be coalesced.
                            On output, this variable will hold the base address
                            of a coalesced capsule.
  @param[out] MemorySize    Size of the memory region pointed to by MemoryBase.
                            On output, this variable will contain the size of the
                            coalesced capsule.

  @retval EFI_NOT_FOUND          If: boot mode could not be determined, or the
                                 boot mode is not flash-update, or the capsule descriptors were not found.
  @retval EFI_BUFFER_TOO_SMALL   The capsule could not be coalesced in the provided memory region.
  @retval EFI_SUCCESS            There was no capsule, or the capsule was processed successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_CAPSULE_COALESCE)(
  IN EFI_PEI_SERVICES  **PeiServices,
  IN OUT VOID          **MemoryBase,
  IN OUT UINTN         *MemSize
  );

/**
  Determine if a capsule needs to be processed.
  The means by which the presence of a capsule is determined is platform
  specific. For example, an implementation could be driven by the presence
  of a Capsule EFI Variable containing a list of EFI_CAPSULE_BLOCK_DESCRIPTOR
  entries. If present, return EFI_SUCCESS, otherwise return EFI_NOT_FOUND.

  @param[in] PeiServices   Pointer to the PEI Services Table.

  @retval EFI_SUCCESS     If a capsule is available.
  @retval EFI_NOT_FOUND   No capsule detected.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_CAPSULE_CHECK_CAPSULE_UPDATE)(
  IN EFI_PEI_SERVICES  **PeiServices
  );

/**
  The Capsule PPI service that gets called after memory is available. The
  capsule coalesce function, which must be called first, returns a base
  address and size. Once the memory init PEIM has discovered memory,
  it should call this function and pass in the base address and size
  returned by the Coalesce() function. Then this function can create a
  capsule HOB and return.

  @par Notes:
    This function assumes it will not be called until the
    actual capsule update.

  @param[in] PeiServices   Pointer to the PEI Services Table.
  @param[in] CapsuleBase   Address returned by the capsule coalesce function.
  @param[in] CapsuleSize   Value returned by the capsule coalesce function.

  @retval EFI_VOLUME_CORRUPTED   CapsuleBase does not appear to point to a
                                 coalesced capsule.
  @retval EFI_SUCCESS            Capsule HOB was created successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_CAPSULE_CREATE_STATE)(
  IN EFI_PEI_SERVICES  **PeiServices,
  IN VOID              *CapsuleBase,
  IN UINTN             CapsuleSize
  );

///
/// This PPI provides several services in PEI to work with the underlying
/// capsule capabilities of the platform.  These services include the ability
/// for PEI to coalesce a capsule from a scattered set of memory locations
/// into a contiguous space in memory, detect if a capsule is present for
/// processing, and once memory is available, create a HOB for the capsule.
///
struct _EFI_PEI_CAPSULE_PPI {
  EFI_PEI_CAPSULE_COALESCE                Coalesce;
  EFI_PEI_CAPSULE_CHECK_CAPSULE_UPDATE    CheckCapsuleUpdate;
  EFI_PEI_CAPSULE_CREATE_STATE            CreateState;
};

///
/// Keep name backwards compatible before PI Version 1.4
///
extern EFI_GUID  gPeiCapsulePpiGuid;

extern EFI_GUID  gEfiPeiCapsulePpiGuid;

#endif // #ifndef _PEI_CAPSULE_PPI_H_
