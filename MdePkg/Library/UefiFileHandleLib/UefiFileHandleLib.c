/** @file
  Provides interface to EFI_FILE_HANDLE functionality.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved. <BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Protocol/SimpleFileSystem.h>
#include <Protocol/UnicodeCollation.h>

#include <Guid/FileInfo.h>

#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/FileHandleLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>

CONST UINT16 gUnicodeFileTag = EFI_UNICODE_BYTE_ORDER_MARK;

#define MAX_FILE_NAME_LEN 522 // (20 * (6+5+2))+1) unicode characters from EFI FAT spec (doubled for bytes)
#define FIND_XXXXX_FILE_BUFFER_SIZE (SIZE_OF_EFI_FILE_INFO + MAX_FILE_NAME_LEN)

/**
  This function will retrieve the information about the file for the handle
  specified and store it in allocated pool memory.

  This function allocates a buffer to store the file's information. It is the
  caller's responsibility to free the buffer

  @param  FileHandle  The file handle of the file for which information is
  being requested.

  @retval NULL information could not be retrieved.

  @return the information about the file
**/
EFI_FILE_INFO*
EFIAPI
FileHandleGetInfo (
  IN EFI_FILE_HANDLE            FileHandle
  )
{
  EFI_FILE_INFO   *FileInfo;
  UINTN           FileInfoSize;
  EFI_STATUS      Status;

  if (FileHandle == NULL) {
    return (NULL);
  }

  //
  // Get the required size to allocate
  //
  FileInfoSize = 0;
  FileInfo = NULL;
  Status = FileHandle->GetInfo(FileHandle,
                               &gEfiFileInfoGuid,
                               &FileInfoSize,
                               NULL);
  if (Status == EFI_BUFFER_TOO_SMALL){
    //
    // error is expected.  getting size to allocate
    //
    FileInfo = AllocateZeroPool(FileInfoSize);
    if (FileInfo != NULL) {
      //
      // now get the information
      //
      Status = FileHandle->GetInfo(FileHandle,
                                   &gEfiFileInfoGuid,
                                   &FileInfoSize,
                                   FileInfo);
      //
      // if we got an error free the memory and return NULL
      //
      if (EFI_ERROR(Status)) {
        FreePool(FileInfo);
        FileInfo = NULL;
      }
    }
  }
  return (FileInfo);
}

/**
  This function sets the information about the file for the opened handle
  specified.

  @param[in]  FileHandle        The file handle of the file for which information
                                is being set.

  @param[in]  FileInfo          The information to set.

  @retval EFI_SUCCESS           The information was set.
  @retval EFI_INVALID_PARAMETER A parameter was out of range or invalid.
  @retval EFI_UNSUPPORTED       The FileHandle does not support FileInfo.
  @retval EFI_NO_MEDIA          The device has no medium.
  @retval EFI_DEVICE_ERROR      The device reported an error.
  @retval EFI_VOLUME_CORRUPTED  The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED   The file or medium is write protected.
  @retval EFI_ACCESS_DENIED     The file was opened read only.
  @retval EFI_VOLUME_FULL       The volume is full.
**/
EFI_STATUS
EFIAPI
FileHandleSetInfo (
  IN EFI_FILE_HANDLE            FileHandle,
  IN CONST EFI_FILE_INFO        *FileInfo
  )
{

  if (FileHandle == NULL || FileInfo == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // Set the info
  //
  return (FileHandle->SetInfo(FileHandle,
                              &gEfiFileInfoGuid,
                              (UINTN)FileInfo->Size,
                              (EFI_FILE_INFO*)FileInfo));
}

/**
  This function reads information from an opened file.

  If FileHandle is not a directory, the function reads the requested number of
  bytes from the file at the file's current position and returns them in Buffer.
  If the read goes beyond the end of the file, the read length is truncated to the
  end of the file. The file's current position is increased by the number of bytes
  returned.  If FileHandle is a directory, the function reads the directory entry
  at the file's current position and returns the entry in Buffer. If the Buffer
  is not large enough to hold the current directory entry, then
  EFI_BUFFER_TOO_SMALL is returned and the current file position is not updated.
  BufferSize is set to be the size of the buffer needed to read the entry. On
  success, the current position is updated to the next directory entry. If there
  are no more directory entries, the read returns a zero-length buffer.
  EFI_FILE_INFO is the structure returned as the directory entry.

  @param FileHandle             the opened file handle
  @param BufferSize             on input the size of buffer in bytes.  on return
                                the number of bytes written.
  @param Buffer                 the buffer to put read data into.

  @retval EFI_SUCCESS           Data was read.
  @retval EFI_NO_MEDIA          The device has no media.
  @retval EFI_DEVICE_ERROR      The device reported an error.
  @retval EFI_VOLUME_CORRUPTED  The file system structures are corrupted.
  @retval EFI_BUFFER_TO_SMALL   Buffer is too small. ReadSize contains required
                                size.

**/
EFI_STATUS
EFIAPI
FileHandleRead(
  IN EFI_FILE_HANDLE            FileHandle,
  IN OUT UINTN                  *BufferSize,
  OUT VOID                      *Buffer
  )
{
  if (FileHandle == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // Perform the read based on EFI_FILE_PROTOCOL
  //
  return (FileHandle->Read(FileHandle, BufferSize, Buffer));
}


/**
  Write data to a file.

  This function writes the specified number of bytes to the file at the current
  file position. The current file position is advanced the actual number of bytes
  written, which is returned in BufferSize. Partial writes only occur when there
  has been a data error during the write attempt (such as "volume space full").
  The file is automatically grown to hold the data if required. Direct writes to
  opened directories are not supported.

  @param FileHandle           The opened file for writing
  @param BufferSize           on input the number of bytes in Buffer.  On output
                              the number of bytes written.
  @param Buffer               the buffer containing data to write is stored.

 @retval EFI_SUCCESS          Data was written.
 @retval EFI_UNSUPPORTED      Writes to an open directory are not supported.
 @retval EFI_NO_MEDIA         The device has no media.
 @retval EFI_DEVICE_ERROR     The device reported an error.
 @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
 @retval EFI_WRITE_PROTECTED  The device is write-protected.
 @retval EFI_ACCESS_DENIED    The file was open for read only.
 @retval EFI_VOLUME_FULL      The volume is full.
**/
EFI_STATUS
EFIAPI
FileHandleWrite(
  IN EFI_FILE_HANDLE            FileHandle,
  IN OUT UINTN                  *BufferSize,
  IN VOID                       *Buffer
  )
{
  if (FileHandle == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // Perform the write based on EFI_FILE_PROTOCOL
  //
  return (FileHandle->Write(FileHandle, BufferSize, Buffer));
}

/**
  Close an open file handle.

  This function closes a specified file handle. All "dirty" cached file data is
  flushed to the device, and the file is closed. In all cases the handle is
  closed.

@param FileHandle               the file handle to close.

@retval EFI_SUCCESS             the file handle was closed successfully.
**/
EFI_STATUS
EFIAPI
FileHandleClose (
  IN EFI_FILE_HANDLE            FileHandle
  )
{
  EFI_STATUS Status;

  if (FileHandle == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // Perform the Close based on EFI_FILE_PROTOCOL
  //
  Status = FileHandle->Close(FileHandle);
  return Status;
}

/**
  Delete a file and close the handle

  This function closes and deletes a file. In all cases the file handle is closed.
  If the file cannot be deleted, the warning code EFI_WARN_DELETE_FAILURE is
  returned, but the handle is still closed.

  @param FileHandle             the file handle to delete

  @retval EFI_SUCCESS           the file was closed successfully
  @retval EFI_WARN_DELETE_FAILURE the handle was closed, but the file was not
                                deleted
  @retval INVALID_PARAMETER     One of the parameters has an invalid value.
**/
EFI_STATUS
EFIAPI
FileHandleDelete (
  IN EFI_FILE_HANDLE    FileHandle
  )
{
  EFI_STATUS Status;

  if (FileHandle == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // Perform the Delete based on EFI_FILE_PROTOCOL
  //
  Status = FileHandle->Delete(FileHandle);
  return Status;
}

/**
  Set the current position in a file.

  This function sets the current file position for the handle to the position
  supplied. With the exception of seeking to position 0xFFFFFFFFFFFFFFFF, only
  absolute positioning is supported, and seeking past the end of the file is
  allowed (a subsequent write would grow the file). Seeking to position
  0xFFFFFFFFFFFFFFFF causes the current position to be set to the end of the file.
  If FileHandle is a directory, the only position that may be set is zero. This
  has the effect of starting the read process of the directory entries over.

  @param FileHandle             The file handle on which the position is being set
  @param Position               Byte position from beginning of file

  @retval EFI_SUCCESS           Operation completed successfully.
  @retval EFI_UNSUPPORTED       the seek request for non-zero is not valid on
                                directories.
  @retval INVALID_PARAMETER     One of the parameters has an invalid value.
**/
EFI_STATUS
EFIAPI
FileHandleSetPosition (
  IN EFI_FILE_HANDLE    FileHandle,
  IN UINT64             Position
  )
{
  if (FileHandle == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // Perform the SetPosition based on EFI_FILE_PROTOCOL
  //
  return (FileHandle->SetPosition(FileHandle, Position));
}

/**
  Gets a file's current position

  This function retrieves the current file position for the file handle. For
  directories, the current file position has no meaning outside of the file
  system driver and as such the operation is not supported. An error is returned
  if FileHandle is a directory.

  @param FileHandle             The open file handle on which to get the position.
  @param Position               Byte position from beginning of file.

  @retval EFI_SUCCESS           the operation completed successfully.
  @retval INVALID_PARAMETER     One of the parameters has an invalid value.
  @retval EFI_UNSUPPORTED       the request is not valid on directories.
**/
EFI_STATUS
EFIAPI
FileHandleGetPosition (
  IN EFI_FILE_HANDLE            FileHandle,
  OUT UINT64                    *Position
  )
{
  if (Position == NULL || FileHandle == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // Perform the GetPosition based on EFI_FILE_PROTOCOL
  //
  return (FileHandle->GetPosition(FileHandle, Position));
}
/**
  Flushes data on a file

  This function flushes all modified data associated with a file to a device.

  @param FileHandle             The file handle on which to flush data

  @retval EFI_SUCCESS           The data was flushed.
  @retval EFI_NO_MEDIA          The device has no media.
  @retval EFI_DEVICE_ERROR      The device reported an error.
  @retval EFI_VOLUME_CORRUPTED  The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED   The file or medium is write protected.
  @retval EFI_ACCESS_DENIED     The file was opened for read only.
**/
EFI_STATUS
EFIAPI
FileHandleFlush (
  IN EFI_FILE_HANDLE            FileHandle
  )
{
  if (FileHandle == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // Perform the Flush based on EFI_FILE_PROTOCOL
  //
  return (FileHandle->Flush(FileHandle));
}

/**
  Function to determine if a given handle is a directory handle.

  Open the file information on the DirHandle and verify that the Attribute
  includes EFI_FILE_DIRECTORY bit set.

  @param[in] DirHandle          Handle to open file.

  @retval EFI_SUCCESS           DirHandle is a directory.
  @retval EFI_INVALID_PARAMETER DirHandle is NULL.
                                The file information returns from FileHandleGetInfo is NULL.
  @retval EFI_NOT_FOUND         DirHandle is not a directory.
**/
EFI_STATUS
EFIAPI
FileHandleIsDirectory (
  IN EFI_FILE_HANDLE            DirHandle
  )
{
  EFI_FILE_INFO *DirInfo;

  if (DirHandle == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // get the file information for DirHandle
  //
  DirInfo = FileHandleGetInfo (DirHandle);

  //
  // Parse DirInfo
  //
  if (DirInfo == NULL) {
    //
    // We got nothing...
    //
    return (EFI_INVALID_PARAMETER);
  }
  if ((DirInfo->Attribute & EFI_FILE_DIRECTORY) == 0) {
    //
    // Attributes say this is not a directory
    //
    FreePool (DirInfo);
    return (EFI_NOT_FOUND);
  }
  //
  // all good...
  //
  FreePool (DirInfo);
  return (EFI_SUCCESS);
}

/** Retrieve first entry from a directory.

  This function takes an open directory handle and gets information from the
  first entry in the directory.  A buffer is allocated to contain
  the information and a pointer to the buffer is returned in *Buffer.  The
  caller can use FileHandleFindNextFile() to get subsequent directory entries.

  The buffer will be freed by FileHandleFindNextFile() when the last directory
  entry is read.  Otherwise, the caller must free the buffer, using FreePool,
  when finished with it.

  @param[in]  DirHandle         The file handle of the directory to search.
  @param[out] Buffer            The pointer to pointer to buffer for file's information.

  @retval EFI_SUCCESS           Found the first file.
  @retval EFI_NOT_FOUND         Cannot find the directory.
  @retval EFI_NO_MEDIA          The device has no media.
  @retval EFI_DEVICE_ERROR      The device reported an error.
  @retval EFI_VOLUME_CORRUPTED  The file system structures are corrupted.
  @return Others                status of FileHandleGetInfo, FileHandleSetPosition,
                                or FileHandleRead
**/
EFI_STATUS
EFIAPI
FileHandleFindFirstFile (
  IN EFI_FILE_HANDLE            DirHandle,
  OUT EFI_FILE_INFO             **Buffer
  )
{
  EFI_STATUS    Status;
  UINTN         BufferSize;

  if (Buffer == NULL || DirHandle == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // verify that DirHandle is a directory
  //
  Status = FileHandleIsDirectory(DirHandle);
  if (EFI_ERROR(Status)) {
    return (Status);
  }

  //
  // Allocate a buffer sized to struct size + enough for the string at the end
  //
  BufferSize = FIND_XXXXX_FILE_BUFFER_SIZE;
  *Buffer = AllocateZeroPool(BufferSize);
  if (*Buffer == NULL){
    return (EFI_OUT_OF_RESOURCES);
  }

  //
  // reset to the beginning of the directory
  //
  Status = FileHandleSetPosition(DirHandle, 0);
  if (EFI_ERROR(Status)) {
    FreePool(*Buffer);
    *Buffer = NULL;
    return (Status);
  }

  //
  // read in the info about the first file
  //
  Status = FileHandleRead (DirHandle, &BufferSize, *Buffer);
  ASSERT(Status != EFI_BUFFER_TOO_SMALL);
  if (EFI_ERROR(Status) || BufferSize == 0) {
    FreePool(*Buffer);
    *Buffer = NULL;
    if (BufferSize == 0) {
      return (EFI_NOT_FOUND);
    }
    return (Status);
  }
  return (EFI_SUCCESS);
}

/** Retrieve next entries from a directory.

  To use this function, the caller must first call the FileHandleFindFirstFile()
  function to get the first directory entry.  Subsequent directory entries are
  retrieved by using the FileHandleFindNextFile() function.  This function can
  be called several times to get each entry from the directory.  If the call of
  FileHandleFindNextFile() retrieved the last directory entry, the next call of
  this function will set *NoFile to TRUE and free the buffer.

  @param[in]  DirHandle         The file handle of the directory.
  @param[out] Buffer            The pointer to buffer for file's information.
  @param[out] NoFile            The pointer to boolean when last file is found.

  @retval EFI_SUCCESS           Found the next file, or reached last file
  @retval EFI_NO_MEDIA          The device has no media.
  @retval EFI_DEVICE_ERROR      The device reported an error.
  @retval EFI_VOLUME_CORRUPTED  The file system structures are corrupted.
**/
EFI_STATUS
EFIAPI
FileHandleFindNextFile(
  IN EFI_FILE_HANDLE          DirHandle,
  OUT EFI_FILE_INFO          *Buffer,
  OUT BOOLEAN                *NoFile
  )
{
  EFI_STATUS    Status;
  UINTN         BufferSize;

  if (DirHandle == NULL || Buffer == NULL || NoFile == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // This BufferSize MUST stay equal to the originally allocated one in GetFirstFile
  //
  BufferSize = FIND_XXXXX_FILE_BUFFER_SIZE;

  //
  // read in the info about the next file
  //
  Status = FileHandleRead (DirHandle, &BufferSize, Buffer);
  ASSERT(Status != EFI_BUFFER_TOO_SMALL);
  if (EFI_ERROR(Status)) {
    return (Status);
  }

  //
  // If we read 0 bytes (but did not have erros) we already read in the last file.
  //
  if (BufferSize == 0) {
    FreePool(Buffer);
    *NoFile = TRUE;
  }

  return (EFI_SUCCESS);
}

/**
  Retrieve the size of a file.

  This function extracts the file size info from the FileHandle's EFI_FILE_INFO
  data.

  @param[in] FileHandle         The file handle from which size is retrieved.
  @param[out] Size              The pointer to size.

  @retval EFI_SUCCESS           Operation was completed successfully.
  @retval EFI_DEVICE_ERROR      Cannot access the file.
  @retval EFI_INVALID_PARAMETER FileHandle is NULL.
                                Size is NULL.
**/
EFI_STATUS
EFIAPI
FileHandleGetSize (
  IN EFI_FILE_HANDLE            FileHandle,
  OUT UINT64                    *Size
  )
{
  EFI_FILE_INFO                 *FileInfo;

  if (FileHandle == NULL || Size == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // get the FileInfo structure
  //
  FileInfo = FileHandleGetInfo(FileHandle);
  if (FileInfo == NULL) {
    return (EFI_DEVICE_ERROR);
  }

  //
  // Assign the Size pointer to the correct value
  //
  *Size = FileInfo->FileSize;

  //
  // free the FileInfo memory
  //
  FreePool(FileInfo);

  return (EFI_SUCCESS);
}

/**
  Set the size of a file.

  This function changes the file size info from the FileHandle's EFI_FILE_INFO
  data.

  @param[in] FileHandle         The file handle whose size is to be changed.
  @param[in] Size               The new size.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_DEVICE_ERROR      Cannot access the file.
  @retval EFI_INVALID_PARAMETER FileHandle is NULL.
**/
EFI_STATUS
EFIAPI
FileHandleSetSize (
  IN EFI_FILE_HANDLE            FileHandle,
  IN UINT64                     Size
  )
{
  EFI_FILE_INFO                 *FileInfo;
  EFI_STATUS                    Status;

  if (FileHandle == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // get the FileInfo structure
  //
  FileInfo = FileHandleGetInfo(FileHandle);
  if (FileInfo == NULL) {
    return (EFI_DEVICE_ERROR);
  }

  //
  // Assign the FileSize pointer to the new value
  //
  FileInfo->FileSize = Size;

  Status = FileHandleSetInfo(FileHandle, FileInfo);
  //
  // free the FileInfo memory
  //
  FreePool(FileInfo);

  return (Status);
}

/**
  Safely append (on the left) with automatic string resizing given length of Destination and
  desired length of copy from Source.

  append the first D characters of Source to the end of Destination, where D is
  the lesser of Count and the StrLen() of Source. If appending those D characters
  will fit within Destination (whose Size is given as CurrentSize) and
  still leave room for a NULL terminator, then those characters are appended,
  starting at the original terminating NULL of Destination, and a new terminating
  NULL is appended.

  If appending D characters onto Destination will result in a overflow of the size
  given in CurrentSize the string will be grown such that the copy can be performed
  and CurrentSize will be updated to the new size.

  If Source is NULL, there is nothing to append, just return the current buffer in
  Destination.

  if Destination is NULL, then return error
  if Destination's current length (including NULL terminator) is already more then
  CurrentSize, then ASSERT()

  @param[in, out] Destination   The String to append onto
  @param[in, out] CurrentSize   on call the number of bytes in Destination.  On
                                return possibly the new size (still in bytes).  if NULL
                                then allocate whatever is needed.
  @param[in]      Source        The String to append from
  @param[in]      Count         Maximum number of characters to append.  if 0 then
                                all are appended.

  @return Destination           return the resultant string.
**/
CHAR16*
EFIAPI
StrnCatGrowLeft (
  IN OUT CHAR16           **Destination,
  IN OUT UINTN            *CurrentSize,
  IN     CONST CHAR16     *Source,
  IN     UINTN            Count
  )
{
  UINTN DestinationStartSize;
  UINTN NewSize;
  UINTN CopySize;

  if (Destination == NULL) {
    return (NULL);
  }

  //
  // If there's nothing to do then just return Destination
  //
  if (Source == NULL) {
    return (*Destination);
  }

  //
  // allow for NULL pointers address as Destination
  //
  if (*Destination != NULL) {
    ASSERT(CurrentSize != 0);
    DestinationStartSize = StrSize(*Destination);
    ASSERT(DestinationStartSize <= *CurrentSize);
  } else {
    DestinationStartSize = 0;
//    ASSERT(*CurrentSize == 0);
  }

  //
  // Append all of Source?
  //
  if (Count == 0) {
    Count = StrSize(Source);
  }

  //
  // Test and grow if required
  //
  if (CurrentSize != NULL) {
    NewSize = *CurrentSize;
    while (NewSize < (DestinationStartSize + Count)) {
      NewSize += 2 * Count;
    }
    *Destination = ReallocatePool(*CurrentSize, NewSize, *Destination);
    *CurrentSize = NewSize;
  } else {
    *Destination = AllocateZeroPool(Count+sizeof(CHAR16));
  }
  if (*Destination == NULL) {
    return NULL;
  }

  CopySize = StrSize(*Destination);
  CopyMem((*Destination)+((Count-2)/sizeof(CHAR16)), *Destination, CopySize);
  CopyMem(*Destination, Source, Count-2);
  return (*Destination);
}

/**
  Function to get a full filename given a EFI_FILE_HANDLE somewhere lower on the
  directory 'stack'. If the file is a directory, then append the '\' char at the
  end of name string. If it's not a directory, then the last '\' should not be
  added.

  if Handle is NULL, return EFI_INVALID_PARAMETER

  @param[in] Handle             Handle to the Directory or File to create path to.
  @param[out] FullFileName      pointer to pointer to generated full file name.  It
                                is the responsibility of the caller to free this memory
                                with a call to FreePool().
  @retval EFI_SUCCESS           the operation was sucessful and the FullFileName is valid.
  @retval EFI_INVALID_PARAMETER Handle was NULL.
  @retval EFI_INVALID_PARAMETER FullFileName was NULL.
  @retval EFI_OUT_OF_RESOURCES  a memory allocation failed.
**/
EFI_STATUS
EFIAPI
FileHandleGetFileName (
  IN CONST EFI_FILE_HANDLE      Handle,
  OUT CHAR16                    **FullFileName
  )
{
  EFI_STATUS      Status;
  UINTN           Size;
  EFI_FILE_HANDLE CurrentHandle;
  EFI_FILE_HANDLE NextHigherHandle;
  EFI_FILE_INFO   *FileInfo;

  Size = 0;

  //
  // Check our parameters
  //
  if (FullFileName == NULL || Handle == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  *FullFileName = NULL;
  CurrentHandle = NULL;

  Status = Handle->Open(Handle, &CurrentHandle, L".", EFI_FILE_MODE_READ, 0);
  if (!EFI_ERROR(Status)) {
    //
    // Reverse out the current directory on the device
    //
    for (;;) {
      FileInfo = FileHandleGetInfo(CurrentHandle);
      if (FileInfo == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        break;
      } else {
        //
        // Prepare to move to the parent directory.
        // Also determine whether CurrentHandle refers to the Root directory.
        //
        Status = CurrentHandle->Open (CurrentHandle, &NextHigherHandle, L"..", EFI_FILE_MODE_READ, 0);
        //
        // We got info... do we have a name? if yes precede the current path with it...
        //
        if ((StrLen (FileInfo->FileName) == 0) || EFI_ERROR (Status)) {
          //
          // Both FileInfo->FileName being '\0' and EFI_ERROR() suggest that
          // CurrentHandle refers to the Root directory.  As this loop ensures
          // FullFileName is starting with '\\' at all times, signal success
          // and exit the loop.
          // While FileInfo->FileName could theoretically be a value other than
          // '\0' or '\\', '\\' is guaranteed to be supported by the
          // specification and hence its value can safely be ignored.
          //
          Status = EFI_SUCCESS;
          if (*FullFileName == NULL) {
            ASSERT((*FullFileName == NULL && Size == 0) || (*FullFileName != NULL));
            *FullFileName = StrnCatGrowLeft(FullFileName, &Size, L"\\", 0);
          }
          FreePool(FileInfo);
          break;
        } else {
          if (*FullFileName == NULL) {
            ASSERT((*FullFileName == NULL && Size == 0) || (*FullFileName != NULL));
            *FullFileName = StrnCatGrowLeft(FullFileName, &Size, L"\\", 0);
          }
          ASSERT((*FullFileName == NULL && Size == 0) || (*FullFileName != NULL));
          *FullFileName = StrnCatGrowLeft(FullFileName, &Size, FileInfo->FileName, 0);
          *FullFileName = StrnCatGrowLeft(FullFileName, &Size, L"\\", 0);
          FreePool(FileInfo);
        }
      }

      FileHandleClose(CurrentHandle);
      //
      // Move to the parent directory
      //
      CurrentHandle = NextHigherHandle;
    }
  } else if (Status == EFI_NOT_FOUND) {
    Status = EFI_SUCCESS;
    ASSERT((*FullFileName == NULL && Size == 0) || (*FullFileName != NULL));
    *FullFileName = StrnCatGrowLeft(FullFileName, &Size, L"\\", 0);
  }

  if (*FullFileName != NULL &&
      (*FullFileName)[StrLen(*FullFileName) - 1] == L'\\' &&
      StrLen(*FullFileName) > 1 &&
      FileHandleIsDirectory(Handle) == EFI_NOT_FOUND
     ) {
    (*FullFileName)[StrLen(*FullFileName) - 1] = CHAR_NULL;
  }

  if (CurrentHandle != NULL) {
    CurrentHandle->Close (CurrentHandle);
  }

  if (EFI_ERROR(Status) && *FullFileName != NULL) {
    FreePool(*FullFileName);
  }

  return (Status);
}

/**
  Function to read a single line from a file. The \n is not included in the returned
  buffer.  The returned buffer must be callee freed.

  If the position upon start is 0, then the Ascii Boolean will be set.  This should be
  maintained and not changed for all operations with the same file.

  @param[in]       Handle        FileHandle to read from.
  @param[in, out]  Ascii         Boolean value for indicating whether the file is Ascii (TRUE) or UCS2 (FALSE);

  @return                       The line of text from the file.

  @sa FileHandleReadLine
**/
CHAR16*
EFIAPI
FileHandleReturnLine(
  IN EFI_FILE_HANDLE            Handle,
  IN OUT BOOLEAN                *Ascii
  )
{
  CHAR16          *RetVal;
  UINTN           Size;
  EFI_STATUS      Status;

  Size = 0;
  RetVal = NULL;

  Status = FileHandleReadLine(Handle, RetVal, &Size, FALSE, Ascii);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    RetVal = AllocateZeroPool(Size);
    Status = FileHandleReadLine(Handle, RetVal, &Size, FALSE, Ascii);
  }
  ASSERT_EFI_ERROR(Status);
  if (EFI_ERROR(Status) && (RetVal != NULL)) {
    FreePool(RetVal);
    RetVal = NULL;
  }
  return (RetVal);
}

/**
  Function to read a single line (up to but not including the \n) from a file.

  If the position upon start is 0, then the Ascii Boolean will be set.  This should be
  maintained and not changed for all operations with the same file.
  The function will not return the \r and \n character in buffer. When an empty line is
  read a CHAR_NULL character will be returned in buffer.

  @param[in]       Handle        FileHandle to read from.
  @param[in, out]  Buffer        The pointer to buffer to read into.
  @param[in, out]  Size          The pointer to number of bytes in Buffer.
  @param[in]       Truncate      If the buffer is large enough, this has no effect.
                                 If the buffer is is too small and Truncate is TRUE,
                                 the line will be truncated.
                                 If the buffer is is too small and Truncate is FALSE,
                                 then no read will occur.

  @param[in, out]  Ascii         Boolean value for indicating whether the file is
                                 Ascii (TRUE) or UCS2 (FALSE).

  @retval EFI_SUCCESS           The operation was successful.  The line is stored in
                                Buffer.
  @retval EFI_INVALID_PARAMETER Handle was NULL.
  @retval EFI_INVALID_PARAMETER Size was NULL.
  @retval EFI_BUFFER_TOO_SMALL  Size was not large enough to store the line.
                                Size was updated to the minimum space required.
  @sa FileHandleRead
**/
EFI_STATUS
EFIAPI
FileHandleReadLine(
  IN EFI_FILE_HANDLE            Handle,
  IN OUT CHAR16                 *Buffer,
  IN OUT UINTN                  *Size,
  IN BOOLEAN                    Truncate,
  IN OUT BOOLEAN                *Ascii
  )
{
  EFI_STATUS  Status;
  CHAR16      CharBuffer;
  UINT64      FileSize;
  UINTN       CharSize;
  UINTN       CountSoFar;
  UINTN       CrCount;
  UINT64      OriginalFilePosition;

  if (Handle == NULL
    ||Size   == NULL
    ||(Buffer==NULL&&*Size!=0)
   ){
    return (EFI_INVALID_PARAMETER);
  }

  if (Buffer != NULL && *Size != 0) {
    *Buffer = CHAR_NULL;
  }

  Status = FileHandleGetSize (Handle, &FileSize);
  if (EFI_ERROR (Status)) {
    return Status;
  } else if (FileSize == 0) {
    *Ascii = TRUE;
    return EFI_SUCCESS;
  }

  FileHandleGetPosition(Handle, &OriginalFilePosition);
  if (OriginalFilePosition == 0) {
    CharSize = sizeof(CHAR16);
    Status = FileHandleRead(Handle, &CharSize, &CharBuffer);
    ASSERT_EFI_ERROR(Status);
    if (CharBuffer == gUnicodeFileTag) {
      *Ascii = FALSE;
    } else {
      *Ascii = TRUE;
      FileHandleSetPosition(Handle, OriginalFilePosition);
    }
  }

  CrCount = 0;
  for (CountSoFar = 0;;CountSoFar++){
    CharBuffer = 0;
    if (*Ascii) {
      CharSize = sizeof(CHAR8);
    } else {
      CharSize = sizeof(CHAR16);
    }
    Status = FileHandleRead(Handle, &CharSize, &CharBuffer);
    if (  EFI_ERROR(Status)
       || CharSize == 0
       || (CharBuffer == L'\n' && !(*Ascii))
       || (CharBuffer ==  '\n' && *Ascii)
     ){
      break;
    } else if (
        (CharBuffer == L'\r' && !(*Ascii)) ||
        (CharBuffer ==  '\r' && *Ascii)
      ) {
      CrCount++;
      continue;
    }
    //
    // if we have space save it...
    //
    if ((CountSoFar+1-CrCount)*sizeof(CHAR16) < *Size){
      ASSERT(Buffer != NULL);
      ((CHAR16*)Buffer)[CountSoFar-CrCount] = CharBuffer;
      ((CHAR16*)Buffer)[CountSoFar+1-CrCount] = CHAR_NULL;
    }
  }

  //
  // if we ran out of space tell when...
  //
  if ((CountSoFar+1-CrCount)*sizeof(CHAR16) > *Size){
    *Size = (CountSoFar+1-CrCount)*sizeof(CHAR16);
    if (!Truncate) {
      if (Buffer != NULL && *Size != 0) {
        ZeroMem(Buffer, *Size);
      }
      FileHandleSetPosition(Handle, OriginalFilePosition);
      return (EFI_BUFFER_TOO_SMALL);
    } else {
      DEBUG((DEBUG_WARN, "The line was truncated in FileHandleReadLine"));
      return (EFI_SUCCESS);
    }
  }

  return (Status);
}

/**
  Function to write a line of text to a file.

  If the file is a Unicode file (with UNICODE file tag) then write the unicode
  text.
  If the file is an ASCII file then write the ASCII text.
  If the size of file is zero (without file tag at the beginning) then write
  ASCII text as default.

  @param[in]     Handle         FileHandle to write to.
  @param[in]     Buffer         Buffer to write, if NULL the function will
                                take no action and return EFI_SUCCESS.

  @retval  EFI_SUCCESS            The data was written.
                                  Buffer is NULL.
  @retval  EFI_INVALID_PARAMETER  Handle is NULL.
  @retval  EFI_OUT_OF_RESOURCES   Unable to allocate temporary space for ASCII
                                  string due to out of resources.

  @sa FileHandleWrite
**/
EFI_STATUS
EFIAPI
FileHandleWriteLine(
  IN EFI_FILE_HANDLE Handle,
  IN CHAR16          *Buffer
  )
{
  EFI_STATUS  Status;
  CHAR16      CharBuffer;
  UINTN       Size;
  UINTN       Index;
  UINTN       CharSize;
  UINT64      FileSize;
  UINT64      OriginalFilePosition;
  BOOLEAN     Ascii;
  CHAR8       *AsciiBuffer;

  if (Buffer == NULL) {
    return (EFI_SUCCESS);
  }

  if (Handle == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  Ascii = FALSE;
  AsciiBuffer = NULL;

  Status = FileHandleGetPosition(Handle, &OriginalFilePosition);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Status = FileHandleSetPosition(Handle, 0);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Status = FileHandleGetSize(Handle, &FileSize);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  if (FileSize == 0) {
    Ascii = TRUE;
  } else {
    CharSize = sizeof (CHAR16);
    Status = FileHandleRead (Handle, &CharSize, &CharBuffer);
    ASSERT_EFI_ERROR (Status);
    if (CharBuffer == gUnicodeFileTag) {
      Ascii = FALSE;
    } else {
      Ascii = TRUE;
    }
  }

  Status = FileHandleSetPosition(Handle, OriginalFilePosition);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  if (Ascii) {
    Size = ( StrSize(Buffer) / sizeof(CHAR16) ) * sizeof(CHAR8);
    AsciiBuffer = (CHAR8 *)AllocateZeroPool(Size);
    if (AsciiBuffer == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    UnicodeStrToAsciiStrS (Buffer, AsciiBuffer, Size);
    for (Index = 0; Index < Size; Index++) {
      if ((AsciiBuffer[Index] & BIT7) != 0) {
        FreePool(AsciiBuffer);
        return EFI_INVALID_PARAMETER;
      }
    }

    Size = AsciiStrSize(AsciiBuffer) - sizeof(CHAR8);
    Status = FileHandleWrite(Handle, &Size, AsciiBuffer);
    if (EFI_ERROR(Status)) {
      FreePool (AsciiBuffer);
      return (Status);
    }
    Size = AsciiStrSize("\r\n") - sizeof(CHAR8);
    Status = FileHandleWrite(Handle, &Size, "\r\n");
  } else {
    if (OriginalFilePosition == 0) {
      Status = FileHandleSetPosition (Handle, sizeof(CHAR16));
      if (EFI_ERROR(Status)) {
        return Status;
      }
    }
    Size = StrSize(Buffer) - sizeof(CHAR16);
    Status = FileHandleWrite(Handle, &Size, Buffer);
    if (EFI_ERROR(Status)) {
      return (Status);
    }
    Size = StrSize(L"\r\n") - sizeof(CHAR16);
    Status = FileHandleWrite(Handle, &Size, L"\r\n");
  }

  if (AsciiBuffer != NULL) {
    FreePool (AsciiBuffer);
  }
  return Status;
}

/**
  function to take a formatted argument and print it to a file.

  @param[in] Handle   the file handle for the file to write to
  @param[in] Format   the format argument (see printlib for format specifier)
  @param[in] ...      the variable arguments for the format

  @retval EFI_SUCCESS the operation was successful
  @return other       a return value from FileHandleWriteLine

  @sa FileHandleWriteLine
**/
EFI_STATUS
EFIAPI
FileHandlePrintLine(
  IN EFI_FILE_HANDLE  Handle,
  IN CONST CHAR16     *Format,
  ...
  )
{
  VA_LIST           Marker;
  CHAR16            *Buffer;
  EFI_STATUS        Status;

  //
  // Get a buffer to print into
  //
  Buffer = AllocateZeroPool (PcdGet16 (PcdUefiFileHandleLibPrintBufferSize));
  if (Buffer == NULL) {
    return (EFI_OUT_OF_RESOURCES);
  }

  //
  // Print into our buffer
  //
  VA_START (Marker, Format);
  UnicodeVSPrint (Buffer, PcdGet16 (PcdUefiFileHandleLibPrintBufferSize), Format, Marker);
  VA_END (Marker);

  //
  // Print buffer into file
  //
  Status = FileHandleWriteLine(Handle, Buffer);

  //
  // Cleanup and return
  //
  FreePool(Buffer);
  return (Status);
}

/**
  Function to determine if a FILE_HANDLE is at the end of the file.

  This will NOT work on directories.

  If Handle is NULL, then return False.

  @param[in] Handle     the file handle

  @retval TRUE          the position is at the end of the file
  @retval FALSE         the position is not at the end of the file
**/
BOOLEAN
EFIAPI
FileHandleEof(
  IN EFI_FILE_HANDLE Handle
  )
{
  EFI_FILE_INFO *Info;
  UINT64        Pos;
  BOOLEAN       RetVal;

  if (Handle == NULL) {
    return (FALSE);
  }

  FileHandleGetPosition(Handle, &Pos);
  Info = FileHandleGetInfo (Handle);

  if (Info == NULL) {
    return (FALSE);
  }

  FileHandleSetPosition(Handle, Pos);

  if (Pos == Info->FileSize) {
    RetVal = TRUE;
  } else {
    RetVal = FALSE;
  }

  FreePool (Info);

  return (RetVal);
}
