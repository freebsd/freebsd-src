/** @file
  EFI_TAPE_IO_PROTOCOL as defined in the UEFI 2.0.
  Provide services to control and access a tape device.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __EFI_TAPE_IO_PROTOCOL_H__
#define __EFI_TAPE_IO_PROTOCOL_H__

#define EFI_TAPE_IO_PROTOCOL_GUID \
  { \
    0x1e93e633, 0xd65a, 0x459e, {0xab, 0x84, 0x93, 0xd9, 0xec, 0x26, 0x6d, 0x18 } \
  }

typedef struct _EFI_TAPE_IO_PROTOCOL EFI_TAPE_IO_PROTOCOL;

typedef struct _EFI_TAPE_HEADER {
  UINT64     Signature;
  UINT32     Revision;
  UINT32     BootDescSize;
  UINT32     BootDescCRC;
  EFI_GUID   TapeGUID;
  EFI_GUID   TapeType;
  EFI_GUID   TapeUnique;
  UINT32     BLLocation;
  UINT32     BLBlocksize;
  UINT32     BLFilesize;
  CHAR8      OSVersion[40];
  CHAR8      AppVersion[40];
  CHAR8      CreationDate[10];
  CHAR8      CreationTime[10];
  CHAR8      SystemName[256];  // UTF-8
  CHAR8      TapeTitle[120];   // UTF-8
  CHAR8      pad[468];         // pad to 1024
} EFI_TAPE_HEADER;

/**
  Reads from the tape.

  @param  This       A pointer to the EFI_TAPE_IO_PROTOCOL instance.
  @param  BufferSize The size of the buffer in bytes pointed to by Buffer.
  @param  Buffer     The pointer to the buffer for data to be read into.

  @retval EFI_SUCCESS           Data was successfully transferred from the media.
  @retval EFI_END_OF_FILE       A filemark was encountered which limited the data
                                transferred by the read operation or the head is positioned
                                just after a filemark.
  @retval EFI_NO_MEDIA          No media is loaded in the device.
  @retval EFI_NOT_READY         The transfer failed since the device was not ready (e.g. not
                                online). The transfer may be retried at a later time.
  @retval EFI_UNSUPPORTED       The device does not support this type of transfer.
  @retval EFI_TIMEOUT           The transfer failed to complete within the timeout specified.
  @retval EFI_MEDIA_CHANGED     The media in the device was changed since the last access.
                                The transfer was aborted since the current position of the
                                media may be incorrect.
  @retval EFI_INVALID_PARAMETER A NULL Buffer was specified with a non-zero
                                BufferSize, or the device is operating in fixed block
                                size mode and the BufferSize was not a multiple of
                                device's fixed block size
  @retval EFI_DEVICE_ERROR      A device error occurred while attempting to transfer data
                                from the media.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TAPE_READ)(
  IN EFI_TAPE_IO_PROTOCOL *This,
  IN OUT UINTN            *BufferSize,
  OUT VOID                *Buffer
  );

/**
  Writes to the tape.

  @param  This       A pointer to the EFI_TAPE_IO_PROTOCOL instance.
  @param  BufferSize Size of the buffer in bytes pointed to by Buffer.
  @param  Buffer     The pointer to the buffer for data to be written from.

  @retval EFI_SUCCESS           Data was successfully transferred to the media.
  @retval EFI_END_OF_MEDIA      The logical end of media has been reached. Data may have
                                been successfully transferred to the media.
  @retval EFI_NO_MEDIA          No media is loaded in the device.
  @retval EFI_NOT_READY         The transfer failed since the device was not ready (e.g. not
                                online). The transfer may be retried at a later time.
  @retval EFI_UNSUPPORTED       The device does not support this type of transfer.
  @retval EFI_TIMEOUT           The transfer failed to complete within the timeout specified.
  @retval EFI_MEDIA_CHANGED     The media in the device was changed since the last access.
                                The transfer was aborted since the current position of the
                                media may be incorrect.
  @retval EFI_WRITE_PROTECTED   The media in the device is write-protected. The transfer
                                was aborted since a write cannot be completed.
  @retval EFI_INVALID_PARAMETER A NULL Buffer was specified with a non-zero
                                BufferSize, or the device is operating in fixed block
                                size mode and the BufferSize was not a multiple of
                                device's fixed block size
  @retval EFI_DEVICE_ERROR      A device error occurred while attempting to transfer data
                                from the media.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TAPE_WRITE)(
  IN EFI_TAPE_IO_PROTOCOL *This,
  IN UINTN                *BufferSize,
  IN VOID                 *Buffer
  );


/**
  Rewinds the tape.

  @param  This A pointer to the EFI_TAPE_IO_PROTOCOL instance.

  @retval EFI_SUCCESS      The media was successfully repositioned.
  @retval EFI_NO_MEDIA     No media is loaded in the device.
  @retval EFI_NOT_READY    Repositioning the media failed since the device was not
                           ready (e.g. not online). The transfer may be retried at a later time.
  @retval EFI_UNSUPPORTED  The device does not support this type of media repositioning.
  @retval EFI_TIMEOUT      Repositioning of the media did not complete within the timeout specified.
  @retval EFI_DEVICE_ERROR A device error occurred while attempting to reposition the media.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TAPE_REWIND)(
  IN EFI_TAPE_IO_PROTOCOL *This
  );


/**
  Positions the tape.

  @param  This      A pointer to the EFI_TAPE_IO_PROTOCOL instance.
  @param  Direction Direction and number of data blocks or filemarks to space over on media.
  @param  Type      Type of mark to space over on media.
                    The following Type marks are mandatory:
                    BLOCK type    : 0
                    FILEMARK type : 1

  @retval EFI_SUCCESS       The media was successfully repositioned.
  @retval EFI_END_OF_MEDIA  Beginning or end of media was reached before the
                            indicated number of data blocks or filemarks were found.
  @retval EFI_NO_MEDIA      No media is loaded in the device.
  @retval EFI_NOT_READY     The reposition failed since the device was not ready (e.g. not
                            online). The reposition may be retried at a later time.
  @retval EFI_UNSUPPORTED   The device does not support this type of repositioning.
  @retval EFI_TIMEOUT       The repositioning failed to complete within the timeout specified.
  @retval EFI_MEDIA_CHANGED The media in the device was changed since the last access.
                            Repositioning the media was aborted since the current
                            position of the media may be incorrect.
  @retval EFI_DEVICE_ERROR  A device error occurred while attempting to reposition the media.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TAPE_SPACE)(
  IN EFI_TAPE_IO_PROTOCOL *This,
  IN INTN                 Direction,
  IN UINTN                Type
  );


/**
  Writes filemarks to the media.

  @param  This  A pointer to the EFI_TAPE_IO_PROTOCOL instance.
  @param  Count Number of filemarks to write to the media.

  @retval EFI_SUCCESS       Data was successfully transferred from the media.
  @retval EFI_NO_MEDIA      No media is loaded in the device.
  @retval EFI_NOT_READY     The transfer failed since the device was not ready (e.g. not
                            online). The transfer may be retried at a later time.
  @retval EFI_UNSUPPORTED   The device does not support this type of repositioning.
  @retval EFI_TIMEOUT       The transfer failed to complete within the timeout specified.
  @retval EFI_MEDIA_CHANGED The media in the device was changed since the last access.
                            The transfer was aborted since the current position of the
                            media may be incorrect.
  @retval EFI_DEVICE_ERROR  A device error occurred while attempting to transfer data from the media.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TAPE_WRITEFM)(
  IN EFI_TAPE_IO_PROTOCOL *This,
  IN UINTN                Count
  );


/**
  Resets the tape device.

  @param  This                 A pointer to the EFI_TAPE_IO_PROTOCOL instance.
  @param  ExtendedVerification Indicates whether the parent bus should also be reset.

  @retval  EFI_SUCCESS      The bus and/or device were successfully reset.
  @retval  EFI_NO_MEDIA     No media is loaded in the device.
  @retval  EFI_NOT_READY    The reset failed since the device and/or bus was not ready.
                            The reset may be retried at a later time.
  @retval  EFI_UNSUPPORTED  The device does not support this type of reset.
  @retval  EFI_TIMEOUT      The reset did not complete within the timeout allowed.
  @retval  EFI_DEVICE_ERROR A device error occurred while attempting to reset the bus and/or device.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TAPE_RESET)(
  IN EFI_TAPE_IO_PROTOCOL *This,
  IN BOOLEAN              ExtendedVerification
  );

///
/// The EFI_TAPE_IO_PROTOCOL provides basic sequential operations for tape devices.
/// These include read, write, rewind, space, write filemarks and reset functions.
/// Per this specification, a boot application uses the services of this protocol
/// to load the bootloader image from tape.
///
struct _EFI_TAPE_IO_PROTOCOL {
  EFI_TAPE_READ           TapeRead;
  EFI_TAPE_WRITE          TapeWrite;
  EFI_TAPE_REWIND         TapeRewind;
  EFI_TAPE_SPACE          TapeSpace;
  EFI_TAPE_WRITEFM        TapeWriteFM;
  EFI_TAPE_RESET          TapeReset;
};

extern EFI_GUID gEfiTapeIoProtocolGuid;

#endif
