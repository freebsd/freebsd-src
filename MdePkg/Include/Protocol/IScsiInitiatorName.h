/** @file
  EFI_ISCSI_INITIATOR_NAME_PROTOCOL as defined in UEFI 2.0.
  It provides the ability to get and set the iSCSI Initiator Name.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ISCSI_INITIATOR_NAME_H__
#define __ISCSI_INITIATOR_NAME_H__

#define EFI_ISCSI_INITIATOR_NAME_PROTOCOL_GUID \
{ \
  0x59324945, 0xec44, 0x4c0d, {0xb1, 0xcd, 0x9d, 0xb1, 0x39, 0xdf, 0x7, 0xc } \
}

typedef struct _EFI_ISCSI_INITIATOR_NAME_PROTOCOL EFI_ISCSI_INITIATOR_NAME_PROTOCOL;

/**
  Retrieves the current set value of iSCSI Initiator Name.

  @param  This       Pointer to the EFI_ISCSI_INITIATOR_NAME_PROTOCOL instance.
  @param  BufferSize Size of the buffer in bytes pointed to by Buffer / Actual size of the
                     variable data buffer.
  @param  Buffer     Pointer to the buffer for data to be read. The data is a null-terminated UTF-8 encoded string.
                     The maximum length is 223 characters, including the null-terminator.

  @retval EFI_SUCCESS           Data was successfully retrieved into the provided buffer and the
                                BufferSize was sufficient to handle the iSCSI initiator name
  @retval EFI_BUFFER_TOO_SMALL  BufferSize is too small for the result.
  @retval EFI_INVALID_PARAMETER BufferSize or Buffer is NULL.
  @retval EFI_DEVICE_ERROR      The iSCSI initiator name could not be retrieved due to a hardware error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_ISCSI_INITIATOR_NAME_GET)(
  IN EFI_ISCSI_INITIATOR_NAME_PROTOCOL *This,
  IN OUT UINTN                         *BufferSize,
  OUT VOID                             *Buffer
  );

/**
  Sets the iSCSI Initiator Name.

  @param  This       Pointer to the EFI_ISCSI_INITIATOR_NAME_PROTOCOL instance.
  @param  BufferSize Size of the buffer in bytes pointed to by Buffer.
  @param  Buffer     Pointer to the buffer for data to be written. The data is a null-terminated UTF-8 encoded string.
                     The maximum length is 223 characters, including the null-terminator.

  @retval EFI_SUCCESS           Data was successfully stored by the protocol.
  @retval EFI_UNSUPPORTED       Platform policies do not allow for data to be written.
  @retval EFI_INVALID_PARAMETER BufferSize or Buffer is NULL, or BufferSize exceeds the maximum allowed limit.
  @retval EFI_DEVICE_ERROR      The data could not be stored due to a hardware error.
  @retval EFI_OUT_OF_RESOURCES  Not enough storage is available to hold the data.
  @retval EFI_PROTOCOL_ERROR    Input iSCSI initiator name does not adhere to RFC 3720
                                (and other related protocols)

**/
typedef EFI_STATUS
(EFIAPI *EFI_ISCSI_INITIATOR_NAME_SET)(
  IN EFI_ISCSI_INITIATOR_NAME_PROTOCOL *This,
  IN OUT UINTN                         *BufferSize,
  IN VOID                              *Buffer
  );

///
/// iSCSI Initiator Name Protocol for setting and obtaining the iSCSI Initiator Name.
///
struct _EFI_ISCSI_INITIATOR_NAME_PROTOCOL {
  EFI_ISCSI_INITIATOR_NAME_GET    Get;
  EFI_ISCSI_INITIATOR_NAME_SET    Set;
};

extern EFI_GUID  gEfiIScsiInitiatorNameProtocolGuid;

#endif
