/** @file
  Disk I/O 2 protocol as defined in the UEFI 2.4 specification.

  The Disk I/O 2 protocol defines an extension to the Disk I/O protocol to enable
  non-blocking / asynchronous byte-oriented disk operation.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DISK_IO2_H__
#define __DISK_IO2_H__

#define EFI_DISK_IO2_PROTOCOL_GUID \
  { \
    0x151c8eae, 0x7f2c, 0x472c, 0x9e, 0x54, 0x98, 0x28, 0x19, 0x4f, 0x6a, 0x88 \
  }

typedef struct _EFI_DISK_IO2_PROTOCOL EFI_DISK_IO2_PROTOCOL;

/**
  The struct of Disk IO2 Token.
**/
typedef struct {
  //
  // If Event is NULL, then blocking I/O is performed.
  // If Event is not NULL and non-blocking I/O is supported, then non-blocking I/O is performed,
  // and Event will be signaled when the I/O request is completed.
  // The caller must be prepared to handle the case where the callback associated with Event occurs
  // before the original asynchronous I/O request call returns.
  //
  EFI_EVENT     Event;

  //
  // Defines whether or not the signaled event encountered an error.
  //
  EFI_STATUS    TransactionStatus;
} EFI_DISK_IO2_TOKEN;

/**
  Terminate outstanding asynchronous requests to a device.

  @param This                   Indicates a pointer to the calling context.

  @retval EFI_SUCCESS           All outstanding requests were successfully terminated.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing the cancel
                                operation.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DISK_CANCEL_EX)(
  IN EFI_DISK_IO2_PROTOCOL *This
  );

/**
  Reads a specified number of bytes from a device.

  @param This                   Indicates a pointer to the calling context.
  @param MediaId                ID of the medium to be read.
  @param Offset                 The starting byte offset on the logical block I/O device to read from.
  @param Token                  A pointer to the token associated with the transaction.
                                If this field is NULL, synchronous/blocking IO is performed.
  @param  BufferSize            The size in bytes of Buffer. The number of bytes to read from the device.
  @param  Buffer                A pointer to the destination buffer for the data.
                                The caller is responsible either having implicit or explicit ownership of the buffer.

  @retval EFI_SUCCESS           If Event is NULL (blocking I/O): The data was read correctly from the device.
                                If Event is not NULL (asynchronous I/O): The request was successfully queued for processing.
                                                                         Event will be signaled upon completion.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing the write.
  @retval EFI_NO_MEDIA          There is no medium in the device.
  @retval EFI_MEDIA_CHNAGED     The MediaId is not for the current medium.
  @retval EFI_INVALID_PARAMETER The read request contains device addresses that are not valid for the device.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DISK_READ_EX)(
  IN EFI_DISK_IO2_PROTOCOL        *This,
  IN UINT32                       MediaId,
  IN UINT64                       Offset,
  IN OUT EFI_DISK_IO2_TOKEN       *Token,
  IN UINTN                        BufferSize,
  OUT VOID                        *Buffer
  );

/**
  Writes a specified number of bytes to a device.

  @param This        Indicates a pointer to the calling context.
  @param MediaId     ID of the medium to be written.
  @param Offset      The starting byte offset on the logical block I/O device to write to.
  @param Token       A pointer to the token associated with the transaction.
                     If this field is NULL, synchronous/blocking IO is performed.
  @param BufferSize  The size in bytes of Buffer. The number of bytes to write to the device.
  @param Buffer      A pointer to the buffer containing the data to be written.

  @retval EFI_SUCCESS           If Event is NULL (blocking I/O): The data was written correctly to the device.
                                If Event is not NULL (asynchronous I/O): The request was successfully queued for processing.
                                                                         Event will be signaled upon completion.
  @retval EFI_WRITE_PROTECTED   The device cannot be written to.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing the write operation.
  @retval EFI_NO_MEDIA          There is no medium in the device.
  @retval EFI_MEDIA_CHNAGED     The MediaId is not for the current medium.
  @retval EFI_INVALID_PARAMETER The write request contains device addresses that are not valid for the device.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DISK_WRITE_EX)(
  IN EFI_DISK_IO2_PROTOCOL        *This,
  IN UINT32                       MediaId,
  IN UINT64                       Offset,
  IN OUT EFI_DISK_IO2_TOKEN       *Token,
  IN UINTN                        BufferSize,
  IN VOID                         *Buffer
  );

/**
  Flushes all modified data to the physical device.

  @param This        Indicates a pointer to the calling context.
  @param MediaId     ID of the medium to be written.
  @param Token       A pointer to the token associated with the transaction.
                     If this field is NULL, synchronous/blocking IO is performed.

  @retval EFI_SUCCESS           If Event is NULL (blocking I/O): The data was flushed successfully to the device.
                                If Event is not NULL (asynchronous I/O): The request was successfully queued for processing.
                                                                         Event will be signaled upon completion.
  @retval EFI_WRITE_PROTECTED   The device cannot be written to.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing the write operation.
  @retval EFI_NO_MEDIA          There is no medium in the device.
  @retval EFI_MEDIA_CHNAGED     The MediaId is not for the current medium.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DISK_FLUSH_EX)(
  IN EFI_DISK_IO2_PROTOCOL        *This,
  IN OUT EFI_DISK_IO2_TOKEN       *Token
  );

#define EFI_DISK_IO2_PROTOCOL_REVISION  0x00020000

///
/// This protocol is used to abstract Block I/O interfaces.
///
struct _EFI_DISK_IO2_PROTOCOL {
  ///
  /// The revision to which the disk I/O interface adheres. All future
  /// revisions must be backwards compatible. If a future version is not
  /// backwards compatible, it is not the same GUID.
  ///
  UINT64                Revision;
  EFI_DISK_CANCEL_EX    Cancel;
  EFI_DISK_READ_EX      ReadDiskEx;
  EFI_DISK_WRITE_EX     WriteDiskEx;
  EFI_DISK_FLUSH_EX     FlushDiskEx;
};

extern EFI_GUID  gEfiDiskIo2ProtocolGuid;

#endif
