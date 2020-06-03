/** @file
  Include file matches things in PI.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  PI Version 1.7

**/

#ifndef __PI_DXECIS_H__
#define __PI_DXECIS_H__

#include <Uefi/UefiMultiPhase.h>
#include <Pi/PiMultiPhase.h>

///
/// Global Coherencey Domain types - Memory type.
///
typedef enum {
  ///
  /// A memory region that is visible to the boot processor. However, there are no system
  /// components that are currently decoding this memory region.
  ///
  EfiGcdMemoryTypeNonExistent,
  ///
  /// A memory region that is visible to the boot processor. This memory region is being
  /// decoded by a system component, but the memory region is not considered to be either
  /// system memory or memory-mapped I/O.
  ///
  EfiGcdMemoryTypeReserved,
  ///
  /// A memory region that is visible to the boot processor. A memory controller is
  /// currently decoding this memory region and the memory controller is producing a
  /// tested system memory region that is available to the memory services.
  ///
  EfiGcdMemoryTypeSystemMemory,
  ///
  /// A memory region that is visible to the boot processor. This memory region is
  /// currently being decoded by a component as memory-mapped I/O that can be used to
  /// access I/O devices in the platform.
  ///
  EfiGcdMemoryTypeMemoryMappedIo,
  ///
  /// A memory region that is visible to the boot processor.
  /// This memory supports byte-addressable non-volatility.
  ///
  EfiGcdMemoryTypePersistent,
  //
  // Keep original one for the compatibility.
  //
  EfiGcdMemoryTypePersistentMemory = EfiGcdMemoryTypePersistent,
  ///
  /// A memory region that provides higher reliability relative to other memory in the
  /// system. If all memory has the same reliability, then this bit is not used.
  ///
  EfiGcdMemoryTypeMoreReliable,
  EfiGcdMemoryTypeMaximum
} EFI_GCD_MEMORY_TYPE;

///
/// Global Coherencey Domain types - IO type.
///
typedef enum {
  ///
  /// An I/O region that is visible to the boot processor. However, there are no system
  /// components that are currently decoding this I/O region.
  ///
  EfiGcdIoTypeNonExistent,
  ///
  /// An I/O region that is visible to the boot processor. This I/O region is currently being
  /// decoded by a system component, but the I/O region cannot be used to access I/O devices.
  ///
  EfiGcdIoTypeReserved,
  ///
  /// An I/O region that is visible to the boot processor. This I/O region is currently being
  /// decoded by a system component that is producing I/O ports that can be used to access I/O devices.
  ///
  EfiGcdIoTypeIo,
  EfiGcdIoTypeMaximum
} EFI_GCD_IO_TYPE;

///
/// The type of allocation to perform.
///
typedef enum {
  ///
  /// The GCD memory space map is searched from the lowest address up to the highest address
  /// looking for unallocated memory ranges.
  ///
  EfiGcdAllocateAnySearchBottomUp,
  ///
  /// The GCD memory space map is searched from the lowest address up
  /// to the specified MaxAddress looking for unallocated memory ranges.
  ///
  EfiGcdAllocateMaxAddressSearchBottomUp,
  ///
  /// The GCD memory space map is checked to see if the memory range starting
  /// at the specified Address is available.
  ///
  EfiGcdAllocateAddress,
  ///
  /// The GCD memory space map is searched from the highest address down to the lowest address
  /// looking for unallocated memory ranges.
  ///
  EfiGcdAllocateAnySearchTopDown,
  ///
  /// The GCD memory space map is searched from the specified MaxAddress
  /// down to the lowest address looking for unallocated memory ranges.
  ///
  EfiGcdAllocateMaxAddressSearchTopDown,
  EfiGcdMaxAllocateType
} EFI_GCD_ALLOCATE_TYPE;

///
/// EFI_GCD_MEMORY_SPACE_DESCRIPTOR.
///
typedef struct {
  ///
  /// The physical address of the first byte in the memory region. Type
  /// EFI_PHYSICAL_ADDRESS is defined in the AllocatePages() function
  /// description in the UEFI 2.0 specification.
  ///
  EFI_PHYSICAL_ADDRESS  BaseAddress;

  ///
  /// The number of bytes in the memory region.
  ///
  UINT64                Length;

  ///
  /// The bit mask of attributes that the memory region is capable of supporting. The bit
  /// mask of available attributes is defined in the GetMemoryMap() function description
  /// in the UEFI 2.0 specification.
  ///
  UINT64                Capabilities;
  ///
  /// The bit mask of attributes that the memory region is currently using. The bit mask of
  /// available attributes is defined in GetMemoryMap().
  ///
  UINT64                Attributes;
  ///
  /// Type of the memory region. Type EFI_GCD_MEMORY_TYPE is defined in the
  /// AddMemorySpace() function description.
  ///
  EFI_GCD_MEMORY_TYPE   GcdMemoryType;

  ///
  /// The image handle of the agent that allocated the memory resource described by
  /// PhysicalStart and NumberOfBytes. If this field is NULL, then the memory
  /// resource is not currently allocated. Type EFI_HANDLE is defined in
  /// InstallProtocolInterface() in the UEFI 2.0 specification.
  ///
  EFI_HANDLE            ImageHandle;

  ///
  /// The device handle for which the memory resource has been allocated. If
  /// ImageHandle is NULL, then the memory resource is not currently allocated. If this
  /// field is NULL, then the memory resource is not associated with a device that is
  /// described by a device handle. Type EFI_HANDLE is defined in
  /// InstallProtocolInterface() in the UEFI 2.0 specification.
  ///
  EFI_HANDLE            DeviceHandle;
} EFI_GCD_MEMORY_SPACE_DESCRIPTOR;

///
/// EFI_GCD_IO_SPACE_DESCRIPTOR.
///
typedef struct {
  ///
  /// Physical address of the first byte in the I/O region. Type
  /// EFI_PHYSICAL_ADDRESS is defined in the AllocatePages() function
  /// description in the UEFI 2.0 specification.
  ///
  EFI_PHYSICAL_ADDRESS  BaseAddress;

  ///
  /// Number of bytes in the I/O region.
  ///
  UINT64                Length;

  ///
  /// Type of the I/O region. Type EFI_GCD_IO_TYPE is defined in the
  /// AddIoSpace() function description.
  ///
  EFI_GCD_IO_TYPE       GcdIoType;

  ///
  /// The image handle of the agent that allocated the I/O resource described by
  /// PhysicalStart and NumberOfBytes. If this field is NULL, then the I/O
  /// resource is not currently allocated. Type EFI_HANDLE is defined in
  /// InstallProtocolInterface() in the UEFI 2.0 specification.
  ///
  EFI_HANDLE            ImageHandle;

  ///
  /// The device handle for which the I/O resource has been allocated. If ImageHandle
  /// is NULL, then the I/O resource is not currently allocated. If this field is NULL, then
  /// the I/O resource is not associated with a device that is described by a device handle.
  /// Type EFI_HANDLE is defined in InstallProtocolInterface() in the UEFI
  /// 2.0 specification.
  ///
  EFI_HANDLE            DeviceHandle;
} EFI_GCD_IO_SPACE_DESCRIPTOR;


/**
  Adds reserved memory, system memory, or memory-mapped I/O resources to the
  global coherency domain of the processor.

  @param  GcdMemoryType    The type of memory resource being added.
  @param  BaseAddress      The physical address that is the start address
                           of the memory resource being added.
  @param  Length           The size, in bytes, of the memory resource that
                           is being added.
  @param  Capabilities     The bit mask of attributes that the memory
                           resource region supports.

  @retval EFI_SUCCESS            The memory resource was added to the global
                                 coherency domain of the processor.
  @retval EFI_INVALID_PARAMETER  GcdMemoryType is invalid.
  @retval EFI_INVALID_PARAMETER  Length is zero.
  @retval EFI_OUT_OF_RESOURCES   There are not enough system resources to add
                                 the memory resource to the global coherency
                                 domain of the processor.
  @retval EFI_UNSUPPORTED        The processor does not support one or more bytes
                                 of the memory resource range specified by
                                 BaseAddress and Length.
  @retval EFI_ACCESS_DENIED      One or more bytes of the memory resource range
                                 specified by BaseAddress and Length conflicts
                                 with a memory resource range that was previously
                                 added to the global coherency domain of the processor.
  @retval EFI_ACCESS_DENIED      One or more bytes of the memory resource range
                                 specified by BaseAddress and Length was allocated
                                 in a prior call to AllocateMemorySpace().

**/
typedef
EFI_STATUS
(EFIAPI *EFI_ADD_MEMORY_SPACE)(
  IN EFI_GCD_MEMORY_TYPE   GcdMemoryType,
  IN EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN UINT64                Length,
  IN UINT64                Capabilities
  );

/**
  Allocates nonexistent memory, reserved memory, system memory, or memorymapped
  I/O resources from the global coherency domain of the processor.

  @param  GcdAllocateType  The type of allocation to perform.
  @param  GcdMemoryType    The type of memory resource being allocated.
  @param  Alignment        The log base 2 of the boundary that BaseAddress must
                           be aligned on output. Align with 2^Alignment.
  @param  Length           The size in bytes of the memory resource range that
                           is being allocated.
  @param  BaseAddress      A pointer to a physical address to allocate.
  @param  Imagehandle      The image handle of the agent that is allocating
                           the memory resource.
  @param  DeviceHandle     The device handle for which the memory resource
                           is being allocated.

  @retval EFI_INVALID_PARAMETER GcdAllocateType is invalid.
  @retval EFI_INVALID_PARAMETER GcdMemoryType is invalid.
  @retval EFI_INVALID_PARAMETER Length is zero.
  @retval EFI_INVALID_PARAMETER BaseAddress is NULL.
  @retval EFI_INVALID_PARAMETER ImageHandle is NULL.
  @retval EFI_NOT_FOUND         The memory resource request could not be satisfied.
                                No descriptor contains the desired space.
  @retval EFI_OUT_OF_RESOURCES  There are not enough system resources to allocate the memory
                                resource from the global coherency domain of the processor.
  @retval EFI_SUCCESS           The memory resource was allocated from the global coherency
                                domain of the processor.


**/
typedef
EFI_STATUS
(EFIAPI *EFI_ALLOCATE_MEMORY_SPACE)(
  IN     EFI_GCD_ALLOCATE_TYPE               GcdAllocateType,
  IN     EFI_GCD_MEMORY_TYPE                 GcdMemoryType,
  IN     UINTN                               Alignment,
  IN     UINT64                              Length,
  IN OUT EFI_PHYSICAL_ADDRESS                *BaseAddress,
  IN     EFI_HANDLE                          ImageHandle,
  IN     EFI_HANDLE                          DeviceHandle OPTIONAL
  );

/**
  Frees nonexistent memory, reserved memory, system memory, or memory-mapped
  I/O resources from the global coherency domain of the processor.

  @param  BaseAddress      The physical address that is the start address of the memory resource being freed.
  @param  Length           The size in bytes of the memory resource range that is being freed.

  @retval EFI_SUCCESS           The memory resource was freed from the global coherency domain of
                                the processor.
  @retval EFI_INVALID_PARAMETER Length is zero.
  @retval EFI_UNSUPPORTED       The processor does not support one or more bytes of the memory
                                resource range specified by BaseAddress and Length.
  @retval EFI_NOT_FOUND         The memory resource range specified by BaseAddress and
                                Length was not allocated with previous calls to AllocateMemorySpace().
  @retval EFI_OUT_OF_RESOURCES  There are not enough system resources to free the memory resource
                                from the global coherency domain of the processor.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FREE_MEMORY_SPACE)(
  IN EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN UINT64                Length
  );

/**
  Removes reserved memory, system memory, or memory-mapped I/O resources from
  the global coherency domain of the processor.

  @param  BaseAddress      The physical address that is the start address of the memory resource being removed.
  @param  Length           The size in bytes of the memory resource that is being removed.

  @retval EFI_SUCCESS           The memory resource was removed from the global coherency
                                domain of the processor.
  @retval EFI_INVALID_PARAMETER Length is zero.
  @retval EFI_UNSUPPORTED       The processor does not support one or more bytes of the memory
                                resource range specified by BaseAddress and Length.
  @retval EFI_NOT_FOUND         One or more bytes of the memory resource range specified by
                                BaseAddress and Length was not added with previous calls to
                                AddMemorySpace().
  @retval EFI_ACCESS_DEFINED    One or more bytes of the memory resource range specified by
                                BaseAddress and Length has been allocated with AllocateMemorySpace().
  @retval EFI_OUT_OF_RESOURCES  There are not enough system resources to remove the memory
                                resource from the global coherency domain of the processor.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REMOVE_MEMORY_SPACE)(
  IN EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN UINT64                Length
  );

/**
  Retrieves the descriptor for a memory region containing a specified address.

  @param  BaseAddress      The physical address that is the start address of a memory region.
  @param  Descriptor       A pointer to a caller allocated descriptor.

  @retval EFI_SUCCESS           The descriptor for the memory resource region containing
                                BaseAddress was returned in Descriptor.
  @retval EFI_INVALID_PARAMETER Descriptor is NULL.
  @retval EFI_NOT_FOUND         A memory resource range containing BaseAddress was not found.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_GET_MEMORY_SPACE_DESCRIPTOR)(
  IN  EFI_PHYSICAL_ADDRESS             BaseAddress,
  OUT EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *Descriptor
  );

/**
  Modifies the attributes for a memory region in the global coherency domain of the
  processor.

  @param  BaseAddress      The physical address that is the start address of a memory region.
  @param  Length           The size in bytes of the memory region.
  @param  Attributes       The bit mask of attributes to set for the memory region.

  @retval EFI_SUCCESS           The attributes were set for the memory region.
  @retval EFI_INVALID_PARAMETER Length is zero.
  @retval EFI_UNSUPPORTED       The processor does not support one or more bytes of the memory
                                resource range specified by BaseAddress and Length.
  @retval EFI_UNSUPPORTED       The bit mask of attributes is not support for the memory resource
                                range specified by BaseAddress and Length.
  @retval EFI_ACCESS_DENIED     The attributes for the memory resource range specified by
                                BaseAddress and Length cannot be modified.
  @retval EFI_OUT_OF_RESOURCES  There are not enough system resources to modify the attributes of
                                the memory resource range.
  @retval EFI_NOT_AVAILABLE_YET The attributes cannot be set because CPU architectural protocol is
                                not available yet.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SET_MEMORY_SPACE_ATTRIBUTES)(
  IN EFI_PHYSICAL_ADDRESS         BaseAddress,
  IN UINT64                       Length,
  IN UINT64                       Attributes
  );

/**
  Modifies the capabilities for a memory region in the global coherency domain of the
  processor.

  @param  BaseAddress      The physical address that is the start address of a memory region.
  @param  Length           The size in bytes of the memory region.
  @param  Capabilities     The bit mask of capabilities that the memory region supports.

  @retval EFI_SUCCESS           The capabilities were set for the memory region.
  @retval EFI_INVALID_PARAMETER Length is zero.
  @retval EFI_UNSUPPORTED       The capabilities specified by Capabilities do not include the
                                memory region attributes currently in use.
  @retval EFI_ACCESS_DENIED     The capabilities for the memory resource range specified by
                                BaseAddress and Length cannot be modified.
  @retval EFI_OUT_OF_RESOURCES  There are not enough system resources to modify the capabilities
                                of the memory resource range.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SET_MEMORY_SPACE_CAPABILITIES) (
  IN EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN UINT64                Length,
  IN UINT64                Capabilities
  );

/**
  Returns a map of the memory resources in the global coherency domain of the
  processor.

  @param  NumberOfDescriptors A pointer to number of descriptors returned in the MemorySpaceMap buffer.
  @param  MemorySpaceMap      A pointer to the array of EFI_GCD_MEMORY_SPACE_DESCRIPTORs.

  @retval EFI_SUCCESS           The memory space map was returned in the MemorySpaceMap
                                buffer, and the number of descriptors in MemorySpaceMap was
                                returned in NumberOfDescriptors.
  @retval EFI_INVALID_PARAMETER NumberOfDescriptors is NULL.
  @retval EFI_INVALID_PARAMETER MemorySpaceMap is NULL.
  @retval EFI_OUT_OF_RESOURCES  There are not enough resources to allocate MemorySpaceMap.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_GET_MEMORY_SPACE_MAP)(
  OUT UINTN                            *NumberOfDescriptors,
  OUT EFI_GCD_MEMORY_SPACE_DESCRIPTOR  **MemorySpaceMap
  );

/**
  Adds reserved I/O or I/O resources to the global coherency domain of the processor.

  @param  GcdIoType        The type of I/O resource being added.
  @param  BaseAddress      The physical address that is the start address of the I/O resource being added.
  @param  Length           The size in bytes of the I/O resource that is being added.

  @retval EFI_SUCCESS           The I/O resource was added to the global coherency domain of
                                the processor.
  @retval EFI_INVALID_PARAMETER GcdIoType is invalid.
  @retval EFI_INVALID_PARAMETER Length is zero.
  @retval EFI_OUT_OF_RESOURCES  There are not enough system resources to add the I/O resource to
                                the global coherency domain of the processor.
  @retval EFI_UNSUPPORTED       The processor does not support one or more bytes of the I/O
                                resource range specified by BaseAddress and Length.
  @retval EFI_ACCESS_DENIED     One or more bytes of the I/O resource range specified by
                                BaseAddress and Length conflicts with an I/O resource
                                range that was previously added to the global coherency domain
                                of the processor.
  @retval EFI_ACCESS_DENIED     One or more bytes of the I/O resource range specified by
                                BaseAddress and Length was allocated in a prior call to
                                AllocateIoSpace().

**/
typedef
EFI_STATUS
(EFIAPI *EFI_ADD_IO_SPACE)(
  IN EFI_GCD_IO_TYPE       GcdIoType,
  IN EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN UINT64                Length
  );

/**
  Allocates nonexistent I/O, reserved I/O, or I/O resources from the global coherency
  domain of the processor.

  @param  GcdAllocateType  The type of allocation to perform.
  @param  GcdIoType        The type of I/O resource being allocated.
  @param  Alignment        The log base 2 of the boundary that BaseAddress must be aligned on output.
  @param  Length           The size in bytes of the I/O resource range that is being allocated.
  @param  BaseAddress      A pointer to a physical address.
  @param  Imagehandle      The image handle of the agent that is allocating the I/O resource.
  @param  DeviceHandle     The device handle for which the I/O resource is being allocated.

  @retval EFI_SUCCESS           The I/O resource was allocated from the global coherency domain
                                of the processor.
  @retval EFI_INVALID_PARAMETER GcdAllocateType is invalid.
  @retval EFI_INVALID_PARAMETER GcdIoType is invalid.
  @retval EFI_INVALID_PARAMETER Length is zero.
  @retval EFI_INVALID_PARAMETER BaseAddress is NULL.
  @retval EFI_INVALID_PARAMETER ImageHandle is NULL.
  @retval EFI_OUT_OF_RESOURCES  There are not enough system resources to allocate the I/O
                                resource from the global coherency domain of the processor.
  @retval EFI_NOT_FOUND         The I/O resource request could not be satisfied.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_ALLOCATE_IO_SPACE)(
  IN     EFI_GCD_ALLOCATE_TYPE               GcdAllocateType,
  IN     EFI_GCD_IO_TYPE                     GcdIoType,
  IN     UINTN                               Alignment,
  IN     UINT64                              Length,
  IN OUT EFI_PHYSICAL_ADDRESS                *BaseAddress,
  IN     EFI_HANDLE                          ImageHandle,
  IN     EFI_HANDLE                          DeviceHandle OPTIONAL
  );

/**
  Frees nonexistent I/O, reserved I/O, or I/O resources from the global coherency
  domain of the processor.

  @param  BaseAddress      The physical address that is the start address of the I/O resource being freed.
  @param  Length           The size in bytes of the I/O resource range that is being freed.

  @retval EFI_SUCCESS           The I/O resource was freed from the global coherency domain of the
                                processor.
  @retval EFI_INVALID_PARAMETER Length is zero.
  @retval EFI_UNSUPPORTED       The processor does not support one or more bytes of the I/O resource
                                range specified by BaseAddress and Length.
  @retval EFI_NOT_FOUND         The I/O resource range specified by BaseAddress and Length
                                was not allocated with previous calls to AllocateIoSpace().
  @retval EFI_OUT_OF_RESOURCES  There are not enough system resources to free the I/O resource from
                                the global coherency domain of the processor.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FREE_IO_SPACE)(
  IN EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN UINT64                Length
  );

/**
  Removes reserved I/O or I/O resources from the global coherency domain of the
  processor.

  @param  BaseAddress      A pointer to a physical address that is the start address of the I/O resource being
                           removed.
  @param Length            The size in bytes of the I/O resource that is being removed.

  @retval EFI_SUCCESS           The I/O resource was removed from the global coherency domain
                                of the processor.
  @retval EFI_INVALID_PARAMETER Length is zero.
  @retval EFI_UNSUPPORTED       The processor does not support one or more bytes of the I/O
                                resource range specified by BaseAddress and Length.
  @retval EFI_NOT_FOUND         One or more bytes of the I/O resource range specified by
                                BaseAddress and Length was not added with previous
                                calls to AddIoSpace().
  @retval EFI_ACCESS_DENIED     One or more bytes of the I/O resource range specified by
                                BaseAddress and Length has been allocated with
                                AllocateIoSpace().
  @retval EFI_OUT_OF_RESOURCES  There are not enough system resources to remove the I/O
                                resource from the global coherency domain of the processor.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REMOVE_IO_SPACE)(
  IN EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN UINT64                Length
  );

/**
  Retrieves the descriptor for an I/O region containing a specified address.

  @param  BaseAddress      The physical address that is the start address of an I/O region.
  @param  Descriptor       A pointer to a caller allocated descriptor.

  @retval EFI_SUCCESS           The descriptor for the I/O resource region containing
                                BaseAddress was returned in Descriptor.
  @retval EFI_INVALID_PARAMETER Descriptor is NULL.
  @retval EFI_NOT_FOUND         An I/O resource range containing BaseAddress was not found.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_GET_IO_SPACE_DESCRIPTOR)(
  IN  EFI_PHYSICAL_ADDRESS         BaseAddress,
  OUT EFI_GCD_IO_SPACE_DESCRIPTOR  *Descriptor
  );

/**
  Returns a map of the I/O resources in the global coherency domain of the processor.

  @param  NumberOfDescriptors A pointer to number of descriptors returned in the IoSpaceMap buffer.
  @param  MemorySpaceMap      A pointer to the array of EFI_GCD_IO_SPACE_DESCRIPTORs.

  @retval EFI_SUCCESS           The I/O space map was returned in the IoSpaceMap buffer, and
                                the number of descriptors in IoSpaceMap was returned in
                                NumberOfDescriptors.
  @retval EFI_INVALID_PARAMETER NumberOfDescriptors is NULL.
  @retval EFI_INVALID_PARAMETER IoSpaceMap is NULL.
  @retval EFI_OUT_OF_RESOURCES  There are not enough resources to allocate IoSpaceMap.


**/
typedef
EFI_STATUS
(EFIAPI *EFI_GET_IO_SPACE_MAP)(
  OUT UINTN                        *NumberOfDescriptors,
  OUT EFI_GCD_IO_SPACE_DESCRIPTOR  **IoSpaceMap
  );



/**
  Loads and executed DXE drivers from firmware volumes.

  The Dispatch() function searches for DXE drivers in firmware volumes that have been
  installed since the last time the Dispatch() service was called. It then evaluates
  the dependency expressions of all the DXE drivers and loads and executes those DXE
  drivers whose dependency expression evaluate to TRUE. This service must interact with
  the Security Architectural Protocol to authenticate DXE drivers before they are executed.
  This process is continued until no more DXE drivers can be executed.

  @retval EFI_SUCCESS         One or more DXE driver were dispatched.
  @retval EFI_NOT_FOUND       No DXE drivers were dispatched.
  @retval EFI_ALREADY_STARTED An attempt is being made to start the DXE Dispatcher recursively.
                              Thus, no action was taken.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DISPATCH)(
  VOID
  );

/**
  Clears the Schedule on Request (SOR) flag for a component that is stored in a firmware volume.

  @param  FirmwareVolumeHandle The handle of the firmware volume that contains the file specified by FileName.
  @param  FileName             A pointer to the name of the file in a firmware volume.

  @retval EFI_SUCCESS         The DXE driver was found and its SOR bit was cleared.
  @retval EFI_NOT_FOUND       The DXE driver does not exist, or the DXE driver exists and its SOR
                              bit is not set.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SCHEDULE)(
  IN EFI_HANDLE  FirmwareVolumeHandle,
  IN CONST EFI_GUID    *FileName
  );

/**
  Promotes a file stored in a firmware volume from the untrusted to the trusted state.

  @param  FirmwareVolumeHandle The handle of the firmware volume that contains the file specified by FileName.
  @param  DriverName           A pointer to the name of the file in a firmware volume.

  @return Status of promoting FFS from untrusted to trusted
          state.
  @retval EFI_NOT_FOUND       The file was not found in the untrusted state.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TRUST)(
  IN EFI_HANDLE  FirmwareVolumeHandle,
  IN CONST EFI_GUID    *FileName
  );

/**
  Creates a firmware volume handle for a firmware volume that is present in system memory.

  @param  FirmwareVolumeHeader A pointer to the header of the firmware volume.
  @param  Size                 The size, in bytes, of the firmware volume.
  @param  FirmwareVolumeHandle On output, a pointer to the created handle.

  @retval EFI_SUCCESS          The EFI_FIRMWARE_VOLUME_PROTOCOL and
                               EFI_DEVICE_PATH_PROTOCOL were installed onto
                               FirmwareVolumeHandle for the firmware volume described
                               by FirmwareVolumeHeader and Size.
  @retval EFI_VOLUME_CORRUPTED The firmware volume described by FirmwareVolumeHeader
                               and Size is corrupted.
  @retval EFI_OUT_OF_RESOURCES There are not enough system resources available to produce the
                               EFI_FIRMWARE_VOLUME_PROTOCOL and EFI_DEVICE_PATH_PROTOCOL
                               for the firmware volume described by FirmwareVolumeHeader and Size.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PROCESS_FIRMWARE_VOLUME)(
  IN CONST VOID                       *FirmwareVolumeHeader,
  IN UINTN                            Size,
  OUT EFI_HANDLE                      *FirmwareVolumeHandle
  );

//
// DXE Services Table
//
#define DXE_SERVICES_SIGNATURE            0x565245535f455844ULL
#define DXE_SPECIFICATION_MAJOR_REVISION  1
#define DXE_SPECIFICATION_MINOR_REVISION  70
#define DXE_SERVICES_REVISION             ((DXE_SPECIFICATION_MAJOR_REVISION<<16) | (DXE_SPECIFICATION_MINOR_REVISION))

typedef struct {
  ///
  /// The table header for the DXE Services Table.
  /// This header contains the DXE_SERVICES_SIGNATURE and DXE_SERVICES_REVISION values.
  ///
  EFI_TABLE_HEADER                Hdr;

  //
  // Global Coherency Domain Services
  //
  EFI_ADD_MEMORY_SPACE            AddMemorySpace;
  EFI_ALLOCATE_MEMORY_SPACE       AllocateMemorySpace;
  EFI_FREE_MEMORY_SPACE           FreeMemorySpace;
  EFI_REMOVE_MEMORY_SPACE         RemoveMemorySpace;
  EFI_GET_MEMORY_SPACE_DESCRIPTOR GetMemorySpaceDescriptor;
  EFI_SET_MEMORY_SPACE_ATTRIBUTES SetMemorySpaceAttributes;
  EFI_GET_MEMORY_SPACE_MAP        GetMemorySpaceMap;
  EFI_ADD_IO_SPACE                AddIoSpace;
  EFI_ALLOCATE_IO_SPACE           AllocateIoSpace;
  EFI_FREE_IO_SPACE               FreeIoSpace;
  EFI_REMOVE_IO_SPACE             RemoveIoSpace;
  EFI_GET_IO_SPACE_DESCRIPTOR     GetIoSpaceDescriptor;
  EFI_GET_IO_SPACE_MAP            GetIoSpaceMap;

  //
  // Dispatcher Services
  //
  EFI_DISPATCH                    Dispatch;
  EFI_SCHEDULE                    Schedule;
  EFI_TRUST                       Trust;
  //
  // Service to process a single firmware volume found in a capsule
  //
  EFI_PROCESS_FIRMWARE_VOLUME     ProcessFirmwareVolume;
  //
  // Extensions to Global Coherency Domain Services
  //
  EFI_SET_MEMORY_SPACE_CAPABILITIES SetMemorySpaceCapabilities;
} DXE_SERVICES;

typedef DXE_SERVICES EFI_DXE_SERVICES;

#endif
