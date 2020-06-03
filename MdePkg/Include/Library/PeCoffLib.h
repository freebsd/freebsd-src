/** @file
  Provides services to load and relocate a PE/COFF image.

  The PE/COFF Loader Library abstracts the implementation of a PE/COFF loader for
  IA-32, x86, IPF, and EBC processor types. The library functions are memory-based
  and can be ported easily to any environment.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BASE_PE_COFF_LIB_H__
#define __BASE_PE_COFF_LIB_H__

#include <IndustryStandard/PeImage.h>
//
// Return status codes from the PE/COFF Loader services
//
#define IMAGE_ERROR_SUCCESS                      0
#define IMAGE_ERROR_IMAGE_READ                   1
#define IMAGE_ERROR_INVALID_PE_HEADER_SIGNATURE  2
#define IMAGE_ERROR_INVALID_MACHINE_TYPE         3
#define IMAGE_ERROR_INVALID_SUBSYSTEM            4
#define IMAGE_ERROR_INVALID_IMAGE_ADDRESS        5
#define IMAGE_ERROR_INVALID_IMAGE_SIZE           6
#define IMAGE_ERROR_INVALID_SECTION_ALIGNMENT    7
#define IMAGE_ERROR_SECTION_NOT_LOADED           8
#define IMAGE_ERROR_FAILED_RELOCATION            9
#define IMAGE_ERROR_FAILED_ICACHE_FLUSH          10
#define IMAGE_ERROR_UNSUPPORTED                  11

/**
  Reads contents of a PE/COFF image.

  A function of this type reads contents of the PE/COFF image specified by FileHandle. The read
  operation copies ReadSize bytes from the PE/COFF image starting at byte offset FileOffset into
  the buffer specified by Buffer. The size of the buffer actually read is returned in ReadSize.
  If FileOffset specifies an offset past the end of the PE/COFF image, a ReadSize of 0 is returned.
  A function of this type must be registered in the ImageRead field of a PE_COFF_LOADER_IMAGE_CONTEXT
  structure for the PE/COFF Loader Library service to function correctly.  This function abstracts access
  to a PE/COFF image so it can be implemented in an environment specific manner.  For example, SEC and PEI
  environments may access memory directly to read the contents of a PE/COFF image, and DXE or UEFI
  environments may require protocol services to read the contents of PE/COFF image
  stored on FLASH, disk, or network devices.

  If FileHandle is not a valid handle, then ASSERT().
  If ReadSize is NULL, then ASSERT().
  If Buffer is NULL, then ASSERT().

  @param  FileHandle      Pointer to the file handle to read the PE/COFF image.
  @param  FileOffset      Offset into the PE/COFF image to begin the read operation.
  @param  ReadSize        On input, the size in bytes of the requested read operation.
                          On output, the number of bytes actually read.
  @param  Buffer          Output buffer that contains the data read from the PE/COFF image.

  @retval RETURN_SUCCESS            The specified portion of the PE/COFF image was
                                    read and the size return in ReadSize.
  @retval RETURN_DEVICE_ERROR       The specified portion of the PE/COFF image
                                    could not be read due to a device error.

**/
typedef
RETURN_STATUS
(EFIAPI *PE_COFF_LOADER_READ_FILE)(
  IN     VOID   *FileHandle,
  IN     UINTN  FileOffset,
  IN OUT UINTN  *ReadSize,
  OUT    VOID   *Buffer
  );

///
/// The context structure used while PE/COFF image is being loaded and relocated.
///
typedef struct {
  ///
  /// Set by PeCoffLoaderGetImageInfo() to the ImageBase in the PE/COFF header.
  ///
  PHYSICAL_ADDRESS                  ImageAddress;
  ///
  /// Set by PeCoffLoaderGetImageInfo() to the SizeOfImage in the PE/COFF header.
  /// Image size includes the size of Debug Entry if it is present.
  ///
  UINT64                            ImageSize;
  ///
  /// Is set to zero by PeCoffLoaderGetImageInfo(). If DestinationAddress is non-zero,
  /// PeCoffLoaderRelocateImage() will relocate the image using this base address.
  /// If the DestinationAddress is zero, the ImageAddress will be used as the base
  /// address of relocation.
  ///
  PHYSICAL_ADDRESS                  DestinationAddress;
  ///
  /// PeCoffLoaderLoadImage() sets EntryPoint to to the entry point of the PE/COFF image.
  ///
  PHYSICAL_ADDRESS                  EntryPoint;
  ///
  /// Passed in by the caller to PeCoffLoaderGetImageInfo() and PeCoffLoaderLoadImage()
  /// to abstract accessing the image from the library.
  ///
  PE_COFF_LOADER_READ_FILE          ImageRead;
  ///
  /// Used as the FileHandle passed into the ImageRead function when it's called.
  ///
  VOID                              *Handle;
  ///
  /// Caller allocated buffer of size FixupDataSize that can be optionally allocated
  /// prior to calling PeCoffLoaderRelocateImage().
  /// This buffer is filled with the information used to fix up the image.
  /// The fixups have been applied to the image and this entry is just for information.
  ///
  VOID                              *FixupData;
  ///
  /// Set by PeCoffLoaderGetImageInfo() to the Section Alignment in the PE/COFF header.
  /// If the image is a TE image, then this field is set to 0.
  ///
  UINT32                            SectionAlignment;
  ///
  /// Set by PeCoffLoaderGetImageInfo() to offset to the PE/COFF header.
  /// If the PE/COFF image does not start with a DOS header, this value is zero.
  /// Otherwise, it's the offset to the PE/COFF header.
  ///
  UINT32                            PeCoffHeaderOffset;
  ///
  /// Set by PeCoffLoaderGetImageInfo() to the Relative Virtual Address of the debug directory,
  /// if it exists in the image
  ///
  UINT32                            DebugDirectoryEntryRva;
  ///
  /// Set by PeCoffLoaderLoadImage() to CodeView area of the PE/COFF Debug directory.
  ///
  VOID                              *CodeView;
  ///
  /// Set by PeCoffLoaderLoadImage() to point to the PDB entry contained in the CodeView area.
  /// The PdbPointer points to the filename of the PDB file used for source-level debug of
  /// the image by a debugger.
  ///
  CHAR8                             *PdbPointer;
  ///
  /// Is set by PeCoffLoaderGetImageInfo() to the Section Alignment in the PE/COFF header.
  ///
  UINTN                             SizeOfHeaders;
  ///
  /// Not used by this library class. Other library classes that layer on  top of this library
  /// class fill in this value as part of their GetImageInfo call.
  /// This allows the caller of the library to know what type of memory needs to be allocated
  /// to load and relocate the image.
  ///
  UINT32                            ImageCodeMemoryType;
  ///
  /// Not used by this library class. Other library classes that layer on top of this library
  /// class fill in this value as part of their GetImageInfo call.
  /// This allows the caller of the library to know what type of memory needs to be allocated
  /// to load and relocate the image.
  ///
  UINT32                            ImageDataMemoryType;
  ///
  /// Set by any of the library functions if they encounter an error.
  ///
  UINT32                            ImageError;
  ///
  /// Set by PeCoffLoaderLoadImage() to indicate the size of FixupData that the caller must
  /// allocate before calling PeCoffLoaderRelocateImage().
  ///
  UINTN                             FixupDataSize;
  ///
  /// Set by PeCoffLoaderGetImageInfo() to the machine type stored in the PE/COFF header.
  ///
  UINT16                            Machine;
  ///
  /// Set by PeCoffLoaderGetImageInfo() to the subsystem type stored in the PE/COFF header.
  ///
  UINT16                            ImageType;
  ///
  /// Set by PeCoffLoaderGetImageInfo() to TRUE if the PE/COFF image does not contain
  /// relocation information.
  ///
  BOOLEAN                           RelocationsStripped;
  ///
  /// Set by PeCoffLoaderGetImageInfo() to TRUE if the image is a TE image.
  /// For a definition of the TE Image format, see the Platform Initialization Pre-EFI
  /// Initialization Core Interface Specification.
  ///
  BOOLEAN                           IsTeImage;
  ///
  /// Set by PeCoffLoaderLoadImage() to the HII resource offset
  /// if the image contains a custom PE/COFF resource with the type 'HII'.
  /// Otherwise, the entry remains to be 0.
  ///
  PHYSICAL_ADDRESS                  HiiResourceData;
  ///
  /// Private storage for implementation specific data.
  ///
  UINT64                            Context;
} PE_COFF_LOADER_IMAGE_CONTEXT;

/**
  Retrieves information about a PE/COFF image.

  Computes the PeCoffHeaderOffset, IsTeImage, ImageType, ImageAddress, ImageSize,
  DestinationAddress, RelocationsStripped, SectionAlignment, SizeOfHeaders, and
  DebugDirectoryEntryRva fields of the ImageContext structure.
  If ImageContext is NULL, then return RETURN_INVALID_PARAMETER.
  If the PE/COFF image accessed through the ImageRead service in the ImageContext
  structure is not a supported PE/COFF image type, then return RETURN_UNSUPPORTED.
  If any errors occur while computing the fields of ImageContext,
  then the error status is returned in the ImageError field of ImageContext.
  If the image is a TE image, then SectionAlignment is set to 0.
  The ImageRead and Handle fields of ImageContext structure must be valid prior
  to invoking this service.

  @param  ImageContext              The pointer to the image context structure that
                                    describes the PE/COFF image that needs to be
                                    examined by this function.

  @retval RETURN_SUCCESS            The information on the PE/COFF image was collected.
  @retval RETURN_INVALID_PARAMETER  ImageContext is NULL.
  @retval RETURN_UNSUPPORTED        The PE/COFF image is not supported.

**/
RETURN_STATUS
EFIAPI
PeCoffLoaderGetImageInfo (
  IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *ImageContext
  );

/**
  Applies relocation fixups to a PE/COFF image that was loaded with PeCoffLoaderLoadImage().

  If the DestinationAddress field of ImageContext is 0, then use the ImageAddress field of
  ImageContext as the relocation base address.  Otherwise, use the DestinationAddress field
  of ImageContext as the relocation base address.  The caller must allocate the relocation
  fixup log buffer and fill in the FixupData field of ImageContext prior to calling this function.

  The ImageRead, Handle, PeCoffHeaderOffset, IsTeImage, Machine, ImageType, ImageAddress,
  ImageSize, DestinationAddress, RelocationsStripped, SectionAlignment, SizeOfHeaders,
  DebugDirectoryEntryRva, EntryPoint, FixupDataSize, CodeView, PdbPointer, and FixupData of
  the ImageContext structure must be valid prior to invoking this service.

  If ImageContext is NULL, then ASSERT().

  Note that if the platform does not maintain coherency between the instruction cache(s) and the data
  cache(s) in hardware, then the caller is responsible for performing cache maintenance operations
  prior to transferring control to a PE/COFF image that is loaded using this library.

  @param  ImageContext        The pointer to the image context structure that describes the PE/COFF
                              image that is being relocated.

  @retval RETURN_SUCCESS      The PE/COFF image was relocated.
                              Extended status information is in the ImageError field of ImageContext.
  @retval RETURN_LOAD_ERROR   The image in not a valid PE/COFF image.
                              Extended status information is in the ImageError field of ImageContext.
  @retval RETURN_UNSUPPORTED  A relocation record type is not supported.
                              Extended status information is in the ImageError field of ImageContext.

**/
RETURN_STATUS
EFIAPI
PeCoffLoaderRelocateImage (
  IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *ImageContext
  );

/**
  Loads a PE/COFF image into memory.

  Loads the PE/COFF image accessed through the ImageRead service of ImageContext into the buffer
  specified by the ImageAddress and ImageSize fields of ImageContext.  The caller must allocate
  the load buffer and fill in the ImageAddress and ImageSize fields prior to calling this function.
  The EntryPoint, FixupDataSize, CodeView, PdbPointer and HiiResourceData fields of ImageContext are computed.
  The ImageRead, Handle, PeCoffHeaderOffset, IsTeImage, Machine, ImageType, ImageAddress, ImageSize,
  DestinationAddress, RelocationsStripped, SectionAlignment, SizeOfHeaders, and DebugDirectoryEntryRva
  fields of the ImageContext structure must be valid prior to invoking this service.

  If ImageContext is NULL, then ASSERT().

  Note that if the platform does not maintain coherency between the instruction cache(s) and the data
  cache(s) in hardware, then the caller is responsible for performing cache maintenance operations
  prior to transferring control to a PE/COFF image that is loaded using this library.

  @param  ImageContext              The pointer to the image context structure that describes the PE/COFF
                                    image that is being loaded.

  @retval RETURN_SUCCESS            The PE/COFF image was loaded into the buffer specified by
                                    the ImageAddress and ImageSize fields of ImageContext.
                                    Extended status information is in the ImageError field of ImageContext.
  @retval RETURN_BUFFER_TOO_SMALL   The caller did not provide a large enough buffer.
                                    Extended status information is in the ImageError field of ImageContext.
  @retval RETURN_LOAD_ERROR         The PE/COFF image is an EFI Runtime image with no relocations.
                                    Extended status information is in the ImageError field of ImageContext.
  @retval RETURN_INVALID_PARAMETER  The image address is invalid.
                                    Extended status information is in the ImageError field of ImageContext.

**/
RETURN_STATUS
EFIAPI
PeCoffLoaderLoadImage (
  IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *ImageContext
  );


/**
  Reads contents of a PE/COFF image from a buffer in system memory.

  This is the default implementation of a PE_COFF_LOADER_READ_FILE function
  that assumes FileHandle pointer to the beginning of a PE/COFF image.
  This function reads contents of the PE/COFF image that starts at the system memory
  address specified by FileHandle. The read operation copies ReadSize bytes from the
  PE/COFF image starting at byte offset FileOffset into the buffer specified by Buffer.
  The size of the buffer actually read is returned in ReadSize.

  If FileHandle is NULL, then ASSERT().
  If ReadSize is NULL, then ASSERT().
  If Buffer is NULL, then ASSERT().

  @param  FileHandle        The pointer to base of the input stream
  @param  FileOffset        Offset into the PE/COFF image to begin the read operation.
  @param  ReadSize          On input, the size in bytes of the requested read operation.
                            On output, the number of bytes actually read.
  @param  Buffer            Output buffer that contains the data read from the PE/COFF image.

  @retval RETURN_SUCCESS    The data is read from FileOffset from the Handle into
                            the buffer.
**/
RETURN_STATUS
EFIAPI
PeCoffLoaderImageReadFromMemory (
  IN     VOID    *FileHandle,
  IN     UINTN   FileOffset,
  IN OUT UINTN   *ReadSize,
  OUT    VOID    *Buffer
  );


/**
  Reapply fixups on a fixed up PE32/PE32+ image to allow virtual calling at EFI
  runtime.

  This function reapplies relocation fixups to the PE/COFF image specified by ImageBase
  and ImageSize so the image will execute correctly when the PE/COFF image is mapped
  to the address specified by VirtualImageBase. RelocationData must be identical
  to the FiuxupData buffer from the PE_COFF_LOADER_IMAGE_CONTEXT structure
  after this PE/COFF image was relocated with PeCoffLoaderRelocateImage().

  Note that if the platform does not maintain coherency between the instruction cache(s) and the data
  cache(s) in hardware, then the caller is responsible for performing cache maintenance operations
  prior to transferring control to a PE/COFF image that is loaded using this library.

  @param  ImageBase          The base address of a PE/COFF image that has been loaded
                             and relocated into system memory.
  @param  VirtImageBase      The request virtual address that the PE/COFF image is to
                             be fixed up for.
  @param  ImageSize          The size, in bytes, of the PE/COFF image.
  @param  RelocationData     A pointer to the relocation data that was collected when the PE/COFF
                             image was relocated using PeCoffLoaderRelocateImage().

**/
VOID
EFIAPI
PeCoffLoaderRelocateImageForRuntime (
  IN  PHYSICAL_ADDRESS        ImageBase,
  IN  PHYSICAL_ADDRESS        VirtImageBase,
  IN  UINTN                   ImageSize,
  IN  VOID                    *RelocationData
  );

/**
  Unloads a loaded PE/COFF image from memory and releases its taken resource.
  Releases any environment specific resources that were allocated when the image
  specified by ImageContext was loaded using PeCoffLoaderLoadImage().

  For NT32 emulator, the PE/COFF image loaded by system needs to release.
  For real platform, the PE/COFF image loaded by Core doesn't needs to be unloaded,
  this function can simply return RETURN_SUCCESS.

  If ImageContext is NULL, then ASSERT().

  @param  ImageContext              Pointer to the image context structure that describes the PE/COFF
                                    image to be unloaded.

  @retval RETURN_SUCCESS            The PE/COFF image was unloaded successfully.
**/
RETURN_STATUS
EFIAPI
PeCoffLoaderUnloadImage (
  IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *ImageContext
  );
#endif
