/** @file
  Provides interface to EFI_FILE_HANDLE functionality.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _FILE_HANDLE_LIBRARY_HEADER_
#define _FILE_HANDLE_LIBRARY_HEADER_

#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>

/// The tag for use in identifying UNICODE files.
/// If the file is UNICODE, the first 16 bits of the file will equal this value.
extern CONST UINT16 gUnicodeFileTag;

/**
  This function retrieves information about the file for the handle
  specified and stores it in the allocated pool memory.

  This function allocates a buffer to store the file's information. It is the
  caller's responsibility to free the buffer.

  @param[in] FileHandle         The file handle of the file for which information is
                                being requested.

  @retval NULL                  Information could not be retrieved.
  @retval !NULL                 The information about the file.
**/
EFI_FILE_INFO*
EFIAPI
FileHandleGetInfo (
  IN EFI_FILE_HANDLE            FileHandle
  );

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
  );

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

  @param[in] FileHandle          The opened file handle.
  @param[in, out] BufferSize     On input, the size of buffer in bytes.  On return,
                                 the number of bytes written.
  @param[out] Buffer             The buffer to put read data into.

  @retval EFI_SUCCESS         Data was read.
  @retval EFI_NO_MEDIA          The device has no media.
  @retval EFI_DEVICE_ERROR  The device reported an error.
  @retval EFI_VOLUME_CORRUPTED  The file system structures are corrupted.
  @retval EFI_BUFFER_TO_SMALL Buffer is too small. ReadSize contains required
                                size.

**/
EFI_STATUS
EFIAPI
FileHandleRead(
  IN EFI_FILE_HANDLE            FileHandle,
  IN OUT UINTN                  *BufferSize,
  OUT VOID                      *Buffer
  );

/**
  Write data to a file.

  This function writes the specified number of bytes to the file at the current
  file position. The current file position is advanced the actual number of bytes
  written, which is returned in BufferSize. Partial writes only occur when there
  has been a data error during the write attempt (such as "volume space full").
  The file is automatically grown to hold the data if required. Direct writes to
  opened directories are not supported.

  @param[in] FileHandle          The opened file for writing.
  @param[in, out] BufferSize     On input, the number of bytes in Buffer.  On output,
                                 the number of bytes written.
  @param[in] Buffer              The buffer containing data to write is stored.

  @retval EFI_SUCCESS         Data was written.
  @retval EFI_UNSUPPORTED       Writes to an open directory are not supported.
  @retval EFI_NO_MEDIA          The device has no media.
  @retval EFI_DEVICE_ERROR  The device reported an error.
  @retval EFI_VOLUME_CORRUPTED  The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED The device is write-protected.
  @retval EFI_ACCESS_DENIED The file was opened for read only.
  @retval EFI_VOLUME_FULL The volume is full.
**/
EFI_STATUS
EFIAPI
FileHandleWrite(
  IN EFI_FILE_HANDLE            FileHandle,
  IN OUT UINTN                  *BufferSize,
  IN VOID                       *Buffer
  );

/**
  Close an open file handle.

  This function closes a specified file handle. All "dirty" cached file data is
  flushed to the device, and the file is closed. In all cases the handle is
  closed.

  @param[in] FileHandle           The file handle to close.

  @retval EFI_SUCCESS             The file handle was closed successfully.
**/
EFI_STATUS
EFIAPI
FileHandleClose (
  IN EFI_FILE_HANDLE            FileHandle
  );

/**
  Delete a file and close the handle.

  This function closes and deletes a file. In all cases the file handle is closed.
  If the file cannot be deleted, the warning code EFI_WARN_DELETE_FAILURE is
  returned, but the handle is still closed.

  @param[in] FileHandle             The file handle to delete.

  @retval EFI_SUCCESS               The file was closed successfully.
  @retval EFI_WARN_DELETE_FAILURE   The handle was closed, but the file was not
                                    deleted.
  @retval INVALID_PARAMETER         One of the parameters has an invalid value.
**/
EFI_STATUS
EFIAPI
FileHandleDelete (
  IN EFI_FILE_HANDLE    FileHandle
  );

/**
  Set the current position in a file.

  This function sets the current file position for the handle to the position
  supplied. With the exception of moving to position 0xFFFFFFFFFFFFFFFF, only
  absolute positioning is supported, and moving past the end of the file is
  allowed (a subsequent write would grow the file). Moving to position
  0xFFFFFFFFFFFFFFFF causes the current position to be set to the end of the file.
  If FileHandle is a directory, the only position that may be set is zero. This
  has the effect of starting the read process of the directory entries over again.

  @param[in] FileHandle         The file handle on which the position is being set.
  @param[in] Position           The byte position from the beginning of the file.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_UNSUPPORTED       The request for non-zero is not valid on
                                directories.
  @retval INVALID_PARAMETER     One of the parameters has an invalid value.
**/
EFI_STATUS
EFIAPI
FileHandleSetPosition (
  IN EFI_FILE_HANDLE    FileHandle,
  IN UINT64             Position
  );

/**
  Gets a file's current position.

  This function retrieves the current file position for the file handle. For
  directories, the current file position has no meaning outside of the file
  system driver. As such, the operation is not supported. An error is returned
  if FileHandle is a directory.

  @param[in] FileHandle         The open file handle on which to get the position.
  @param[out] Position          The byte position from beginning of file.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval INVALID_PARAMETER     One of the parameters has an invalid value.
  @retval EFI_UNSUPPORTED       The request is not valid on directories.
**/
EFI_STATUS
EFIAPI
FileHandleGetPosition (
  IN EFI_FILE_HANDLE            FileHandle,
  OUT UINT64                    *Position
  );
/**
  Flushes data on a file.

  This function flushes all modified data associated with a file to a device.

  @param[in] FileHandle         The file handle on which to flush data.

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
  );

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
  );

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
  @return Others                The status of FileHandleGetInfo, FileHandleSetPosition,
                                or FileHandleRead.
**/
EFI_STATUS
EFIAPI
FileHandleFindFirstFile (
  IN EFI_FILE_HANDLE            DirHandle,
  OUT EFI_FILE_INFO             **Buffer
  );

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

  @retval EFI_SUCCESS           Found the next file, or reached last file.
  @retval EFI_NO_MEDIA          The device has no media.
  @retval EFI_DEVICE_ERROR      The device reported an error.
  @retval EFI_VOLUME_CORRUPTED  The file system structures are corrupted.
**/
EFI_STATUS
EFIAPI
FileHandleFindNextFile(
  IN EFI_FILE_HANDLE             DirHandle,
  OUT EFI_FILE_INFO              *Buffer,
  OUT BOOLEAN                    *NoFile
  );

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
  );

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
  );

/**
  Function to get a full filename given a EFI_FILE_HANDLE somewhere lower on the
  directory 'stack'. If the file is a directory, then append the '\' char at the
  end of name string. If it's not a directory, then the last '\' should not be
  added.

  @param[in] Handle             Handle to the Directory or File to create path to.
  @param[out] FullFileName      Pointer to pointer to generated full file name.  It
                                is the responsibility of the caller to free this memory
                                with a call to FreePool().
  @retval EFI_SUCCESS           The operation was successful and FullFileName is valid.
  @retval EFI_INVALID_PARAMETER Handle was NULL.
  @retval EFI_INVALID_PARAMETER FullFileName was NULL.
  @retval EFI_OUT_OF_MEMORY     A memory allocation failed.
**/
EFI_STATUS
EFIAPI
FileHandleGetFileName (
  IN CONST EFI_FILE_HANDLE      Handle,
  OUT CHAR16                    **FullFileName
  );

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
  );

/**
  Function to read a single line from a file. The \n is not included in the returned
  buffer.  The returned buffer must be callee freed.

  If the position upon start is 0, then the Ascii Boolean will be set.  This should be
  maintained and not changed for all operations with the same file.

  @param[in]       Handle        FileHandle to read from.
  @param[in, out]  Ascii         Boolean value for indicating whether the file is
                                 Ascii (TRUE) or UCS2 (FALSE).

  @return                       The line of text from the file.

  @sa FileHandleReadLine
**/
CHAR16*
EFIAPI
FileHandleReturnLine(
  IN EFI_FILE_HANDLE            Handle,
  IN OUT BOOLEAN                *Ascii
  );

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
  );

/**
  Function to take a formatted argument and print it to a file.

  @param[in] Handle   The file handle for the file to write to.
  @param[in] Format   The format argument (see printlib for the format specifier).
  @param[in] ...      The variable arguments for the format.

  @retval EFI_SUCCESS The operation was successful.
  @retval other       A return value from FileHandleWriteLine.

  @sa FileHandleWriteLine
**/
EFI_STATUS
EFIAPI
FileHandlePrintLine(
  IN EFI_FILE_HANDLE  Handle,
  IN CONST CHAR16     *Format,
  ...
  );

/**
  Function to determine if a FILE_HANDLE is at the end of the file.

  This will NOT work on directories.

  If Handle is NULL, then ASSERT().

  @param[in] Handle     The file handle.

  @retval TRUE          The position is at the end of the file.
  @retval FALSE         The position is not at the end of the file.
**/
BOOLEAN
EFIAPI
FileHandleEof(
  IN EFI_FILE_HANDLE Handle
  );

#endif //_FILE_HANDLE_LIBRARY_HEADER_

