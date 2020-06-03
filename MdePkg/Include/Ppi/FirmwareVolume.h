/** @file
  This file provides functions for accessing a memory-mapped firmware volume of a specific format.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is from PI Version 1.0 errata.

**/

#ifndef __FIRMWARE_VOLUME_PPI_H__
#define __FIRMWARE_VOLUME_PPI_H__

///
/// The GUID for this PPI is the same as the firmware volume format GUID.
/// The FV format can be EFI_FIRMWARE_FILE_SYSTEM2_GUID or the GUID for a user-defined
/// format. The EFI_FIRMWARE_FILE_SYSTEM2_GUID is the PI Firmware Volume format.
///
typedef struct _EFI_PEI_FIRMWARE_VOLUME_PPI   EFI_PEI_FIRMWARE_VOLUME_PPI;


/**
  Process a firmware volume and create a volume handle.

  Create a volume handle from the information in the buffer. For
  memory-mapped firmware volumes, Buffer and BufferSize refer to
  the start of the firmware volume and the firmware volume size.
  For non memory-mapped firmware volumes, this points to a
  buffer which contains the necessary information for creating
  the firmware volume handle. Normally, these values are derived
  from the EFI_FIRMWARE_VOLUME_INFO_PPI.


  @param This                   Points to this instance of the
                                EFI_PEI_FIRMWARE_VOLUME_PPI.
  @param Buffer                 Points to the start of the buffer.
  @param BufferSize             Size of the buffer.
  @param FvHandle               Points to the returned firmware volume
                                handle. The firmware volume handle must
                                be unique within the system.

  @retval EFI_SUCCESS           Firmware volume handle created.
  @retval EFI_VOLUME_CORRUPTED  Volume was corrupt.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FV_PROCESS_FV)(
  IN  CONST  EFI_PEI_FIRMWARE_VOLUME_PPI *This,
  IN  VOID                               *Buffer,
  IN  UINTN                              BufferSize,
  OUT EFI_PEI_FV_HANDLE                  *FvHandle
);

/**
  Finds the next file of the specified type.

  This service enables PEI modules to discover additional firmware files.
  The FileHandle must be unique within the system.

  @param This           Points to this instance of the
                        EFI_PEI_FIRMWARE_VOLUME_PPI.
  @param SearchType     A filter to find only files of this type. Type
                        EFI_FV_FILETYPE_ALL causes no filtering to be
                        done.
  @param FvHandle       Handle of firmware volume in which to
                        search.
  @param FileHandle     Points to the current handle from which to
                        begin searching or NULL to start at the
                        beginning of the firmware volume. Updated
                        upon return to reflect the file found.

  @retval EFI_SUCCESS   The file was found.
  @retval EFI_NOT_FOUND The file was not found. FileHandle contains NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FV_FIND_FILE_TYPE)(
  IN     CONST EFI_PEI_FIRMWARE_VOLUME_PPI   *This,
  IN     EFI_FV_FILETYPE                     SearchType,
  IN     EFI_PEI_FV_HANDLE                   FvHandle,
  IN OUT EFI_PEI_FILE_HANDLE                 *FileHandle
);


/**
  Find a file within a volume by its name.

  This service searches for files with a specific name, within
  either the specified firmware volume or all firmware volumes.

  @param This                   Points to this instance of the
                                EFI_PEI_FIRMWARE_VOLUME_PPI.
  @param FileName               A pointer to the name of the file to find
                                within the firmware volume.
  @param FvHandle               Upon entry, the pointer to the firmware
                                volume to search or NULL if all firmware
                                volumes should be searched. Upon exit, the
                                actual firmware volume in which the file was
                                found.
  @param FileHandle             Upon exit, points to the found file's
                                handle or NULL if it could not be found.

  @retval EFI_SUCCESS           File was found.
  @retval EFI_NOT_FOUND         File was not found.
  @retval EFI_INVALID_PARAMETER FvHandle or FileHandle or
                                FileName was NULL.


**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FV_FIND_FILE_NAME)(
  IN  CONST  EFI_PEI_FIRMWARE_VOLUME_PPI *This,
  IN  CONST  EFI_GUID                    *FileName,
  IN  EFI_PEI_FV_HANDLE                  *FvHandle,
  OUT EFI_PEI_FILE_HANDLE                *FileHandle
);


/**
  Returns information about a specific file.

  This function returns information about a specific
  file, including its file name, type, attributes, starting
  address and size.

  @param This                     Points to this instance of the
                                  EFI_PEI_FIRMWARE_VOLUME_PPI.
  @param FileHandle               Handle of the file.
  @param FileInfo                 Upon exit, points to the file's
                                  information.

  @retval EFI_SUCCESS             File information returned.
  @retval EFI_INVALID_PARAMETER   If FileHandle does not
                                  represent a valid file.
  @retval EFI_INVALID_PARAMETER   If FileInfo is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FV_GET_FILE_INFO)(
  IN  CONST EFI_PEI_FIRMWARE_VOLUME_PPI   *This,
  IN  EFI_PEI_FILE_HANDLE                 FileHandle,
  OUT EFI_FV_FILE_INFO                    *FileInfo
);

/**
  Returns information about a specific file.

  This function returns information about a specific
  file, including its file name, type, attributes, starting
  address, size and authentication status.

  @param This                     Points to this instance of the
                                  EFI_PEI_FIRMWARE_VOLUME_PPI.
  @param FileHandle               Handle of the file.
  @param FileInfo                 Upon exit, points to the file's
                                  information.

  @retval EFI_SUCCESS             File information returned.
  @retval EFI_INVALID_PARAMETER   If FileHandle does not
                                  represent a valid file.
  @retval EFI_INVALID_PARAMETER   If FileInfo is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FV_GET_FILE_INFO2)(
  IN  CONST EFI_PEI_FIRMWARE_VOLUME_PPI   *This,
  IN  EFI_PEI_FILE_HANDLE                 FileHandle,
  OUT EFI_FV_FILE_INFO2                   *FileInfo
);

/**
  This function returns information about the firmware volume.

  @param This                     Points to this instance of the
                                  EFI_PEI_FIRMWARE_VOLUME_PPI.
  @param FvHandle                 Handle to the firmware handle.
  @param VolumeInfo               Points to the returned firmware volume
                                  information.

  @retval EFI_SUCCESS             Information returned successfully.
  @retval EFI_INVALID_PARAMETER   FvHandle does not indicate a valid
                                  firmware volume or VolumeInfo is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FV_GET_INFO)(
  IN  CONST  EFI_PEI_FIRMWARE_VOLUME_PPI   *This,
  IN  EFI_PEI_FV_HANDLE                    FvHandle,
  OUT EFI_FV_INFO                          *VolumeInfo
);

/**
  Find the next matching section in the firmware file.

  This service enables PEI modules to discover sections
  of a given type within a valid file.

  @param This             Points to this instance of the
                          EFI_PEI_FIRMWARE_VOLUME_PPI.
  @param SearchType       A filter to find only sections of this
                          type.
  @param FileHandle       Handle of firmware file in which to
                          search.
  @param SectionData      Updated upon  return to point to the
                          section found.

  @retval EFI_SUCCESS     Section was found.
  @retval EFI_NOT_FOUND   Section of the specified type was not
                          found. SectionData contains NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FV_FIND_SECTION)(
  IN  CONST EFI_PEI_FIRMWARE_VOLUME_PPI    *This,
  IN  EFI_SECTION_TYPE                     SearchType,
  IN  EFI_PEI_FILE_HANDLE                  FileHandle,
  OUT VOID                                 **SectionData
);

/**
  Find the next matching section in the firmware file.

  This service enables PEI modules to discover sections
  of a given instance and type within a valid file.

  @param This                   Points to this instance of the
                                EFI_PEI_FIRMWARE_VOLUME_PPI.
  @param SearchType             A filter to find only sections of this
                                type.
  @param SearchInstance         A filter to find the specific instance
                                of sections.
  @param FileHandle             Handle of firmware file in which to
                                search.
  @param SectionData            Updated upon return to point to the
                                section found.
  @param AuthenticationStatus   Updated upon return to point to the
                                authentication status for this section.

  @retval EFI_SUCCESS     Section was found.
  @retval EFI_NOT_FOUND   Section of the specified type was not
                          found. SectionData contains NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_FV_FIND_SECTION2)(
  IN  CONST EFI_PEI_FIRMWARE_VOLUME_PPI    *This,
  IN  EFI_SECTION_TYPE                     SearchType,
  IN  UINTN                                SearchInstance,
  IN  EFI_PEI_FILE_HANDLE                  FileHandle,
  OUT VOID                                 **SectionData,
  OUT UINT32                               *AuthenticationStatus
);

#define EFI_PEI_FIRMWARE_VOLUME_PPI_SIGNATURE SIGNATURE_32 ('P', 'F', 'V', 'P')
#define EFI_PEI_FIRMWARE_VOLUME_PPI_REVISION 0x00010030

///
/// This PPI provides functions for accessing a memory-mapped firmware volume of a specific format.
///
struct _EFI_PEI_FIRMWARE_VOLUME_PPI {
  EFI_PEI_FV_PROCESS_FV       ProcessVolume;
  EFI_PEI_FV_FIND_FILE_TYPE   FindFileByType;
  EFI_PEI_FV_FIND_FILE_NAME   FindFileByName;
  EFI_PEI_FV_GET_FILE_INFO    GetFileInfo;
  EFI_PEI_FV_GET_INFO         GetVolumeInfo;
  EFI_PEI_FV_FIND_SECTION     FindSectionByType;
  EFI_PEI_FV_GET_FILE_INFO2   GetFileInfo2;
  EFI_PEI_FV_FIND_SECTION2    FindSectionByType2;
  ///
  /// Signature is used to keep backward-compatibility, set to {'P','F','V','P'}.
  ///
  UINT32                      Signature;
  ///
  /// Revision for further extension.
  ///
  UINT32                      Revision;
};

extern EFI_GUID gEfiPeiFirmwareVolumePpiGuid;

#endif
