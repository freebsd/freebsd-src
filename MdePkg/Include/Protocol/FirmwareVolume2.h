/** @file
  The Firmware Volume Protocol provides file-level access to the firmware volume. 
  Each firmware volume driver must produce an instance of the 
  Firmware Volume Protocol if the firmware volume is to be visible to
  the system during the DXE phase. The Firmware Volume Protocol also provides
  mechanisms for determining and modifying some attributes of the firmware volume.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

  @par Revision Reference: PI
  Version 1.00.

**/

#ifndef __FIRMWARE_VOLUME2_H__
#define __FIRMWARE_VOLUME2_H__

#define EFI_FIRMWARE_VOLUME2_PROTOCOL_GUID \
  { 0x220e73b6, 0x6bdb, 0x4413, { 0x84, 0x5, 0xb9, 0x74, 0xb1, 0x8, 0x61, 0x9a } }

typedef struct _EFI_FIRMWARE_VOLUME2_PROTOCOL EFI_FIRMWARE_VOLUME2_PROTOCOL;


///
/// EFI_FV_ATTRIBUTES
///
typedef UINT64  EFI_FV_ATTRIBUTES;

//
// EFI_FV_ATTRIBUTES bit definitions
//
// EFI_FV_ATTRIBUTES bit semantics
#define EFI_FV2_READ_DISABLE_CAP        0x0000000000000001ULL
#define EFI_FV2_READ_ENABLE_CAP         0x0000000000000002ULL
#define EFI_FV2_READ_STATUS             0x0000000000000004ULL
#define EFI_FV2_WRITE_DISABLE_CAP       0x0000000000000008ULL
#define EFI_FV2_WRITE_ENABLE_CAP        0x0000000000000010ULL
#define EFI_FV2_WRITE_STATUS            0x0000000000000020ULL
#define EFI_FV2_LOCK_CAP                0x0000000000000040ULL
#define EFI_FV2_LOCK_STATUS             0x0000000000000080ULL
#define EFI_FV2_WRITE_POLICY_RELIABLE   0x0000000000000100ULL
#define EFI_FV2_READ_LOCK_CAP           0x0000000000001000ULL
#define EFI_FV2_READ_LOCK_STATUS        0x0000000000002000ULL
#define EFI_FV2_WRITE_LOCK_CAP          0x0000000000004000ULL
#define EFI_FV2_WRITE_LOCK_STATUS       0x0000000000008000ULL
#define EFI_FV2_ALIGNMENT               0x00000000001F0000ULL
#define EFI_FV2_ALIGNMENT_1             0x0000000000000000ULL
#define EFI_FV2_ALIGNMENT_2             0x0000000000010000ULL
#define EFI_FV2_ALIGNMENT_4             0x0000000000020000ULL
#define EFI_FV2_ALIGNMENT_8             0x0000000000030000ULL
#define EFI_FV2_ALIGNMENT_16            0x0000000000040000ULL
#define EFI_FV2_ALIGNMENT_32            0x0000000000050000ULL
#define EFI_FV2_ALIGNMENT_64            0x0000000000060000ULL
#define EFI_FV2_ALIGNMENT_128           0x0000000000070000ULL
#define EFI_FV2_ALIGNMENT_256           0x0000000000080000ULL
#define EFI_FV2_ALIGNMENT_512           0x0000000000090000ULL
#define EFI_FV2_ALIGNMENT_1K            0x00000000000A0000ULL
#define EFI_FV2_ALIGNMENT_2K            0x00000000000B0000ULL
#define EFI_FV2_ALIGNMENT_4K            0x00000000000C0000ULL
#define EFI_FV2_ALIGNMENT_8K            0x00000000000D0000ULL
#define EFI_FV2_ALIGNMENT_16K           0x00000000000E0000ULL
#define EFI_FV2_ALIGNMENT_32K           0x00000000000F0000ULL
#define EFI_FV2_ALIGNMENT_64K           0x0000000000100000ULL
#define EFI_FV2_ALIGNMENT_128K          0x0000000000110000ULL
#define EFI_FV2_ALIGNMENT_256K          0x0000000000120000ULL
#define EFI_FV2_ALIGNMENT_512K          0x0000000000130000ULL
#define EFI_FV2_ALIGNMENT_1M            0x0000000000140000ULL
#define EFI_FV2_ALIGNMENT_2M            0x0000000000150000ULL
#define EFI_FV2_ALIGNMENT_4M            0x0000000000160000ULL
#define EFI_FV2_ALIGNMENT_8M            0x0000000000170000ULL
#define EFI_FV2_ALIGNMENT_16M           0x0000000000180000ULL
#define EFI_FV2_ALIGNMENT_32M           0x0000000000190000ULL
#define EFI_FV2_ALIGNMENT_64M           0x00000000001A0000ULL
#define EFI_FV2_ALIGNMENT_128M          0x00000000001B0000ULL
#define EFI_FV2_ALIGNMENT_256M          0x00000000001C0000ULL
#define EFI_FV2_ALIGNMENT_512M          0x00000000001D0000ULL
#define EFI_FV2_ALIGNMENT_1G            0x00000000001E0000ULL
#define EFI_FV2_ALIGNMENT_2G            0x00000000001F0000ULL

/**
  Returns the attributes and current settings of the firmware volume.

  Because of constraints imposed by the underlying firmware
  storage, an instance of the Firmware Volume Protocol may not
  be to able to support all possible variations of this
  architecture. These constraints and the current state of the
  firmware volume are exposed to the caller using the
  GetVolumeAttributes() function. GetVolumeAttributes() is
  callable only from TPL_NOTIFY and below. Behavior of
  GetVolumeAttributes() at any EFI_TPL above TPL_NOTIFY is
  undefined.

  @param  This          Indicates the EFI_FIRMWARE_VOLUME2_PROTOCOL instance.
  
  @param  FvAttributes  Pointer to an EFI_FV_ATTRIBUTES in which
                        the attributes and current settings are
                        returned.


  @retval EFI_SUCCESS   The firmware volume attributes were
                        returned.

**/
typedef
EFI_STATUS
(EFIAPI * EFI_FV_GET_ATTRIBUTES)(
  IN CONST  EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
  OUT       EFI_FV_ATTRIBUTES             *FvAttributes
);


/**
  Modifies the current settings of the firmware volume according to the input parameter.
  
  The SetVolumeAttributes() function is used to set configurable
  firmware volume attributes. Only EFI_FV_READ_STATUS,
  EFI_FV_WRITE_STATUS, and EFI_FV_LOCK_STATUS may be modified, and
   then only in accordance with the declared capabilities. All
  other bits of FvAttributes are ignored on input. On successful
  return, all bits of *FvAttributes are valid and it contains the
  completed EFI_FV_ATTRIBUTES for the volume. To modify an
  attribute, the corresponding status bit in the EFI_FV_ATTRIBUTES
  is set to the desired value on input. The EFI_FV_LOCK_STATUS bit
  does not affect the ability to read or write the firmware
  volume. Rather, once the EFI_FV_LOCK_STATUS bit is set, it
  prevents further modification to all the attribute bits.
  SetVolumeAttributes() is callable only from TPL_NOTIFY and
  below. Behavior of SetVolumeAttributes() at any EFI_TPL above
  TPL_NOTIFY is undefined.

  @param  This          Indicates the EFI_FIRMWARE_VOLUME2_PROTOCOL instance.
  
  @param  FvAttributes  On input, FvAttributes is a pointer to
                        an EFI_FV_ATTRIBUTES containing the
                        desired firmware volume settings. On
                        successful return, it contains the new
                        settings of the firmware volume. On
                        unsuccessful return, FvAttributes is not
                        modified and the firmware volume
                        settings are not changed.
  
  @retval EFI_SUCCESS           The requested firmware volume attributes
                                were set and the resulting
                                EFI_FV_ATTRIBUTES is returned in
                                FvAttributes.

  @retval EFI_INVALID_PARAMETER FvAttributes:EFI_FV_READ_STATUS
                                is set to 1 on input, but the
                                device does not support enabling
                                reads
                                (FvAttributes:EFI_FV_READ_ENABLE
                                is clear on return from
                                GetVolumeAttributes()). Actual
                                volume attributes are unchanged.

  @retval EFI_INVALID_PARAMETER FvAttributes:EFI_FV_READ_STATUS
                                is cleared to 0 on input, but
                                the device does not support
                                disabling reads
                                (FvAttributes:EFI_FV_READ_DISABL
                                is clear on return from
                                GetVolumeAttributes()). Actual
                                volume attributes are unchanged.

  @retval EFI_INVALID_PARAMETER FvAttributes:EFI_FV_WRITE_STATUS
                                is set to 1 on input, but the
                                device does not support enabling
                                writes
                                (FvAttributes:EFI_FV_WRITE_ENABL
                                is clear on return from
                                GetVolumeAttributes()). Actual
                                volume attributes are unchanged.

  @retval EFI_INVALID_PARAMETER FvAttributes:EFI_FV_WRITE_STATUS
                                is cleared to 0 on input, but
                                the device does not support
                                disabling writes
                                (FvAttributes:EFI_FV_WRITE_DISAB
                                is clear on return from
                                GetVolumeAttributes()). Actual
                                volume attributes are unchanged.

  @retval EFI_INVALID_PARAMETER FvAttributes:EFI_FV_LOCK_STATUS
                                is set on input, but the device
                                does not support locking
                                (FvAttributes:EFI_FV_LOCK_CAP is
                                clear on return from
                                GetVolumeAttributes()). Actual
                                volume attributes are unchanged.

  @retval EFI_ACCESS_DENIED     Device is locked and does not
                                allow attribute modification
                                (FvAttributes:EFI_FV_LOCK_STATUS
                                is set on return from
                                GetVolumeAttributes()). Actual
                                volume attributes are unchanged.

**/
typedef
EFI_STATUS
(EFIAPI * EFI_FV_SET_ATTRIBUTES)(
  IN CONST  EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
  IN OUT    EFI_FV_ATTRIBUTES             *FvAttributes
);


/**
  Retrieves a file and/or file information from the firmware volume.

  ReadFile() is used to retrieve any file from a firmware volume
  during the DXE phase. The actual binary encoding of the file in
  the firmware volume media may be in any arbitrary format as long
  as it does the following: It is accessed using the Firmware
  Volume Protocol. The image that is returned follows the image
  format defined in Code Definitions: PI Firmware File Format.
  If the input value of Buffer==NULL, it indicates the caller is
  requesting only that the type, attributes, and size of the
  file be returned and that there is no output buffer. In this
  case, the following occurs:
  - BufferSize is returned with the size that is required to
    successfully complete the read.
  - The output parameters FoundType and *FileAttributes are
  returned with valid values.
  - The returned value of *AuthenticationStatus is undefined.

  If the input value of Buffer!=NULL, the output buffer is
  specified by a double indirection of the Buffer parameter. The
  input value of *Buffer is used to determine if the output
  buffer is caller allocated or is dynamically allocated by
  ReadFile(). If the input value of *Buffer!=NULL, it indicates
  the output buffer is caller allocated. In this case, the input
   value of *BufferSize indicates the size of the
  caller-allocated output buffer. If the output buffer is not
  large enough to contain the entire requested output, it is
  filled up to the point that the output buffer is exhausted and
  EFI_WARN_BUFFER_TOO_SMALL is returned, and then BufferSize is
   returned with the size required to successfully complete the
  read. All other output parameters are returned with valid
  values. If the input value of *Buffer==NULL, it indicates the
  output buffer is to be allocated by ReadFile(). In this case,
  ReadFile() will allocate an appropriately sized buffer from
  boot services pool memory, which will be returned in Buffer.
  The size of the new buffer is returned in BufferSize and all
  other output parameters are returned with valid values.
  ReadFile() is callable only from TPL_NOTIFY and below.
  Behavior of ReadFile() at any EFI_TPL above TPL_NOTIFY is
  undefined.

  @param  This                  Indicates the EFI_FIRMWARE_VOLUME2_PROTOCOL instance.
  
  @param  NameGuid              Pointer to an EFI_GUID, which is the file
                                name. All firmware file names are EFI_GUIDs.
                                A single firmware volume must not have two
                                valid files with the same file name
                                EFI_GUID.
  
  @param  Buffer                Pointer to a pointer to a buffer in which the
                                file contents are returned, not including the
                                file header.

  @param  BufferSize            Pointer to a caller-allocated UINTN. It
                                indicates the size of the memory
                                represented by Buffer.
  
  @param  FoundType             Pointer to a caller-allocated EFI_FV_FILETYPE.
  
  @param  FileAttributes        Pointer to a  caller-allocated
                                EFI_FV_FILE_ATTRIBUTES.
  
  @param  AuthenticationStatus  Pointer to a caller-allocated
                                UINT32 in which the
                                authentication status is
                                returned.
  
  @retval EFI_SUCCESS                 The call completed successfully.
  
  @retval EFI_WARN_BUFFER_TOO_SMALL   The buffer is too small to
                                      contain the requested
                                      output. The buffer is
                                      filled and the output is
                                      truncated.

  @retval EFI_OUT_OF_RESOURCES        An allocation failure occurred.

  @retval EFI_NOT_FOUND               Name was not found in the firmware volume.

  @retval EFI_DEVICE_ERROR            A hardware error occurred when
                                      attempting to access the firmware volume.

  @retval EFI_ACCESS_DENIED           The firmware volume is configured to
                                      disallow reads.

**/
typedef
EFI_STATUS
(EFIAPI * EFI_FV_READ_FILE)(
  IN CONST  EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
  IN CONST  EFI_GUID                      *NameGuid,
  IN OUT    VOID                          **Buffer,
  IN OUT    UINTN                         *BufferSize,
  OUT       EFI_FV_FILETYPE               *FoundType,
  OUT       EFI_FV_FILE_ATTRIBUTES        *FileAttributes,
  OUT       UINT32                        *AuthenticationStatus
);



/**
  Locates the requested section within a file and returns it in a buffer.

  ReadSection() is used to retrieve a specific section from a file
  within a firmware volume. The section returned is determined
  using a depth-first, left-to-right search algorithm through all
  sections found in the specified file. The output buffer is specified by a double indirection
  of the Buffer parameter. The input value of Buffer is used to
  determine if the output buffer is caller allocated or is
  dynamically allocated by ReadSection(). If the input value of
  Buffer!=NULL, it indicates that the output buffer is caller
  allocated. In this case, the input value of *BufferSize
  indicates the size of the caller-allocated output buffer. If
  the output buffer is not large enough to contain the entire
  requested output, it is filled up to the point that the output
  buffer is exhausted and EFI_WARN_BUFFER_TOO_SMALL is returned,
  and then BufferSize is returned with the size that is required
  to successfully complete the read. All other
  output parameters are returned with valid values. If the input
  value of *Buffer==NULL, it indicates the output buffer is to
  be allocated by ReadSection(). In this case, ReadSection()
  will allocate an appropriately sized buffer from boot services
  pool memory, which will be returned in *Buffer. The size of
  the new buffer is returned in *BufferSize and all other output
  parameters are returned with valid values. ReadSection() is
  callable only from TPL_NOTIFY and below. Behavior of
  ReadSection() at any EFI_TPL above TPL_NOTIFY is
  undefined.

  @param  This                Indicates the EFI_FIRMWARE_VOLUME2_PROTOCOL instance.
 
  @param NameGuid             Pointer to an EFI_GUID, which indicates the
                              file name from which the requested section
                              will be read.
  
  @param SectionType          Indicates the section type to return.
                              SectionType in conjunction with
                              SectionInstance indicates which section to
                              return.
  
  @param SectionInstance      Indicates which instance of sections
                              with a type of SectionType to return.
                              SectionType in conjunction with
                              SectionInstance indicates which
                              section to return. SectionInstance is
                              zero based.
  
  @param Buffer               Pointer to a pointer to a buffer in which the
                              section contents are returned, not including
                              the section header.
  
  @param BufferSize           Pointer to a caller-allocated UINTN. It
                              indicates the size of the memory
                              represented by Buffer.
  
  @param AuthenticationStatus Pointer to a caller-allocated
                              UINT32 in which the authentication
                              status is returned.
  
  
  @retval EFI_SUCCESS   The call completed successfully.
  
  @retval EFI_WARN_BUFFER_TOO_SMALL   The caller-allocated
                                      buffer is too small to
                                      contain the requested
                                      output. The buffer is
                                      filled and the output is
                                      truncated.
  
  @retval EFI_OUT_OF_RESOURCES  An allocation failure occurred.
  
  @retval EFI_NOT_FOUND   The requested file was not found in
                          the firmware volume. EFI_NOT_FOUND The
                          requested section was not found in the
                          specified file.
  
  @retval EFI_DEVICE_ERROR  A hardware error occurred when
                            attempting to access the firmware
                            volume.
  
  @retval EFI_ACCESS_DENIED The firmware volume is configured to
                            disallow reads. EFI_PROTOCOL_ERROR
                            The requested section was not found,
                            but the file could not be fully
                            parsed because a required
                            GUIDED_SECTION_EXTRACTION_PROTOCOL
                            was not found. It is possible the
                            requested section exists within the
                            file and could be successfully
                            extracted once the required
                            GUIDED_SECTION_EXTRACTION_PROTOCOL
                            is published.

**/
typedef
EFI_STATUS
(EFIAPI * EFI_FV_READ_SECTION)(
  IN CONST  EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
  IN CONST  EFI_GUID                      *NameGuid,
  IN        EFI_SECTION_TYPE              SectionType,
  IN        UINTN                         SectionInstance,
  IN OUT    VOID                          **Buffer,
  IN OUT    UINTN                         *BufferSize,
  OUT       UINT32                        *AuthenticationStatus
);

///
/// EFI_FV_WRITE_POLICY, two policies (unreliable write and reliable write) are defined.
///
typedef UINT32 EFI_FV_WRITE_POLICY;
#define EFI_FV_UNRELIABLE_WRITE   0x00000000
#define EFI_FV_RELIABLE_WRITE     0x00000001

//
// EFI_FV_WRITE_FILE_DATA
//
typedef struct {
  ///
  /// Pointer to a GUID, which is the file name to be written.
  ///
  EFI_GUID                *NameGuid;
  ///
  /// Indicates the type of file to be written.
  ///
  EFI_FV_FILETYPE         Type;
  ///
  /// Indicates the attributes for the file to be written.
  ///
  EFI_FV_FILE_ATTRIBUTES  FileAttributes;
  ///
  /// Pointer to a buffer containing the file to be written.
  ///
  VOID                    *Buffer;
  ///
  /// Indicates the size of the file image contained in Buffer.
  ///
  UINT32                  BufferSize;
} EFI_FV_WRITE_FILE_DATA;

/**
  Locates the requested section within a file and returns it in a buffer.

  WriteFile() is used to write one or more files to a firmware
  volume. Each file to be written is described by an
  EFI_FV_WRITE_FILE_DATA structure. The caller must ensure that
  any required alignment for all files listed in the FileData
  array is compatible with the firmware volume. Firmware volume
  capabilities can be determined using the GetVolumeAttributes()
  call. Similarly, if the WritePolicy is set to
  EFI_FV_RELIABLE_WRITE, the caller must check the firmware volume
  capabilities to ensure EFI_FV_RELIABLE_WRITE is supported by the
  firmware volume. EFI_FV_UNRELIABLE_WRITE must always be
  supported. Writing a file with a size of zero
  (FileData[n].BufferSize == 0) deletes the file from the firmware
  volume if it exists. Deleting a file must be done one at a time.
  Deleting a file as part of a multiple file write is not allowed.
  Platform Initialization Specification VOLUME 3 Shared
  Architectural Elements 84 August 21, 2006 Version 1.0
  WriteFile() is callable only from TPL_NOTIFY and below.
  Behavior of WriteFile() at any EFI_TPL above TPL_NOTIFY is
  undefined. 

  @param This           Indicates the EFI_FIRMWARE_VOLUME2_PROTOCOL instance.

  @param NumberOfFiles  Indicates the number of elements in the array pointed to by FileData

  @param WritePolicy    Indicates the level of reliability for the
                        write in the event of a power failure or
                        other system failure during the write
                        operation.
 
  @param FileData       Pointer to an array of
                        EFI_FV_WRITE_FILE_DATA. Each element of
                        FileData[] represents a file to be written.


  @retval EFI_SUCCESS             The write completed successfully.
  
  @retval EFI_OUT_OF_RESOURCES    The firmware volume does not
                                  have enough free space to
                                  storefile(s).
  
  @retval EFI_DEVICE_ERROR        A hardware error occurred when
                                  attempting to access the firmware volume.
  
  @retval EFI_WRITE_PROTECTED     The firmware volume is
                                  configured to disallow writes.
  
  @retval EFI_NOT_FOUND           A delete was requested, but the
                                  requested file was not found in the
                                  firmware volume.
  
  @retval EFI_INVALID_PARAMETER   A delete was requested with a
                                  multiple file write.
  
  @retval EFI_INVALID_PARAMETER   An unsupported WritePolicy was
                                  requested.

  @retval EFI_INVALID_PARAMETER   An unknown file type was
                                  specified.

  @retval EFI_INVALID_PARAMETER   A file system specific error
                                  has occurred.
  
**/
typedef
EFI_STATUS 
(EFIAPI * EFI_FV_WRITE_FILE)(
  IN CONST  EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
  IN        UINT32                        NumberOfFiles,
  IN        EFI_FV_WRITE_POLICY           WritePolicy,
  IN        EFI_FV_WRITE_FILE_DATA        *FileData
);


/**
  Retrieves information about the next file in the firmware volume store 
  that matches the search criteria.

  GetNextFile() is the interface that is used to search a firmware
  volume for a particular file. It is called successively until
  the desired file is located or the function returns
   EFI_NOT_FOUND. To filter uninteresting files from the output,
  the type of file to search for may be specified in FileType. For
  example, if *FileType is EFI_FV_FILETYPE_DRIVER, only files of
  this type will be returned in the output. If *FileType is
  EFI_FV_FILETYPE_ALL, no filtering of file types is done. The Key
  parameter is used to indicate a starting point of the search. If
  the buffer *Key is completely initialized to zero, the search
  re-initialized and starts at the beginning. Subsequent calls to
  GetNextFile() must maintain the value of *Key returned by the
  immediately previous call. The actual contents of *Key are
  implementation specific and no semantic content is implied.
  GetNextFile() is callable only from TPL_NOTIFY and below.
  Behavior of GetNextFile() at any EFI_TPL above TPL_NOTIFY is
  undefined. 

  @param This       Indicates the EFI_FIRMWARE_VOLUME2_PROTOCOL instance.

  @param Key        Pointer to a caller-allocated buffer that contains implementation-specific data that is
                    used to track where to begin the search for the next file. The size of the buffer must be
                    at least This->KeySize bytes long. To re-initialize the search and begin from the
                    beginning of the firmware volume, the entire buffer must be cleared to zero. Other
                    than clearing the buffer to initiate a new search, the caller must not modify the data in
                    the buffer between calls to GetNextFile().

  @param FileType   Pointer to a caller-allocated
                    EFI_FV_FILETYPE. The GetNextFile() API can
                    filter its search for files based on the
                    value of the FileType input. A *FileType
                    input of EFI_FV_FILETYPE_ALL causes
                    GetNextFile() to search for files of all
                    types. If a file is found, the file's type
                    is returned in FileType. *FileType is not
                    modified if no file is found.

  @param NameGuid   Pointer to a caller-allocated EFI_GUID. If a
                    matching file is found, the file's name is
                    returned in NameGuid. If no matching file is
                    found, *NameGuid is not modified.

  @param Attributes Pointer to a caller-allocated
                    EFI_FV_FILE_ATTRIBUTES. If a matching file
                    is found, the file's attributes are returned
                    in Attributes. If no matching file is found,
                    Attributes is not modified. Type
                    EFI_FV_FILE_ATTRIBUTES is defined in
                    ReadFile().

  @param Size       Pointer to a caller-allocated UINTN. If a
                    matching file is found, the file's size is
                    returned in *Size. If no matching file is found,
                    Size is not modified.

  @retval EFI_SUCCESS       The output parameters are filled with data
                            obtained from the first matching file that
                            was found.

  @retval FI_NOT_FOUND      No files of type FileType were found.


  @retval EFI_DEVICE_ERROR  A hardware error occurred when
                            attempting to access the firmware
                            volume.

  @retval EFI_ACCESS_DENIED The firmware volume is configured to
                            disallow reads.

   
**/
typedef
EFI_STATUS
(EFIAPI * EFI_FV_GET_NEXT_FILE)(
  IN CONST  EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
  IN OUT    VOID                          *Key,
  IN OUT    EFI_FV_FILETYPE               *FileType,
  OUT       EFI_GUID                      *NameGuid,
  OUT       EFI_FV_FILE_ATTRIBUTES        *Attributes,
  OUT       UINTN                         *Size
);

/**
  Return information about a firmware volume.

  The GetInfo() function returns information of type
  InformationType for the requested firmware volume. If the volume
  does not support the requested information type, then
  EFI_UNSUPPORTED is returned. If the buffer is not large enough
  to hold the requested structure, EFI_BUFFER_TOO_SMALL is
  returned and the BufferSize is set to the size of buffer that is
  required to make the request. The information types defined by
  this specification are required information types that all file
  systems must support.

  @param This             A pointer to the EFI_FIRMWARE_VOLUME2_PROTOCOL
                          instance that is the file handle the requested
                          information is for.
  
  @param InformationType  The type identifier for the
                          information being requested.
  
  @param BufferSize       On input, the size of Buffer. On output,
                          the amount of data returned in Buffer. In
                          both cases, the size is measured in bytes.
  
  @param Buffer           A pointer to the data buffer to return. The
                          buffer's type is indicated by InformationType.
  
  
  @retval EFI_SUCCESS           The information was retrieved.
  
  @retval EFI_UNSUPPORTED       The InformationType is not known.
  
  @retval EFI_NO_MEDIA          The device has no medium.
  
  @retval EFI_DEVICE_ERROR      The device reported an error.
  
  @retval EFI_VOLUME_CORRUPTED  The file system structures are
                                corrupted.
  
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize is too small to
                                read the current directory
                                entry. BufferSize has been
                                updated with the size needed to
                                complete the request.


**/
typedef
EFI_STATUS
(EFIAPI *EFI_FV_GET_INFO)(
  IN CONST  EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
  IN CONST  EFI_GUID                      *InformationType,
  IN OUT    UINTN                         *BufferSize,
  OUT       VOID                          *Buffer
);


/**
  Sets information about a firmware volume.

  The SetInfo() function sets information of type InformationType
  on the requested firmware volume.


  @param This             A pointer to the EFI_FIRMWARE_VOLUME2_PROTOCOL
                          instance that is the file handle the information
                          is for.

  @param InformationType  The type identifier for the
                          information being set.

  @param BufferSize       The size, in bytes, of Buffer.

  @param Buffer           A pointer to the data buffer to write. The
                          buffer's type is indicated by InformationType.

  @retval EFI_SUCCESS           The information was set.

  @retval EFI_UNSUPPORTED       The InformationType is not known.

  @retval EFI_NO_MEDIA          The device has no medium.

  @retval EFI_DEVICE_ERROR      The device reported an error.

  @retval EFI_VOLUME_CORRUPTED  The file system structures are
                                corrupted.


  @retval EFI_WRITE_PROTECTED   The media is read only.

  @retval EFI_VOLUME_FULL       The volume is full.

  @retval EFI_BAD_BUFFER_SIZE   BufferSize is smaller than the
                                size of the type indicated by
                                InformationType.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FV_SET_INFO)(
  IN CONST  EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
  IN CONST  EFI_GUID                      *InformationType,
  IN        UINTN                         BufferSize,
  IN CONST  VOID                          *Buffer
);


///
/// The Firmware Volume Protocol contains the file-level
/// abstraction to the firmware volume as well as some firmware
/// volume attribute reporting and configuration services. The
/// Firmware Volume Protocol is the interface used by all parts of
/// DXE that are not directly involved with managing the firmware
/// volume itself. This abstraction allows many varied types of
/// firmware volume implementations. A firmware volume may be a
/// flash device or it may be a file in the UEFI system partition,
/// for example. This level of firmware volume implementation
/// detail is not visible to the consumers of the Firmware Volume
/// Protocol.
///
struct _EFI_FIRMWARE_VOLUME2_PROTOCOL {
  EFI_FV_GET_ATTRIBUTES   GetVolumeAttributes;
  EFI_FV_SET_ATTRIBUTES   SetVolumeAttributes;
  EFI_FV_READ_FILE        ReadFile;
  EFI_FV_READ_SECTION     ReadSection;
  EFI_FV_WRITE_FILE       WriteFile;
  EFI_FV_GET_NEXT_FILE    GetNextFile;
  
  ///
  /// Data field that indicates the size in bytes
  /// of the Key input buffer for the
  /// GetNextFile() API.  
  ///
  UINT32                  KeySize;
  
  ///
  /// Handle of the parent firmware volume.
  ///
  EFI_HANDLE              ParentHandle;
  EFI_FV_GET_INFO         GetInfo;
  EFI_FV_SET_INFO         SetInfo;
};


extern EFI_GUID gEfiFirmwareVolume2ProtocolGuid;

#endif
