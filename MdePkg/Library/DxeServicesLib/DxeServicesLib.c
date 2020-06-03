/** @file
  MDE DXE Services Library provides functions that simplify the development of DXE Drivers.
  These functions help access data from sections of FFS files or from file path.

  Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2015 Hewlett Packard Enterprise Development LP<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiLib.h>
#include <Library/DxeServicesLib.h>
#include <Protocol/FirmwareVolume2.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/LoadFile2.h>
#include <Protocol/LoadFile.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>

/**
  Identify the device handle from which the Image is loaded from. As this device handle is passed to
  GetSectionFromFv as the identifier for a Firmware Volume, an EFI_FIRMWARE_VOLUME2_PROTOCOL
  protocol instance should be located successfully by calling gBS->HandleProtocol ().

  This function locates the EFI_LOADED_IMAGE_PROTOCOL instance installed
  on ImageHandle. It then returns EFI_LOADED_IMAGE_PROTOCOL.DeviceHandle.

  If ImageHandle is NULL, then ASSERT ();
  If failed to locate a EFI_LOADED_IMAGE_PROTOCOL on ImageHandle, then ASSERT ();

  @param  ImageHandle         The firmware allocated handle for UEFI image.

  @retval  EFI_HANDLE         The device handle from which the Image is loaded from.

**/
EFI_HANDLE
InternalImageHandleToFvHandle (
  EFI_HANDLE ImageHandle
  )
{
  EFI_STATUS                    Status;
  EFI_LOADED_IMAGE_PROTOCOL     *LoadedImage;

  ASSERT (ImageHandle != NULL);

  Status = gBS->HandleProtocol (
             ImageHandle,
             &gEfiLoadedImageProtocolGuid,
             (VOID **) &LoadedImage
             );

  ASSERT_EFI_ERROR (Status);

  //
  // The LoadedImage->DeviceHandle may be NULL.
  // For example for DxeCore, there is LoadedImage protocol installed for it, but the
  // LoadedImage->DeviceHandle could not be initialized before the FV2 (contain DxeCore)
  // protocol is installed.
  //
  return LoadedImage->DeviceHandle;

}

/**
  Allocate and fill a buffer from a Firmware Section identified by a Firmware File GUID name, a Firmware
  Section type and instance number from the specified Firmware Volume.

  This functions first locate the EFI_FIRMWARE_VOLUME2_PROTOCOL protocol instance on FvHandle in order to
  carry out the Firmware Volume read operation. The function then reads the Firmware Section found specified
  by NameGuid, SectionType and SectionInstance.

  The details of this search order is defined in description of EFI_FIRMWARE_VOLUME2_PROTOCOL.ReadSection ()
  found in PI Specification.

  If SectionType is EFI_SECTION_TE, EFI_SECTION_TE is used as section type to start the search. If EFI_SECTION_TE section
  is not found, EFI_SECTION_PE32 will be used to try the search again. If no EFI_SECTION_PE32 section is found, EFI_NOT_FOUND
  is returned.

  The data and size is returned by Buffer and Size. The caller is responsible to free the Buffer allocated
  by this function. This function can be only called at TPL_NOTIFY and below.

  If NameGuid is NULL, then ASSERT();
  If Buffer is NULL, then ASSERT();
  If Size is NULL, then ASSERT().

  @param  FvHandle                The device handle that contains a instance of
                                  EFI_FIRMWARE_VOLUME2_PROTOCOL instance.
  @param  NameGuid                The GUID name of a Firmware File.
  @param  SectionType             The Firmware Section type.
  @param  SectionInstance         The instance number of Firmware Section to
                                  read from starting from 0.
  @param  Buffer                  On output, Buffer contains the data read
                                  from the section in the Firmware File found.
  @param  Size                    On output, the size of Buffer.

  @retval  EFI_SUCCESS            The image is found and data and size is returned.
  @retval  EFI_NOT_FOUND          The image specified by NameGuid and SectionType
                                  can't be found.
  @retval  EFI_OUT_OF_RESOURCES   There were not enough resources to allocate the
                                  output data buffer or complete the operations.
  @retval  EFI_DEVICE_ERROR       A hardware error occurs during reading from the
                                  Firmware Volume.
  @retval  EFI_ACCESS_DENIED      The firmware volume containing the searched
                                  Firmware File is configured to disallow reads.

**/
EFI_STATUS
InternalGetSectionFromFv (
  IN  EFI_HANDLE                    FvHandle,
  IN  CONST EFI_GUID                *NameGuid,
  IN  EFI_SECTION_TYPE              SectionType,
  IN  UINTN                         SectionInstance,
  OUT VOID                          **Buffer,
  OUT UINTN                         *Size
  )
{
  EFI_STATUS                    Status;
  EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv;
  UINT32                        AuthenticationStatus;

  ASSERT (NameGuid != NULL);
  ASSERT (Buffer != NULL);
  ASSERT (Size != NULL);

  if (FvHandle == NULL) {
    //
    // Return EFI_NOT_FOUND directly for NULL FvHandle.
    //
    return EFI_NOT_FOUND;
  }

  Status = gBS->HandleProtocol (
                  FvHandle,
                  &gEfiFirmwareVolume2ProtocolGuid,
                  (VOID **) &Fv
                  );
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  //
  // Read desired section content in NameGuid file
  //
  *Buffer     = NULL;
  *Size       = 0;
  Status      = Fv->ReadSection (
                      Fv,
                      NameGuid,
                      SectionType,
                      SectionInstance,
                      Buffer,
                      Size,
                      &AuthenticationStatus
                      );

  if (EFI_ERROR (Status) && (SectionType == EFI_SECTION_TE)) {
    //
    // Try reading PE32 section, if the required section is TE type
    //
    *Buffer = NULL;
    *Size   = 0;
    Status  = Fv->ReadSection (
                    Fv,
                    NameGuid,
                    EFI_SECTION_PE32,
                    SectionInstance,
                    Buffer,
                    Size,
                    &AuthenticationStatus
                    );
  }

  return Status;
}

/**
  Searches all the available firmware volumes and returns the first matching FFS section.

  This function searches all the firmware volumes for FFS files with FV file type specified by FileType
  The order that the firmware volumes is searched is not deterministic. For each available FV a search
  is made for FFS file of type FileType. If the FV contains more than one FFS file with the same FileType,
  the FileInstance instance will be the matched FFS file. For each FFS file found a search
  is made for FFS sections of type SectionType. If the FFS file contains at least SectionInstance instances
  of the FFS section specified by SectionType, then the SectionInstance instance is returned in Buffer.
  Buffer is allocated using AllocatePool(), and the size of the allocated buffer is returned in Size.
  It is the caller's responsibility to use FreePool() to free the allocated buffer.
  See EFI_FIRMWARE_VOLUME2_PROTOCOL.ReadSection() for details on how sections
  are retrieved from an FFS file based on SectionType and SectionInstance.

  If SectionType is EFI_SECTION_TE, and the search with an FFS file fails,
  the search will be retried with a section type of EFI_SECTION_PE32.
  This function must be called with a TPL <= TPL_NOTIFY.

  If Buffer is NULL, then ASSERT().
  If Size is NULL, then ASSERT().

  @param  FileType             Indicates the FV file type to search for within all
                               available FVs.
  @param  FileInstance         Indicates which file instance within all available
                               FVs specified by FileType.
                               FileInstance starts from zero.
  @param  SectionType          Indicates the FFS section type to search for
                               within the FFS file
                               specified by FileType with FileInstance.
  @param  SectionInstance      Indicates which section instance within the FFS file
                               specified by FileType with FileInstance to retrieve.
                               SectionInstance starts from zero.
  @param  Buffer               On output, a pointer to a callee allocated buffer
                               containing the FFS file section that was found.
                               Is it the caller's responsibility to free this
                               buffer using FreePool().
  @param  Size                 On output, a pointer to the size, in bytes, of Buffer.

  @retval  EFI_SUCCESS          The specified FFS section was returned.
  @retval  EFI_NOT_FOUND        The specified FFS section could not be found.
  @retval  EFI_OUT_OF_RESOURCES There are not enough resources available to retrieve
                                the matching FFS section.
  @retval  EFI_DEVICE_ERROR     The FFS section could not be retrieves due to a
                                device error.
  @retval  EFI_ACCESS_DENIED    The FFS section could not be retrieves because
                                the firmware volume that
                                contains the matching FFS section does not allow reads.
**/
EFI_STATUS
EFIAPI
GetSectionFromAnyFvByFileType  (
  IN  EFI_FV_FILETYPE               FileType,
  IN  UINTN                         FileInstance,
  IN  EFI_SECTION_TYPE              SectionType,
  IN  UINTN                         SectionInstance,
  OUT VOID                          **Buffer,
  OUT UINTN                         *Size
  )
{
  EFI_STATUS                    Status;
  EFI_HANDLE                    *HandleBuffer;
  UINTN                         HandleCount;
  UINTN                         IndexFv;
  UINTN                         IndexFile;
  UINTN                         Key;
  EFI_GUID                      NameGuid;
  EFI_FV_FILE_ATTRIBUTES        Attributes;
  EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv;

  ASSERT (Buffer != NULL);
  ASSERT (Size != NULL);

  //
  // Locate all available FVs.
  //
  HandleBuffer = NULL;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiFirmwareVolume2ProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Go through FVs one by one to find the required section data.
  //
  for (IndexFv = 0; IndexFv < HandleCount; IndexFv++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[IndexFv],
                    &gEfiFirmwareVolume2ProtocolGuid,
                    (VOID **)&Fv
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    //
    // Use Firmware Volume 2 Protocol to search for a file of type FileType in all FVs.
    //
    IndexFile = FileInstance + 1;
    Key = 0;
    do {
      Status = Fv->GetNextFile (Fv, &Key, &FileType, &NameGuid, &Attributes, Size);
      if (EFI_ERROR (Status)) {
        break;
      }
      IndexFile --;
    } while (IndexFile > 0);

    //
    // Fv File with the required FV file type is found.
    // Search the section file in the found FV file.
    //
    if (IndexFile == 0) {
      Status = InternalGetSectionFromFv (
                 HandleBuffer[IndexFv],
                 &NameGuid,
                 SectionType,
                 SectionInstance,
                 Buffer,
                 Size
                 );

      if (!EFI_ERROR (Status)) {
        goto Done;
      }
    }
  }

  //
  // The required FFS section file is not found.
  //
  if (IndexFv == HandleCount) {
    Status = EFI_NOT_FOUND;
  }

Done:
  if (HandleBuffer != NULL) {
    FreePool(HandleBuffer);
  }

  return Status;
}

/**
  Searches all the availables firmware volumes and returns the first matching FFS section.

  This function searches all the firmware volumes for FFS files with an FFS filename specified by NameGuid.
  The order that the firmware volumes is searched is not deterministic. For each FFS file found a search
  is made for FFS sections of type SectionType. If the FFS file contains at least SectionInstance instances
  of the FFS section specified by SectionType, then the SectionInstance instance is returned in Buffer.
  Buffer is allocated using AllocatePool(), and the size of the allocated buffer is returned in Size.
  It is the caller's responsibility to use FreePool() to free the allocated buffer.
  See EFI_FIRMWARE_VOLUME2_PROTOCOL.ReadSection() for details on how sections
  are retrieved from an FFS file based on SectionType and SectionInstance.

  If SectionType is EFI_SECTION_TE, and the search with an FFS file fails,
  the search will be retried with a section type of EFI_SECTION_PE32.
  This function must be called with a TPL <= TPL_NOTIFY.

  If NameGuid is NULL, then ASSERT().
  If Buffer is NULL, then ASSERT().
  If Size is NULL, then ASSERT().


  @param  NameGuid             A pointer to to the FFS filename GUID to search for
                               within any of the firmware volumes in the platform.
  @param  SectionType          Indicates the FFS section type to search for within
                               the FFS file specified by NameGuid.
  @param  SectionInstance      Indicates which section instance within the FFS file
                               specified by NameGuid to retrieve.
  @param  Buffer               On output, a pointer to a callee allocated buffer
                               containing the FFS file section that was found.
                               Is it the caller's responsibility to free this buffer
                               using FreePool().
  @param  Size                 On output, a pointer to the size, in bytes, of Buffer.

  @retval  EFI_SUCCESS          The specified FFS section was returned.
  @retval  EFI_NOT_FOUND        The specified FFS section could not be found.
  @retval  EFI_OUT_OF_RESOURCES There are not enough resources available to
                                retrieve the matching FFS section.
  @retval  EFI_DEVICE_ERROR     The FFS section could not be retrieves due to a
                                device error.
  @retval  EFI_ACCESS_DENIED    The FFS section could not be retrieves because the
                                firmware volume that
                                contains the matching FFS section does not allow reads.
**/
EFI_STATUS
EFIAPI
GetSectionFromAnyFv  (
  IN CONST  EFI_GUID           *NameGuid,
  IN        EFI_SECTION_TYPE   SectionType,
  IN        UINTN              SectionInstance,
  OUT       VOID               **Buffer,
  OUT       UINTN              *Size
  )
{
  EFI_STATUS                    Status;
  EFI_HANDLE                    *HandleBuffer;
  UINTN                         HandleCount;
  UINTN                         Index;
  EFI_HANDLE                    FvHandle;

  //
  // Search the FV that contain the caller's FFS first.
  // FV builder can choose to build FFS into the this FV
  // so that this implementation of GetSectionFromAnyFv
  // will locate the FFS faster.
  //
  FvHandle = InternalImageHandleToFvHandle (gImageHandle);
  Status = InternalGetSectionFromFv (
             FvHandle,
             NameGuid,
             SectionType,
             SectionInstance,
             Buffer,
             Size
             );
  if (!EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }

  HandleBuffer = NULL;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiFirmwareVolume2ProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    //
    // Skip the FV that contain the caller's FFS
    //
    if (HandleBuffer[Index] != FvHandle) {
      Status = InternalGetSectionFromFv (
                 HandleBuffer[Index],
                 NameGuid,
                 SectionType,
                 SectionInstance,
                 Buffer,
                 Size
                 );

      if (!EFI_ERROR (Status)) {
        goto Done;
      }
    }

  }

  if (Index == HandleCount) {
    Status = EFI_NOT_FOUND;
  }

Done:

  if (HandleBuffer != NULL) {
    FreePool(HandleBuffer);
  }
  return Status;

}

/**
  Searches the firmware volume that the currently executing module was loaded from and returns the first matching FFS section.

  This function searches the firmware volume that the currently executing module was loaded
  from for an FFS file with an FFS filename specified by NameGuid. If the FFS file is found a search
  is made for FFS sections of type SectionType. If the FFS file contains at least SectionInstance
  instances of the FFS section specified by SectionType, then the SectionInstance instance is returned in Buffer.
  Buffer is allocated using AllocatePool(), and the size of the allocated buffer is returned in Size.
  It is the caller's responsibility to use FreePool() to free the allocated buffer.
  See EFI_FIRMWARE_VOLUME2_PROTOCOL.ReadSection() for details on how sections are retrieved from
  an FFS file based on SectionType and SectionInstance.

  If the currently executing module was not loaded from a firmware volume, then EFI_NOT_FOUND is returned.
  If SectionType is EFI_SECTION_TE, and the search with an FFS file fails,
  the search will be retried with a section type of EFI_SECTION_PE32.

  This function must be called with a TPL <= TPL_NOTIFY.
  If NameGuid is NULL, then ASSERT().
  If Buffer is NULL, then ASSERT().
  If Size is NULL, then ASSERT().

  @param  NameGuid             A pointer to to the FFS filename GUID to search for
                               within the firmware volumes that the currently
                               executing module was loaded from.
  @param  SectionType          Indicates the FFS section type to search for within
                               the FFS file specified by NameGuid.
  @param  SectionInstance      Indicates which section instance within the FFS file
                               specified by NameGuid to retrieve.
  @param  Buffer               On output, a pointer to a callee allocated buffer
                               containing the FFS file section that was found.
                               Is it the caller's responsibility to free this buffer
                               using FreePool().
  @param  Size                 On output, a pointer to the size, in bytes, of Buffer.


  @retval  EFI_SUCCESS          The specified FFS section was returned.
  @retval  EFI_NOT_FOUND        The specified FFS section could not be found.
  @retval  EFI_OUT_OF_RESOURCES There are not enough resources available to retrieve
                                the matching FFS section.
  @retval  EFI_DEVICE_ERROR     The FFS section could not be retrieves due to a
                                device error.
  @retval  EFI_ACCESS_DENIED    The FFS section could not be retrieves because the
                                firmware volume that contains the matching FFS
                                section does not allow reads.
**/
EFI_STATUS
EFIAPI
GetSectionFromFv (
  IN  CONST EFI_GUID                *NameGuid,
  IN  EFI_SECTION_TYPE              SectionType,
  IN  UINTN                         SectionInstance,
  OUT VOID                          **Buffer,
  OUT UINTN                         *Size
    )
{
  return InternalGetSectionFromFv (
           InternalImageHandleToFvHandle(gImageHandle),
           NameGuid,
           SectionType,
           SectionInstance,
           Buffer,
           Size
           );
}


/**
  Searches the FFS file the currently executing module was loaded from and returns the first matching FFS section.

  This function searches the FFS file that the currently executing module was loaded from for a FFS sections of type SectionType.
  If the FFS file contains at least SectionInstance instances of the FFS section specified by SectionType,
  then the SectionInstance instance is returned in Buffer. Buffer is allocated using AllocatePool(),
  and the size of the allocated buffer is returned in Size. It is the caller's responsibility
  to use FreePool() to free the allocated buffer. See EFI_FIRMWARE_VOLUME2_PROTOCOL.ReadSection() for
  details on how sections are retrieved from an FFS file based on SectionType and SectionInstance.

  If the currently executing module was not loaded from an FFS file, then EFI_NOT_FOUND is returned.
  If SectionType is EFI_SECTION_TE, and the search with an FFS file fails,
  the search will be retried with a section type of EFI_SECTION_PE32.
  This function must be called with a TPL <= TPL_NOTIFY.

  If Buffer is NULL, then ASSERT().
  If Size is NULL, then ASSERT().


  @param  SectionType          Indicates the FFS section type to search for within
                               the FFS file that the currently executing module
                               was loaded from.
  @param  SectionInstance      Indicates which section instance to retrieve within
                               the FFS file that the currently executing module
                               was loaded from.
  @param  Buffer               On output, a pointer to a callee allocated buffer
                               containing the FFS file section that was found.
                               Is it the caller's responsibility to free this buffer
                               using FreePool().
  @param  Size                 On output, a pointer to the size, in bytes, of Buffer.

  @retval  EFI_SUCCESS          The specified FFS section was returned.
  @retval  EFI_NOT_FOUND        The specified FFS section could not be found.
  @retval  EFI_OUT_OF_RESOURCES There are not enough resources available to retrieve
                                the matching FFS section.
  @retval  EFI_DEVICE_ERROR     The FFS section could not be retrieves due to a
                                device error.
  @retval  EFI_ACCESS_DENIED    The FFS section could not be retrieves because the
                                firmware volume that contains the matching FFS
                                section does not allow reads.

**/
EFI_STATUS
EFIAPI
GetSectionFromFfs (
  IN  EFI_SECTION_TYPE              SectionType,
  IN  UINTN                         SectionInstance,
  OUT VOID                          **Buffer,
  OUT UINTN                         *Size
    )
{
  return InternalGetSectionFromFv(
           InternalImageHandleToFvHandle(gImageHandle),
           &gEfiCallerIdGuid,
           SectionType,
           SectionInstance,
           Buffer,
           Size
           );
}


/**
  Get the image file buffer data and buffer size by its device path.

  Access the file either from a firmware volume, from a file system interface,
  or from the load file interface.

  Allocate memory to store the found image. The caller is responsible to free memory.

  If FilePath is NULL, then NULL is returned.
  If FileSize is NULL, then NULL is returned.
  If AuthenticationStatus is NULL, then NULL is returned.

  @param[in]       BootPolicy           Policy for Open Image File.If TRUE, indicates
                                        that the request originates from the boot
                                        manager, and that the boot manager is
                                        attempting to load FilePath as a boot
                                        selection. If FALSE, then FilePath must
                                        match an exact file to be loaded.
  @param[in]       FilePath             The pointer to the device path of the file
                                        that is abstracted to the file buffer.
  @param[out]      FileSize             The pointer to the size of the abstracted
                                        file buffer.
  @param[out]      AuthenticationStatus Pointer to the authentication status.

  @retval NULL   FilePath is NULL, or FileSize is NULL, or AuthenticationStatus is NULL, or the file can't be found.
  @retval other  The abstracted file buffer. The caller is responsible to free memory.
**/
VOID *
EFIAPI
GetFileBufferByFilePath (
  IN BOOLEAN                           BootPolicy,
  IN CONST EFI_DEVICE_PATH_PROTOCOL    *FilePath,
  OUT      UINTN                       *FileSize,
  OUT UINT32                           *AuthenticationStatus
  )
{
  EFI_DEVICE_PATH_PROTOCOL          *DevicePathNode;
  EFI_DEVICE_PATH_PROTOCOL          *OrigDevicePathNode;
  EFI_DEVICE_PATH_PROTOCOL          *TempDevicePathNode;
  EFI_HANDLE                        Handle;
  EFI_GUID                          *FvNameGuid;
  EFI_FIRMWARE_VOLUME2_PROTOCOL     *FwVol;
  EFI_SECTION_TYPE                  SectionType;
  UINT8                             *ImageBuffer;
  UINTN                             ImageBufferSize;
  EFI_FV_FILETYPE                   Type;
  EFI_FV_FILE_ATTRIBUTES            Attrib;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL   *Volume;
  EFI_FILE_HANDLE                   FileHandle;
  EFI_FILE_HANDLE                   LastHandle;
  EFI_FILE_INFO                     *FileInfo;
  UINTN                             FileInfoSize;
  EFI_LOAD_FILE_PROTOCOL            *LoadFile;
  EFI_LOAD_FILE2_PROTOCOL           *LoadFile2;
  EFI_STATUS                        Status;

  //
  // Check input File device path.
  //
  if (FilePath == NULL || FileSize == NULL || AuthenticationStatus == NULL) {
    return NULL;
  }

  //
  // Init local variable
  //
  TempDevicePathNode  = NULL;
  FvNameGuid          = NULL;
  FileInfo            = NULL;
  FileHandle          = NULL;
  ImageBuffer         = NULL;
  ImageBufferSize     = 0;
  *AuthenticationStatus = 0;

  //
  // Copy File Device Path
  //
  OrigDevicePathNode = DuplicateDevicePath (FilePath);
  if (OrigDevicePathNode == NULL) {
    return NULL;
  }

  //
  // Check whether this device path support FV2 protocol.
  // Is so, this device path may contain a Image.
  //
  DevicePathNode = OrigDevicePathNode;
  Status = gBS->LocateDevicePath (&gEfiFirmwareVolume2ProtocolGuid, &DevicePathNode, &Handle);
  if (!EFI_ERROR (Status)) {
    //
    // For FwVol File system there is only a single file name that is a GUID.
    //
    FvNameGuid = EfiGetNameGuidFromFwVolDevicePathNode ((CONST MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *) DevicePathNode);
    if (FvNameGuid == NULL) {
      Status = EFI_INVALID_PARAMETER;
    } else {
      //
      // Read image from the firmware file
      //
      Status = gBS->HandleProtocol (Handle, &gEfiFirmwareVolume2ProtocolGuid, (VOID**)&FwVol);
      if (!EFI_ERROR (Status)) {
        SectionType = EFI_SECTION_PE32;
        ImageBuffer = NULL;
        Status = FwVol->ReadSection (
                          FwVol,
                          FvNameGuid,
                          SectionType,
                          0,
                          (VOID **)&ImageBuffer,
                          &ImageBufferSize,
                          AuthenticationStatus
                          );
        if (EFI_ERROR (Status)) {
          //
          // Try a raw file, since a PE32 SECTION does not exist
          //
          if (ImageBuffer != NULL) {
            FreePool (ImageBuffer);
            *AuthenticationStatus = 0;
          }
          ImageBuffer = NULL;
          Status = FwVol->ReadFile (
                            FwVol,
                            FvNameGuid,
                            (VOID **)&ImageBuffer,
                            &ImageBufferSize,
                            &Type,
                            &Attrib,
                            AuthenticationStatus
                            );
        }
      }
    }
    if (!EFI_ERROR (Status)) {
      goto Finish;
    }
  }

  //
  // Attempt to access the file via a file system interface
  //
  DevicePathNode = OrigDevicePathNode;
  Status = gBS->LocateDevicePath (&gEfiSimpleFileSystemProtocolGuid, &DevicePathNode, &Handle);
  if (!EFI_ERROR (Status)) {
    Status = gBS->HandleProtocol (Handle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&Volume);
    if (!EFI_ERROR (Status)) {
      //
      // Open the Volume to get the File System handle
      //
      Status = Volume->OpenVolume (Volume, &FileHandle);
      if (!EFI_ERROR (Status)) {
        //
        // Duplicate the device path to avoid the access to unaligned device path node.
        // Because the device path consists of one or more FILE PATH MEDIA DEVICE PATH
        // nodes, It assures the fields in device path nodes are 2 byte aligned.
        //
        TempDevicePathNode = DuplicateDevicePath (DevicePathNode);
        if (TempDevicePathNode == NULL) {
          FileHandle->Close (FileHandle);
          //
          // Setting Status to an EFI_ERROR value will cause the rest of
          // the file system support below to be skipped.
          //
          Status = EFI_OUT_OF_RESOURCES;
        }
        //
        // Parse each MEDIA_FILEPATH_DP node. There may be more than one, since the
        // directory information and filename can be separate. The goal is to inch
        // our way down each device path node and close the previous node
        //
        DevicePathNode = TempDevicePathNode;
        while (!EFI_ERROR (Status) && !IsDevicePathEnd (DevicePathNode)) {
          if (DevicePathType (DevicePathNode) != MEDIA_DEVICE_PATH ||
              DevicePathSubType (DevicePathNode) != MEDIA_FILEPATH_DP) {
            Status = EFI_UNSUPPORTED;
            break;
          }

          LastHandle = FileHandle;
          FileHandle = NULL;

          Status = LastHandle->Open (
                                LastHandle,
                                &FileHandle,
                                ((FILEPATH_DEVICE_PATH *) DevicePathNode)->PathName,
                                EFI_FILE_MODE_READ,
                                0
                                );

          //
          // Close the previous node
          //
          LastHandle->Close (LastHandle);

          DevicePathNode = NextDevicePathNode (DevicePathNode);
        }

        if (!EFI_ERROR (Status)) {
          //
          // We have found the file. Now we need to read it. Before we can read the file we need to
          // figure out how big the file is.
          //
          FileInfo = NULL;
          FileInfoSize = 0;
          Status = FileHandle->GetInfo (
                                FileHandle,
                                &gEfiFileInfoGuid,
                                &FileInfoSize,
                                FileInfo
                                );

          if (Status == EFI_BUFFER_TOO_SMALL) {
            FileInfo = AllocatePool (FileInfoSize);
            if (FileInfo == NULL) {
              Status = EFI_OUT_OF_RESOURCES;
            } else {
              Status = FileHandle->GetInfo (
                                    FileHandle,
                                    &gEfiFileInfoGuid,
                                    &FileInfoSize,
                                    FileInfo
                                    );
            }
          }

          if (!EFI_ERROR (Status) && (FileInfo != NULL)) {
            if ((FileInfo->Attribute & EFI_FILE_DIRECTORY) == 0) {
              //
              // Allocate space for the file
              //
              ImageBuffer = AllocatePool ((UINTN)FileInfo->FileSize);
              if (ImageBuffer == NULL) {
                Status = EFI_OUT_OF_RESOURCES;
              } else {
                //
                // Read the file into the buffer we allocated
                //
                ImageBufferSize = (UINTN)FileInfo->FileSize;
                Status          = FileHandle->Read (FileHandle, &ImageBufferSize, ImageBuffer);
              }
            }
          }
        }
        //
        // Close the file and Free FileInfo and TempDevicePathNode since we are done
        //
        if (FileInfo != NULL) {
          FreePool (FileInfo);
        }
        if (FileHandle != NULL) {
          FileHandle->Close (FileHandle);
        }
        if (TempDevicePathNode != NULL) {
          FreePool (TempDevicePathNode);
        }
      }
    }
    if (!EFI_ERROR (Status)) {
      goto Finish;
    }
  }

  //
  // Attempt to access the file via LoadFile2 interface
  //
  if (!BootPolicy) {
    DevicePathNode = OrigDevicePathNode;
    Status = gBS->LocateDevicePath (&gEfiLoadFile2ProtocolGuid, &DevicePathNode, &Handle);
    if (!EFI_ERROR (Status)) {
      Status = gBS->HandleProtocol (Handle, &gEfiLoadFile2ProtocolGuid, (VOID**)&LoadFile2);
      if (!EFI_ERROR (Status)) {
        //
        // Call LoadFile2 with the correct buffer size
        //
        ImageBufferSize = 0;
        ImageBuffer     = NULL;
        Status = LoadFile2->LoadFile (
                             LoadFile2,
                             DevicePathNode,
                             FALSE,
                             &ImageBufferSize,
                             ImageBuffer
                             );
        if (Status == EFI_BUFFER_TOO_SMALL) {
          ImageBuffer = AllocatePool (ImageBufferSize);
          if (ImageBuffer == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
          } else {
            Status = LoadFile2->LoadFile (
                                 LoadFile2,
                                 DevicePathNode,
                                 FALSE,
                                 &ImageBufferSize,
                                 ImageBuffer
                                 );
          }
        }
      }
      if (!EFI_ERROR (Status)) {
        goto Finish;
      }
    }
  }

  //
  // Attempt to access the file via LoadFile interface
  //
  DevicePathNode = OrigDevicePathNode;
  Status = gBS->LocateDevicePath (&gEfiLoadFileProtocolGuid, &DevicePathNode, &Handle);
  if (!EFI_ERROR (Status)) {
    Status = gBS->HandleProtocol (Handle, &gEfiLoadFileProtocolGuid, (VOID**)&LoadFile);
    if (!EFI_ERROR (Status)) {
      //
      // Call LoadFile with the correct buffer size
      //
      ImageBufferSize = 0;
      ImageBuffer     = NULL;
      Status = LoadFile->LoadFile (
                           LoadFile,
                           DevicePathNode,
                           BootPolicy,
                           &ImageBufferSize,
                           ImageBuffer
                           );
      if (Status == EFI_BUFFER_TOO_SMALL) {
        ImageBuffer = AllocatePool (ImageBufferSize);
        if (ImageBuffer == NULL) {
          Status = EFI_OUT_OF_RESOURCES;
        } else {
          Status = LoadFile->LoadFile (
                               LoadFile,
                               DevicePathNode,
                               BootPolicy,
                               &ImageBufferSize,
                               ImageBuffer
                               );
        }
      }
    }
  }

Finish:

  if (EFI_ERROR (Status)) {
    if (ImageBuffer != NULL) {
      FreePool (ImageBuffer);
      ImageBuffer = NULL;
    }
    *FileSize = 0;
  } else {
    *FileSize = ImageBufferSize;
  }

  FreePool (OrigDevicePathNode);

  return ImageBuffer;
}

/**
  Searches all the available firmware volumes and returns the file device path of first matching
  FFS section.

  This function searches all the firmware volumes for FFS files with an FFS filename specified by NameGuid.
  The order that the firmware volumes is searched is not deterministic. For each FFS file found a search
  is made for FFS sections of type SectionType.

  If SectionType is EFI_SECTION_TE, and the search with an FFS file fails,
  the search will be retried with a section type of EFI_SECTION_PE32.
  This function must be called with a TPL <= TPL_NOTIFY.

  If NameGuid is NULL, then ASSERT().

   @param  NameGuid             A pointer to to the FFS filename GUID to search for
                                within any of the firmware volumes in the platform.
   @param  SectionType          Indicates the FFS section type to search for within
                                the FFS file specified by NameGuid.
   @param  SectionInstance      Indicates which section instance within the FFS file
                                specified by NameGuid to retrieve.
   @param  FvFileDevicePath     Device path for the target FFS
                                file.

   @retval  EFI_SUCCESS           The specified file device path of FFS section was returned.
   @retval  EFI_NOT_FOUND         The specified file device path of FFS section could not be found.
   @retval  EFI_DEVICE_ERROR      The FFS section could not be retrieves due to a
                                  device error.
   @retval  EFI_ACCESS_DENIED     The FFS section could not be retrieves because the
                                  firmware volume that contains the matching FFS section does not
                                  allow reads.
   @retval  EFI_INVALID_PARAMETER FvFileDevicePath is NULL.

**/
EFI_STATUS
EFIAPI
GetFileDevicePathFromAnyFv (
  IN CONST  EFI_GUID                  *NameGuid,
  IN        EFI_SECTION_TYPE          SectionType,
  IN        UINTN                     SectionInstance,
  OUT       EFI_DEVICE_PATH_PROTOCOL  **FvFileDevicePath
  )
{
  EFI_STATUS                        Status;
  EFI_HANDLE                        *HandleBuffer;
  UINTN                             HandleCount;
  UINTN                             Index;
  EFI_HANDLE                        FvHandle;
  EFI_DEVICE_PATH_PROTOCOL          *FvDevicePath;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *TempFvFileDevicePath;
  VOID                              *Buffer;
  UINTN                             Size;

  if (FvFileDevicePath == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  HandleBuffer         = NULL;
  FvDevicePath         = NULL;
  TempFvFileDevicePath = NULL;
  Buffer               = NULL;
  Size                 = 0;

  //
  // Search the FV that contain the caller's FFS first.
  // FV builder can choose to build FFS into the this FV
  // so that this implementation of GetSectionFromAnyFv
  // will locate the FFS faster.
  //
  FvHandle = InternalImageHandleToFvHandle (gImageHandle);
  Status = InternalGetSectionFromFv (
             FvHandle,
             NameGuid,
             SectionType,
             SectionInstance,
             &Buffer,
             &Size
             );
  if (!EFI_ERROR (Status)) {
    goto Done;
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiFirmwareVolume2ProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    //
    // Skip the FV that contain the caller's FFS
    //
    if (HandleBuffer[Index] != FvHandle) {
      Status = InternalGetSectionFromFv (
                 HandleBuffer[Index],
                 NameGuid,
                 SectionType,
                 SectionInstance,
                 &Buffer,
                 &Size
                 );

      if (!EFI_ERROR (Status)) {
        //
        // Update FvHandle to the current handle.
        //
        FvHandle = HandleBuffer[Index];
        goto Done;
      }
    }
  }

  if (Index == HandleCount) {
    Status = EFI_NOT_FOUND;
  }

Done:
  if (Status == EFI_SUCCESS) {
    //
    // Build a device path to the file in the FV to pass into gBS->LoadImage
    //
    Status = gBS->HandleProtocol (FvHandle, &gEfiDevicePathProtocolGuid, (VOID **)&FvDevicePath);
    if (EFI_ERROR (Status)) {
      *FvFileDevicePath = NULL;
    } else {
      TempFvFileDevicePath = AllocateZeroPool (sizeof (MEDIA_FW_VOL_FILEPATH_DEVICE_PATH) + END_DEVICE_PATH_LENGTH);
      if (TempFvFileDevicePath == NULL) {
        *FvFileDevicePath = NULL;
        return EFI_OUT_OF_RESOURCES;
      }
      EfiInitializeFwVolDevicepathNode ((MEDIA_FW_VOL_FILEPATH_DEVICE_PATH*)TempFvFileDevicePath, NameGuid);
      SetDevicePathEndNode (NextDevicePathNode (TempFvFileDevicePath));
      *FvFileDevicePath = AppendDevicePath (
                            FvDevicePath,
                            (EFI_DEVICE_PATH_PROTOCOL *)TempFvFileDevicePath
                            );
      FreePool (TempFvFileDevicePath);
    }
  }

  if (Buffer != NULL) {
    FreePool (Buffer);
  }

  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
  }

  return Status;
}
