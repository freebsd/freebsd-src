/** @file
  EFI MM Access PPI definition.

  This PPI is used to control the visibility of the MMRAM on the platform.
  The EFI_PEI_MM_ACCESS_PPI abstracts the location and characteristics of MMRAM. The
  principal functionality found in the memory controller includes the following:
  - Exposing the MMRAM to all non-MM agents, or the "open" state
  - Shrouding the MMRAM to all but the MM agents, or the "closed" state
  - Preserving the system integrity, or "locking" the MMRAM, such that the settings cannot be
    perturbed by either boot service or runtime agents

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.5.

**/

#ifndef _MM_ACCESS_PPI_H_
#define _MM_ACCESS_PPI_H_

#define EFI_PEI_MM_ACCESS_PPI_GUID \
  { 0x268f33a9, 0xcccd, 0x48be, { 0x88, 0x17, 0x86, 0x5, 0x3a, 0xc3, 0x2e, 0xd6 }}

typedef struct _EFI_PEI_MM_ACCESS_PPI EFI_PEI_MM_ACCESS_PPI;

/**
  Opens the MMRAM area to be accessible by a PEIM.

  This function "opens" MMRAM so that it is visible while not inside of MM. The function should
  return EFI_UNSUPPORTED if the hardware does not support hiding of MMRAM. The function
  should return EFI_DEVICE_ERROR if the MMRAM configuration is locked.

  @param  PeiServices            An indirect pointer to the PEI Services Table published by the PEI Foundation.
  @param  This                   The EFI_PEI_MM_ACCESS_PPI instance.
  @param  DescriptorIndex        The region of MMRAM to Open.

  @retval EFI_SUCCESS            The operation was successful.
  @retval EFI_UNSUPPORTED        The system does not support opening and closing of MMRAM.
  @retval EFI_DEVICE_ERROR       MMRAM cannot be opened, perhaps because it is locked.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_MM_OPEN)(
  IN EFI_PEI_SERVICES                **PeiServices,
  IN EFI_PEI_MM_ACCESS_PPI           *This,
  IN UINTN                           DescriptorIndex
  );

/**
  Inhibits access to the MMRAM.

  This function "closes" MMRAM so that it is not visible while outside of MM. The function should
  return EFI_UNSUPPORTED if the hardware does not support hiding of MMRAM.

  @param  PeiServices            An indirect pointer to the PEI Services Table published by the PEI Foundation.
  @param  This                   The EFI_PEI_MM_ACCESS_PPI instance.
  @param  DescriptorIndex        The region of MMRAM to Close.

  @retval EFI_SUCCESS            The operation was successful.
  @retval EFI_UNSUPPORTED        The system does not support opening and closing of MMRAM.
  @retval EFI_DEVICE_ERROR       MMRAM cannot be closed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_MM_CLOSE)(
  IN EFI_PEI_SERVICES                **PeiServices,
  IN EFI_PEI_MM_ACCESS_PPI           *This,
  IN UINTN                           DescriptorIndex
  );

/**
  This function prohibits access to the MMRAM region. This function is usually implemented such
  that it is a write-once operation.

  @param  PeiServices            An indirect pointer to the PEI Services Table published by the PEI Foundation.
  @param  This                   The EFI_PEI_MM_ACCESS_PPI instance.
  @param  DescriptorIndex        The region of MMRAM to Lock.

  @retval EFI_SUCCESS            The operation was successful.
  @retval EFI_UNSUPPORTED        The system does not support opening and closing of MMRAM.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_MM_LOCK)(
  IN EFI_PEI_SERVICES                **PeiServices,
  IN EFI_PEI_MM_ACCESS_PPI           *This,
  IN UINTN                           DescriptorIndex
  );

/**
  Queries the memory controller for the possible regions that will support MMRAM.

  This function describes the MMRAM regions.
  This data structure forms the contract between the MM_ACCESS and MM_IPL drivers. There is an
  ambiguity when any MMRAM region is remapped. For example, on some chipsets, some MMRAM
  regions can be initialized at one physical address but is later accessed at another processor address.
  There is currently no way for the MM IPL driver to know that it must use two different addresses
  depending on what it is trying to do. As a result, initial configuration and loading can use the
  physical address PhysicalStart while MMRAM is open. However, once the region has been
  closed and needs to be accessed by agents in MM, the CpuStart address must be used.
  This PPI publishes the available memory that the chipset can shroud for the use of installing code.
  These regions serve the dual purpose of describing which regions have been open, closed, or locked.
  In addition, these regions may include overlapping memory ranges, depending on the chipset
  implementation. The latter might include a chipset that supports T-SEG, where memory near the top
  of the physical DRAM can be allocated for MMRAM too.
  The key thing to note is that the regions that are described by the PPI are a subset of the capabilities
  of the hardware.

  @param  PeiServices            An indirect pointer to the PEI Services Table published by the PEI Foundation.
  @param  This                   The EFI_PEI_MM_ACCESS_PPI instance.
  @param  MmramMapSize           A pointer to the size, in bytes, of the MmramMemoryMap buffer. On input, this value is
                                 the size of the buffer that is allocated by the caller. On output, it is the size of the
                                 buffer that was returned by the firmware if the buffer was large enough, or, if the
                                 buffer was too small, the size of the buffer that is needed to contain the map.
  @param MmramMap                A pointer to the buffer in which firmware places the current memory map. The map is
                                 an array of EFI_MMRAM_DESCRIPTORs

  @retval EFI_SUCCESS            The chipset supported the given resource.
  @retval EFI_BUFFER_TOO_SMALL   The MmramMap parameter was too small. The current
                                 buffer size needed to hold the memory map is returned in
                                 MmramMapSize.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_MM_CAPABILITIES)(
  IN EFI_PEI_SERVICES                **PeiServices,
  IN EFI_PEI_MM_ACCESS_PPI           *This,
  IN OUT UINTN                       *MmramMapSize,
  IN OUT EFI_MMRAM_DESCRIPTOR        *MmramMap
  );

///
///  EFI MM Access PPI is used to control the visibility of the MMRAM on the platform.
///  It abstracts the location and characteristics of MMRAM. The platform should report
///  all MMRAM via EFI_PEI_MM_ACCESS_PPI. The expectation is that the north bridge or
///  memory controller would publish this PPI.
///
struct _EFI_PEI_MM_ACCESS_PPI {
  EFI_PEI_MM_OPEN            Open;
  EFI_PEI_MM_CLOSE           Close;
  EFI_PEI_MM_LOCK            Lock;
  EFI_PEI_MM_CAPABILITIES    GetCapabilities;
  BOOLEAN                    LockState;
  BOOLEAN                    OpenState;
};

extern EFI_GUID  gEfiPeiMmAccessPpiGuid;

#endif
