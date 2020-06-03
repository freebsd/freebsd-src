/** @file
  PI PEI master include file. This file should match the PI spec.

Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  PI Version 1.7.

**/

#ifndef __PI_PEICIS_H__
#define __PI_PEICIS_H__

#include <Uefi/UefiMultiPhase.h>
#include <Pi/PiMultiPhase.h>

///
/// The handles of EFI FV.
///
typedef VOID    *EFI_PEI_FV_HANDLE;

///
/// The handles of EFI FFS.
///
typedef VOID    *EFI_PEI_FILE_HANDLE;

///
/// Declare the forward reference data structure for EFI_PEI_SERVICE.
///
typedef struct _EFI_PEI_SERVICES          EFI_PEI_SERVICES;

///
/// Declare the forward reference data structure for EFI_PEI_NOTIFY_DESCRIPTOR.
///
typedef struct _EFI_PEI_NOTIFY_DESCRIPTOR EFI_PEI_NOTIFY_DESCRIPTOR;


#include <Ppi/CpuIo.h>
#include <Ppi/PciCfg2.h>


/**
  The PEI Dispatcher will invoke each PEIM one time.  During this pass, the PEI
  Dispatcher will pass control to the PEIM at the AddressOfEntryPoint in the PE Header.

  @param  FileHandle       Pointer to the FFS file header.
  @param  PeiServices      Describes the list of possible PEI Services.

  @retval EFI_SUCCESS      The PEI completed successfully.
  @retval !EFI_SUCCESS     There is error in PEIM.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEIM_ENTRY_POINT2)(
  IN EFI_PEI_FILE_HANDLE             FileHandle,
  IN CONST EFI_PEI_SERVICES          **PeiServices
  );

/**
  Entry point of the notification callback function itself within the PEIM.

  @param  PeiServices      Indirect reference to the PEI Services Table.
  @param  NotifyDescriptor Address of the notification descriptor data structure.
  @param  Ppi              Address of the PPI that was installed.

  @return Status of the notification.
          The status code returned from this function is ignored.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEIM_NOTIFY_ENTRY_POINT)(
  IN EFI_PEI_SERVICES           **PeiServices,
  IN EFI_PEI_NOTIFY_DESCRIPTOR  *NotifyDescriptor,
  IN VOID                       *Ppi
  );

//
// PEI Ppi Services List Descriptors
//
#define EFI_PEI_PPI_DESCRIPTOR_PIC              0x00000001
#define EFI_PEI_PPI_DESCRIPTOR_PPI              0x00000010
#define EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK  0x00000020
#define EFI_PEI_PPI_DESCRIPTOR_NOTIFY_DISPATCH  0x00000040
#define EFI_PEI_PPI_DESCRIPTOR_NOTIFY_TYPES     0x00000060
#define EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST   0x80000000

///
/// The data structure through which a PEIM describes available services to the PEI Foundation.
///
typedef struct {
  ///
  /// This field is a set of flags describing the characteristics of this imported table entry.
  /// All flags are defined as EFI_PEI_PPI_DESCRIPTOR_***, which can also be combined into one.
  ///
  UINTN     Flags;
  ///
  /// The address of the EFI_GUID that names the interface.
  ///
  EFI_GUID  *Guid;
  ///
  /// A pointer to the PPI. It contains the information necessary to install a service.
  ///
  VOID      *Ppi;
} EFI_PEI_PPI_DESCRIPTOR;

///
/// The data structure in a given PEIM that tells the PEI
/// Foundation where to invoke the notification service.
///
struct _EFI_PEI_NOTIFY_DESCRIPTOR {
  ///
  /// Details if the type of notification are callback or dispatch.
  ///
  UINTN                       Flags;
  ///
  /// The address of the EFI_GUID that names the interface.
  ///
  EFI_GUID                    *Guid;
  ///
  /// Address of the notification callback function itself within the PEIM.
  ///
  EFI_PEIM_NOTIFY_ENTRY_POINT Notify;
};

///
/// This data structure is the means by which callable services are installed and
/// notifications are registered in the PEI phase.
///
typedef union {
  ///
  /// The typedef structure of the notification descriptor.
  ///
  EFI_PEI_NOTIFY_DESCRIPTOR   Notify;
  ///
  /// The typedef structure of the PPI descriptor.
  ///
  EFI_PEI_PPI_DESCRIPTOR      Ppi;
} EFI_PEI_DESCRIPTOR;

/**
  This service is the first one provided by the PEI Foundation.  This function
  installs an interface in the PEI PPI database by GUID.  The purpose of the
  service is to publish an interface that other parties can use to call
  additional PEIMs.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES table
                           published by the PEI Foundation.
  @param  PpiList          A pointer to the list of interfaces that the caller shall install.

  @retval EFI_SUCCESS           The interface was successfully installed.
  @retval EFI_INVALID_PARAMETER The PpiList pointer is NULL or Any of the PEI PPI
                                descriptors in the list do not have the
                                EFI_PEI_PPI_DESCRIPTOR_PPI bit set in the Flags field.
  @retval EFI_OUT_OF_RESOURCES  There is no additional space in the PPI database.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_INSTALL_PPI)(
  IN CONST EFI_PEI_SERVICES            **PeiServices,
  IN CONST EFI_PEI_PPI_DESCRIPTOR      *PpiList
  );

/**
  This function reinstalls an interface in the PEI PPI database by GUID.
  The purpose of the service is to publish an interface that other parties
  can use to replace a same-named interface in the protocol database
  with a different interface.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES table
                           published by the PEI Foundation.
  @param  OldPpi           A pointer to the former PPI in the database.
  @param  NewPpi           A pointer to the new interfaces that the caller shall install.

  @retval EFI_SUCCESS           The interface was successfully installed.
  @retval EFI_INVALID_PARAMETER The PpiList pointer is NULL or Any of the PEI PPI descriptors in the
                                list do not have the EFI_PEI_PPI_DESCRIPTOR_PPI bit set in the Flags field.
  @retval EFI_OUT_OF_RESOURCES  There is no additional space in the PPI database.
  @retval EFI_NOT_FOUND         The PPI for which the reinstallation was requested has not been installed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_REINSTALL_PPI)(
  IN CONST EFI_PEI_SERVICES                **PeiServices,
  IN CONST EFI_PEI_PPI_DESCRIPTOR          *OldPpi,
  IN CONST EFI_PEI_PPI_DESCRIPTOR          *NewPpi
  );

/**
  This function locates an interface in the PEI PPI database by GUID.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES published by the PEI Foundation.
  @param  Guid             A pointer to the GUID whose corresponding interface needs to be found.
  @param  Instance         The N-th instance of the interface that is required.
  @param  PpiDescriptor    A pointer to instance of the EFI_PEI_PPI_DESCRIPTOR.
  @param  Ppi              A pointer to the instance of the interface.

  @retval EFI_SUCCESS           The interface was successfully returned.
  @retval EFI_NOT_FOUND         The PPI descriptor is not found in the database.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_LOCATE_PPI)(
  IN CONST EFI_PEI_SERVICES            **PeiServices,
  IN CONST EFI_GUID                    *Guid,
  IN UINTN                             Instance,
  IN OUT   EFI_PEI_PPI_DESCRIPTOR      **PpiDescriptor OPTIONAL,
  IN OUT   VOID                        **Ppi
  );

/**
  This function installs a notification service to be called back when a
  given interface is installed or reinstalled.  The purpose of the service
  is to publish an interface that other parties can use to call additional PPIs
  that may materialize later.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation
  @param  NotifyList       A pointer to the list of notification interfaces that the caller shall install.

  @retval EFI_SUCCESS           The interface was successfully installed.
  @retval EFI_INVALID_PARAMETER The PpiList pointer is NULL, or any of the PEI PPI descriptors in the
                                list do not have the EFI_PEI_PPI_DESCRIPTOR_NOTIFY_TYPES bit set in the Flags field.
  @retval EFI_OUT_OF_RESOURCES  There is no additional space in the PPI database.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_NOTIFY_PPI)(
  IN CONST EFI_PEI_SERVICES                **PeiServices,
  IN CONST EFI_PEI_NOTIFY_DESCRIPTOR       *NotifyList
  );

/**
  This function returns the present value of the boot mode.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param  BootMode         A pointer to contain the value of the boot mode.

  @retval EFI_SUCCESS           The boot mode returned successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_GET_BOOT_MODE)(
  IN CONST EFI_PEI_SERVICES            **PeiServices,
  OUT EFI_BOOT_MODE                    *BootMode
  );

/**
  This function sets the value of the boot mode.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation
  @param  BootMode         The value of the boot mode to set.

  @retval EFI_SUCCESS           The boot mode returned successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_SET_BOOT_MODE)(
  IN CONST EFI_PEI_SERVICES            **PeiServices,
  IN EFI_BOOT_MODE                     BootMode
  );

/**
  This function returns the pointer to the list of Hand-Off Blocks (HOBs) in memory.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation
  @param  HobList          A pointer to the list of HOBs that the PEI Foundation will initialize

  @retval EFI_SUCCESS           The list was successfully returned.
  @retval EFI_NOT_AVAILABLE_YET The HOB list is not yet published.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_GET_HOB_LIST)(
  IN CONST EFI_PEI_SERVICES        **PeiServices,
  OUT VOID                         **HobList
  );

/**
  This service, published by the PEI Foundation, abstracts the creation of a Hand-Off Block's (HOB's) headers.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param  Type             The type of HOB to be installed.
  @param  Length           The length of the HOB to be added.
  @param  Hob              The address of a pointer that will contain the HOB header.

  @retval EFI_SUCCESS           The HOB was successfully created.
  @retval EFI_OUT_OF_RESOURCES  There is no additional space for HOB creation.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_CREATE_HOB)(
  IN CONST EFI_PEI_SERVICES            **PeiServices,
  IN UINT16                            Type,
  IN UINT16                            Length,
  IN OUT VOID                          **Hob
  );

/**
  The purpose of the service is to abstract the capability of the PEI
  Foundation to discover instances of firmware volumes in the system.

  This service enables PEIMs to discover additional firmware volumes. The PEI Foundation uses this
  service to abstract the locations and formats of various firmware volumes. These volumes include
  the Boot Firmware Volume and any other volumes exposed by EFI_PEI_FV_PPI. The service
  returns a volume handle of type EFI_PEI_FV_HANDLE, which must be unique within the system.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param  Instance         This instance of the firmware volume to find.
                           The value 0 is the Boot Firmware Volume (BFV).
  @param  VolumeHandle     On exit, points to the next volumn handle or NULL if it does not exist.

  @retval EFI_SUCCESS           The volume was found.
  @retval EFI_NOT_FOUND         The volume was not found.
  @retval EFI_INVALID_PARAMETER VolumeHandle is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FFS_FIND_NEXT_VOLUME2)(
  IN CONST EFI_PEI_SERVICES                **PeiServices,
  IN UINTN                                 Instance,
  OUT EFI_PEI_FV_HANDLE                    *VolumeHandle
  );

/**
  Searches for the next matching file in the firmware volume.

  This service enables PEIMs to discover firmware files within a specified volume.
  To find the first instance of a firmware file, pass a FileHandle value of NULL into the service.
  The service returns a file handle of type EFI_PEI_FILE_HANDLE, which must be unique within
  the system.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param  SearchType       A filter to find files only of this type.
  @param  FvHandle         Handle of firmware volume in which to search.
  @param  FileHandle       On entry, points to the current handle from which to begin searching
                           or NULL to start at the beginning of the firmware volume.
                           On exit, points the file handle of the next file in the volume or NULL
                           if there are no more files.

  @retval EFI_SUCCESS      The file was found.
  @retval EFI_NOT_FOUND    The file was not found.
  @retval EFI_NOT_FOUND    The header checksum was not zero.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FFS_FIND_NEXT_FILE2)(
  IN CONST EFI_PEI_SERVICES                **PeiServices,
  IN EFI_FV_FILETYPE                       SearchType,
  IN CONST EFI_PEI_FV_HANDLE               FvHandle,
  IN OUT EFI_PEI_FILE_HANDLE               *FileHandle
  );

/**
  Searches for the next matching section within the specified file.

  This service enables PEI modules to discover the first section of a given type within a valid file.
  This service will search within encapsulation sections (compression and GUIDed) as well. It will
  search inside of a GUIDed section or a compressed section, but may not, for example, search a
  GUIDed section inside a GUIDes section.
  This service will not search within compression sections or GUIDed sections that require
  extraction if memory is not present.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param  SectionType      The value of the section type to find.
  @param  FileHandle       Handle of the firmware file to search.
  @param  SectionData      A pointer to the discovered section, if successful.

  @retval EFI_SUCCESS      The section was found.
  @retval EFI_NOT_FOUND    The section was not found.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FFS_FIND_SECTION_DATA2)(
  IN CONST EFI_PEI_SERVICES            **PeiServices,
  IN EFI_SECTION_TYPE                  SectionType,
  IN EFI_PEI_FILE_HANDLE               FileHandle,
  OUT VOID                             **SectionData
  );

/**
  Searches for the next matching section within the specified file.

  This service enables PEI modules to discover the section of a given type within a valid file.
  This service will search within encapsulation sections (compression and GUIDed) as well. It will
  search inside of a GUIDed section or a compressed section, but may not, for example, search a
  GUIDed section inside a GUIDes section.
  This service will not search within compression sections or GUIDed sections that require
  extraction if memory is not present.

  @param  PeiServices           An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param  SectionType           The value of the section type to find.
  @param  SectionInstance       Section instance to find.
  @param  FileHandle            Handle of the firmware file to search.
  @param  SectionData           A pointer to the discovered section, if successful.
  @param  AuthenticationStatus  A pointer to the authentication status for this section.

  @retval EFI_SUCCESS      The section was found.
  @retval EFI_NOT_FOUND    The section was not found.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FFS_FIND_SECTION_DATA3)(
  IN CONST EFI_PEI_SERVICES            **PeiServices,
  IN EFI_SECTION_TYPE                  SectionType,
  IN UINTN                             SectionInstance,
  IN EFI_PEI_FILE_HANDLE               FileHandle,
  OUT VOID                             **SectionData,
  OUT UINT32                           *AuthenticationStatus
  );

/**
  This function registers the found memory configuration with the PEI Foundation.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param  MemoryBegin      The value of a region of installed memory.
  @param  MemoryLength     The corresponding length of a region of installed memory.

  @retval EFI_SUCCESS           The region was successfully installed in a HOB.
  @retval EFI_INVALID_PARAMETER MemoryBegin and MemoryLength are illegal for this system.
  @retval EFI_OUT_OF_RESOURCES  There is no additional space for HOB creation.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_INSTALL_PEI_MEMORY)(
  IN CONST EFI_PEI_SERVICES     **PeiServices,
  IN EFI_PHYSICAL_ADDRESS       MemoryBegin,
  IN UINT64                     MemoryLength
  );

/**
  The purpose of the service is to publish an interface that allows
  PEIMs to allocate memory ranges that are managed by the PEI Foundation.

  Prior to InstallPeiMemory() being called, PEI will allocate pages from the heap.
  After InstallPeiMemory() is called, PEI will allocate pages within the region
  of memory provided by InstallPeiMemory() service in a best-effort fashion.
  Location-specific allocations are not managed by the PEI foundation code.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param  MemoryType       The type of memory to allocate.
  @param  Pages            The number of contiguous 4 KB pages to allocate.
  @param  Memory           A pointer to a physical address. On output, the address is set to the base
                           of the page range that was allocated.

  @retval EFI_SUCCESS           The memory range was successfully allocated.
  @retval EFI_OUT_OF_RESOURCES  The pages could not be allocated.
  @retval EFI_INVALID_PARAMETER The type is not equal to EfiLoaderCode, EfiLoaderData, EfiRuntimeServicesCode,
                                EfiRuntimeServicesData, EfiBootServicesCode, EfiBootServicesData,
                                EfiACPIReclaimMemory, EfiReservedMemoryType, or EfiACPIMemoryNVS.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_ALLOCATE_PAGES)(
  IN CONST EFI_PEI_SERVICES     **PeiServices,
  IN EFI_MEMORY_TYPE            MemoryType,
  IN UINTN                      Pages,
  OUT EFI_PHYSICAL_ADDRESS      *Memory
  );

/**
  Frees memory pages.

  @param[in] PeiServices        An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param[in] Memory             The base physical address of the pages to be freed.
  @param[in] Pages              The number of contiguous 4 KB pages to free.

  @retval EFI_SUCCESS           The requested pages were freed.
  @retval EFI_INVALID_PARAMETER Memory is not a page-aligned address or Pages is invalid.
  @retval EFI_NOT_FOUND         The requested memory pages were not allocated with
                                AllocatePages().

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FREE_PAGES) (
  IN CONST EFI_PEI_SERVICES     **PeiServices,
  IN EFI_PHYSICAL_ADDRESS       Memory,
  IN UINTN                      Pages
  );

/**
  The purpose of this service is to publish an interface that
  allows PEIMs to allocate memory ranges that are managed by the PEI Foundation.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param  Size             The number of bytes to allocate from the pool.
  @param  Buffer           If the call succeeds, a pointer to a pointer to the allocated buffer; undefined otherwise.

  @retval EFI_SUCCESS           The allocation was successful.
  @retval EFI_OUT_OF_RESOURCES  There is not enough heap to allocate the requested size.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_ALLOCATE_POOL)(
  IN CONST EFI_PEI_SERVICES     **PeiServices,
  IN UINTN                      Size,
  OUT VOID                      **Buffer
  );

/**
  This service copies the contents of one buffer to another buffer.

  @param  Destination      The pointer to the destination buffer of the memory copy.
  @param  Source           The pointer to the source buffer of the memory copy.
  @param  Length           The number of bytes to copy from Source to Destination.

**/
typedef
VOID
(EFIAPI *EFI_PEI_COPY_MEM)(
  IN VOID                       *Destination,
  IN VOID                       *Source,
  IN UINTN                      Length
  );

/**
  The service fills a buffer with a specified value.

  @param  Buffer           The pointer to the buffer to fill.
  @param  Size             The number of bytes in Buffer to fill.
  @param  Value            The value to fill Buffer with.

**/
typedef
VOID
(EFIAPI *EFI_PEI_SET_MEM)(
  IN VOID                       *Buffer,
  IN UINTN                      Size,
  IN UINT8                      Value
  );

/**
  This service publishes an interface that allows PEIMs to report status codes.

  ReportStatusCode() is called by PEIMs that wish to report status information on their
  progress. The principal use model is for a PEIM to emit one of the standard 32-bit error codes. This
  will allow a platform owner to ascertain the state of the system, especially under conditions where
  the full consoles might not have been installed.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES table published by the PEI Foundation.
  @param  Type             Indicates the type of status code being reported.
  @param  Value            Describes the current status of a hardware or
                           software entity. This includes information about the class and
                           subclass that is used to classify the entity as well as an operation.
                           For progress codes, the operation is the current activity.
                           For error codes, it is the exception.For debug codes,it is not defined at this time.
  @param  Instance         The enumeration of a hardware or software entity within
                           the system. A system may contain multiple entities that match a class/subclass
                           pairing. The instance differentiates between them. An instance of 0 indicates
                           that instance information is unavailable, not meaningful, or not relevant.
                           Valid instance numbers start with 1.
  @param  CallerId         This optional parameter may be used to identify the caller.
                           This parameter allows the status code driver to apply different rules to
                           different callers.
  @param  Data             This optional parameter may be used to pass additional data.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_NOT_AVAILABLE_YET No progress code provider has installed an interface in the system.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_REPORT_STATUS_CODE)(
  IN CONST EFI_PEI_SERVICES         **PeiServices,
  IN EFI_STATUS_CODE_TYPE           Type,
  IN EFI_STATUS_CODE_VALUE          Value,
  IN UINT32                         Instance,
  IN CONST EFI_GUID                 *CallerId OPTIONAL,
  IN CONST EFI_STATUS_CODE_DATA     *Data OPTIONAL
  );

/**
  Resets the entire platform.

  This service resets the entire platform, including all processors
  and devices, and reboots the system.
  This service will never return EFI_SUCCESS.

  @param  PeiServices      An indirect pointer to the EFI_PEI_SERVICES
                           table published by the PEI Foundation.

  @retval EFI_NOT_AVAILABLE_YET The service has not been installed yet.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_RESET_SYSTEM)(
  IN CONST EFI_PEI_SERVICES   **PeiServices
  );

/**
  Resets the entire platform.

  @param[in] ResetType      The type of reset to perform.
  @param[in] ResetStatus    The status code for the reset.
  @param[in] DataSize       The size, in bytes, of ResetData.
  @param[in] ResetData      For a ResetType of EfiResetCold, EfiResetWarm, or EfiResetShutdown
                            the data buffer starts with a Null-terminated string, optionally
                            followed by additional binary data. The string is a description
                            that the caller may use to further indicate the reason for the
                            system reset.

**/
typedef
VOID
(EFIAPI *EFI_PEI_RESET2_SYSTEM) (
  IN EFI_RESET_TYPE     ResetType,
  IN EFI_STATUS         ResetStatus,
  IN UINTN              DataSize,
  IN VOID               *ResetData OPTIONAL
  );

/**
  Find a file within a volume by its name.

  This service searches for files with a specific name, within
  either the specified firmware volume or all firmware volumes.
  The service returns a file handle of type EFI_PEI_FILE_HANDLE,
  which must be unique within the system.

  @param FileName       A pointer to the name of the file to
                        find within the firmware volume.
  @param VolumeHandle   The firmware volume to search.
  @param FileHandle     Upon exit, points to the found file's
                        handle or NULL if it could not be found.

  @retval EFI_SUCCESS             The file was found.
  @retval EFI_NOT_FOUND           The file was not found.
  @retval EFI_INVALID_PARAMETER   VolumeHandle or FileHandle or
                                  FileName was NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FFS_FIND_BY_NAME)(
  IN  CONST  EFI_GUID            *FileName,
  IN  EFI_PEI_FV_HANDLE          VolumeHandle,
  OUT EFI_PEI_FILE_HANDLE        *FileHandle
  );

///
/// The information of the FV file.
///
typedef struct {
  ///
  /// Name of the file.
  ///
  EFI_GUID                FileName;
  ///
  /// File type.
  ///
  EFI_FV_FILETYPE         FileType;
  ///
  /// Attributes of the file.
  ///
  EFI_FV_FILE_ATTRIBUTES  FileAttributes;
  ///
  /// Points to the file's data (not the header).
  /// Not valid if EFI_FV_FILE_ATTRIB_MEMORY_MAPPED
  /// is zero.
  ///
  VOID                    *Buffer;
  ///
  /// Size of the file's data.
  ///
  UINT32                  BufferSize;
} EFI_FV_FILE_INFO;

///
/// The information with authentication status of the FV file.
///
typedef struct {
  ///
  /// Name of the file.
  ///
  EFI_GUID                FileName;
  ///
  /// File type.
  ///
  EFI_FV_FILETYPE         FileType;
  ///
  /// Attributes of the file.
  ///
  EFI_FV_FILE_ATTRIBUTES  FileAttributes;
  ///
  /// Points to the file's data (not the header).
  /// Not valid if EFI_FV_FILE_ATTRIB_MEMORY_MAPPED
  /// is zero.
  ///
  VOID                    *Buffer;
  ///
  /// Size of the file's data.
  ///
  UINT32                  BufferSize;
  ///
  /// Authentication status for this file.
  ///
  UINT32                  AuthenticationStatus;
} EFI_FV_FILE_INFO2;

/**
  Returns information about a specific file.

  This function returns information about a specific file,
  including its file name, type, attributes, starting address and
  size. If the firmware volume is not memory mapped, then the
  Buffer member will be NULL.

  @param FileHandle   The handle of the file.
  @param FileInfo     Upon exit, points to the file's
                      information.

  @retval EFI_SUCCESS             File information was returned.
  @retval EFI_INVALID_PARAMETER   FileHandle does not
                                  represent a valid file.
  @retval EFI_INVALID_PARAMETER   FileInfo is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FFS_GET_FILE_INFO)(
  IN  EFI_PEI_FILE_HANDLE         FileHandle,
  OUT EFI_FV_FILE_INFO            *FileInfo
  );

/**
  Returns information about a specific file.

  This function returns information about a specific file,
  including its file name, type, attributes, starting address, size and authentication status.
  If the firmware volume is not memory mapped, then the Buffer member will be NULL.

  @param FileHandle   The handle of the file.
  @param FileInfo     Upon exit, points to the file's
                      information.

  @retval EFI_SUCCESS             File information was returned.
  @retval EFI_INVALID_PARAMETER   FileHandle does not
                                  represent a valid file.
  @retval EFI_INVALID_PARAMETER   FileInfo is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FFS_GET_FILE_INFO2)(
  IN  EFI_PEI_FILE_HANDLE         FileHandle,
  OUT EFI_FV_FILE_INFO2           *FileInfo
  );

///
/// The information of the FV volume.
///
typedef struct {
  ///
  /// Attributes of the firmware volume.
  ///
  EFI_FVB_ATTRIBUTES_2  FvAttributes;
  ///
  /// Format of the firmware volume.
  ///
  EFI_GUID              FvFormat;
  ///
  /// Name of the firmware volume.
  ///
  EFI_GUID              FvName;
  ///
  /// Points to the first byte of the firmware
  /// volume, if bit EFI_FVB_MEMORY_MAPPED is
  /// set in FvAttributes.
  ///
  VOID                  *FvStart;
  ///
  /// Size of the firmware volume.
  ///
  UINT64                FvSize;
} EFI_FV_INFO;

/**
  Returns information about the specified volume.

  This function returns information about a specific firmware
  volume, including its name, type, attributes, starting address
  and size.

  @param VolumeHandle   Handle of the volume.
  @param VolumeInfo     Upon exit, points to the volume's information.

  @retval EFI_SUCCESS             The volume information returned.
  @retval EFI_INVALID_PARAMETER   If VolumeHandle does not represent a valid volume.
  @retval EFI_INVALID_PARAMETER   If VolumeHandle is NULL.
  @retval EFI_SUCCESS             Information was successfully returned.
  @retval EFI_INVALID_PARAMETER   The volume designated by the VolumeHandle is not available.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FFS_GET_VOLUME_INFO)(
  IN  EFI_PEI_FV_HANDLE       VolumeHandle,
  OUT EFI_FV_INFO             *VolumeInfo
  );

/**
  Register a PEIM so that it will be shadowed and called again.

  This service registers a file handle so that after memory is
  available, the PEIM will be re-loaded into permanent memory and
  re-initialized. The PEIM registered this way will always be
  initialized twice. The first time, this function call will
  return EFI_SUCCESS. The second time, this function call will
  return EFI_ALREADY_STARTED. Depending on the order in which
  PEIMs are dispatched, the PEIM making this call may be
  initialized after permanent memory is installed, even the first
  time.

  @param  FileHandle            PEIM's file handle. Must be the currently
                                executing PEIM.

  @retval EFI_SUCCESS           The PEIM was successfully registered for
                                shadowing.
  @retval EFI_ALREADY_STARTED   The PEIM was previously
                                registered for shadowing.
  @retval EFI_NOT_FOUND         The FileHandle does not refer to a
                                valid file handle.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_REGISTER_FOR_SHADOW)(
  IN  EFI_PEI_FILE_HANDLE FileHandle
  );


//
// PEI Specification Revision information
//
#define PEI_SPECIFICATION_MAJOR_REVISION  1
#define PEI_SPECIFICATION_MINOR_REVISION  70
///
/// Specification inconsistency here:
/// In the PI1.0 spec, PEI_SERVICES_SIGNATURE is defined as 0x5652455320494550. But
/// to pass a multiple tool chain, it appends an ULL.
///
//
// PEI Services Table
//
#define PEI_SERVICES_SIGNATURE  0x5652455320494550ULL
///
/// Specification inconsistency here:
/// In the PI1.0 specification, there is a typo error in PEI_SERVICES_REVISION. In the specification the defintion is
/// #define ((PEI_SPECIFICATION_MAJOR_REVISION<<16) |(PEI_SPECIFICATION_MINOR_REVISION))
/// and it should be as follows:
///
#define PEI_SERVICES_REVISION   ((PEI_SPECIFICATION_MAJOR_REVISION<<16) | (PEI_SPECIFICATION_MINOR_REVISION))

///
/// EFI_PEI_SERVICES is a collection of functions whose implementation is provided by the PEI
/// Foundation. These services fall into various classes, including the following:
/// - Managing the boot mode
/// - Allocating both early and permanent memory
/// - Supporting the Firmware File System (FFS)
/// - Abstracting the PPI database abstraction
/// - Creating Hand-Off Blocks (HOBs).
///
struct _EFI_PEI_SERVICES {
  ///
  /// The table header for the PEI Services Table.
  ///
  EFI_TABLE_HEADER                Hdr;

  //
  // PPI Functions
  //
  EFI_PEI_INSTALL_PPI             InstallPpi;
  EFI_PEI_REINSTALL_PPI           ReInstallPpi;
  EFI_PEI_LOCATE_PPI              LocatePpi;
  EFI_PEI_NOTIFY_PPI              NotifyPpi;

  //
  // Boot Mode Functions
  //
  EFI_PEI_GET_BOOT_MODE           GetBootMode;
  EFI_PEI_SET_BOOT_MODE           SetBootMode;

  //
  // HOB Functions
  //
  EFI_PEI_GET_HOB_LIST            GetHobList;
  EFI_PEI_CREATE_HOB              CreateHob;

  //
  // Firmware Volume Functions
  //
  EFI_PEI_FFS_FIND_NEXT_VOLUME2   FfsFindNextVolume;
  EFI_PEI_FFS_FIND_NEXT_FILE2     FfsFindNextFile;
  EFI_PEI_FFS_FIND_SECTION_DATA2  FfsFindSectionData;

  //
  // PEI Memory Functions
  //
  EFI_PEI_INSTALL_PEI_MEMORY      InstallPeiMemory;
  EFI_PEI_ALLOCATE_PAGES          AllocatePages;
  EFI_PEI_ALLOCATE_POOL           AllocatePool;
  EFI_PEI_COPY_MEM                CopyMem;
  EFI_PEI_SET_MEM                 SetMem;

  //
  // Status Code
  //
  EFI_PEI_REPORT_STATUS_CODE      ReportStatusCode;

  //
  // Reset
  //
  EFI_PEI_RESET_SYSTEM            ResetSystem;

  //
  // (the following interfaces are installed by publishing PEIM)
  // I/O Abstractions
  //
  EFI_PEI_CPU_IO_PPI              *CpuIo;
  EFI_PEI_PCI_CFG2_PPI            *PciCfg;

  //
  // Future Installed Services
  //
  EFI_PEI_FFS_FIND_BY_NAME        FfsFindFileByName;
  EFI_PEI_FFS_GET_FILE_INFO       FfsGetFileInfo;
  EFI_PEI_FFS_GET_VOLUME_INFO     FfsGetVolumeInfo;
  EFI_PEI_REGISTER_FOR_SHADOW     RegisterForShadow;
  EFI_PEI_FFS_FIND_SECTION_DATA3  FindSectionData3;
  EFI_PEI_FFS_GET_FILE_INFO2      FfsGetFileInfo2;
  EFI_PEI_RESET2_SYSTEM           ResetSystem2;
  EFI_PEI_FREE_PAGES              FreePages;
};


///
/// EFI_SEC_PEI_HAND_OFF structure holds information about
/// PEI core's operating environment, such as the size of location of
/// temporary RAM, the stack location and BFV location.
///
typedef struct _EFI_SEC_PEI_HAND_OFF {
  ///
  /// Size of the data structure.
  ///
  UINT16  DataSize;

  ///
  /// Points to the first byte of the boot firmware volume,
  /// which the PEI Dispatcher should search for
  /// PEI modules.
  ///
  VOID    *BootFirmwareVolumeBase;

  ///
  /// Size of the boot firmware volume, in bytes.
  ///
  UINTN   BootFirmwareVolumeSize;

  ///
  /// Points to the first byte of the temporary RAM.
  ///
  VOID    *TemporaryRamBase;

  ///
  /// Size of the temporary RAM, in bytes.
  ///
  UINTN   TemporaryRamSize;

  ///
  /// Points to the first byte of the temporary RAM
  /// available for use by the PEI Foundation. The area
  /// described by PeiTemporaryRamBase and PeiTemporaryRamSize
  /// must not extend outside beyond the area described by
  /// TemporaryRamBase & TemporaryRamSize. This area should not
  /// overlap with the area reported by StackBase and
  /// StackSize.
  ///
  VOID    *PeiTemporaryRamBase;

  ///
  /// The size of the available temporary RAM available for
  /// use by the PEI Foundation, in bytes.
  ///
  UINTN   PeiTemporaryRamSize;

  ///
  /// Points to the first byte of the stack.
  /// This are may be part of the memory described by
  /// TemporaryRamBase and TemporaryRamSize
  /// or may be an entirely separate area.
  ///
  VOID    *StackBase;

  ///
  /// Size of the stack, in bytes.
  ///
  UINTN   StackSize;
} EFI_SEC_PEI_HAND_OFF;


/**
  The entry point of PEI Foundation.

  This function is the entry point for the PEI Foundation, which
  allows the SEC phase to pass information about the stack,
  temporary RAM and the Boot Firmware Volume. In addition, it also
  allows the SEC phase to pass services and data forward for use
  during the PEI phase in the form of one or more PPIs. These PPI's
  will be installed and/or immediately signaled if they are
  notification type. There is no limit to the number of additional
  PPIs that can be passed from SEC into the PEI Foundation. As part
  of its initialization phase, the PEI Foundation will add these
  SEC-hosted PPIs to its PPI database such that both the PEI
  Foundation and any modules can leverage the associated service
  calls and/or code in these early PPIs.

  @param SecCoreData    Points to a data structure containing
                        information about the PEI core's
                        operating environment, such as the size
                        and location of temporary RAM, the stack
                        location and the BFV location.

  @param PpiList        Points to a list of one or more PPI
                        descriptors to be installed initially by
                        the PEI core. An empty PPI list consists
                        of a single descriptor with the end-tag
                        EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST.
                        As part of its initialization phase, the
                        PEI Foundation will add these SEC-hosted
                        PPIs to its PPI database such that both
                        the PEI Foundation and any modules can
                        leverage the associated service calls
                        and/or code in these early PPIs.


**/
typedef
VOID
(EFIAPI *EFI_PEI_CORE_ENTRY_POINT)(
  IN CONST  EFI_SEC_PEI_HAND_OFF    *SecCoreData,
  IN CONST  EFI_PEI_PPI_DESCRIPTOR  *PpiList
);

#endif
