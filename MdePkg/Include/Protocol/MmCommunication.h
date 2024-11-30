/** @file
  EFI MM Communication Protocol as defined in the PI 1.5 specification.

  This protocol provides a means of communicating between drivers outside of MM and MMI
  handlers inside of MM.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _MM_COMMUNICATION_H_
#define _MM_COMMUNICATION_H_

#pragma pack(1)

///
/// To avoid confusion in interpreting frames, the communication buffer should always
/// begin with EFI_MM_COMMUNICATE_HEADER
///
typedef struct {
  ///
  /// Allows for disambiguation of the message format.
  ///
  EFI_GUID    HeaderGuid;
  ///
  /// Describes the size of Data (in bytes) and does not include the size of the header.
  ///
  UINTN       MessageLength;
  ///
  /// Designates an array of bytes that is MessageLength in size.
  ///
  UINT8       Data[1];
} EFI_MM_COMMUNICATE_HEADER;

#pragma pack()

#define EFI_MM_COMMUNICATION_PROTOCOL_GUID \
  { \
    0xc68ed8e2, 0x9dc6, 0x4cbd, { 0x9d, 0x94, 0xdb, 0x65, 0xac, 0xc5, 0xc3, 0x32 } \
  }

typedef struct _EFI_MM_COMMUNICATION_PROTOCOL EFI_MM_COMMUNICATION_PROTOCOL;

/**
  Communicates with a registered handler.

  This function provides a service to send and receive messages from a registered UEFI service.

  @param[in] This                The EFI_MM_COMMUNICATION_PROTOCOL instance.
  @param[in] CommBuffer          A pointer to the buffer to convey into MMRAM.
  @param[in] CommSize            The size of the data buffer being passed in. On exit, the size of data
                                 being returned. Zero if the handler does not wish to reply with any data.
                                 This parameter is optional and may be NULL.

  @retval EFI_SUCCESS            The message was successfully posted.
  @retval EFI_INVALID_PARAMETER  The CommBuffer was NULL.
  @retval EFI_BAD_BUFFER_SIZE    The buffer is too large for the MM implementation.
                                 If this error is returned, the MessageLength field
                                 in the CommBuffer header or the integer pointed by
                                 CommSize, are updated to reflect the maximum payload
                                 size the implementation can accommodate.
  @retval EFI_ACCESS_DENIED      The CommunicateBuffer parameter or CommSize parameter,
                                 if not omitted, are in address range that cannot be
                                 accessed by the MM environment.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MM_COMMUNICATE)(
  IN CONST EFI_MM_COMMUNICATION_PROTOCOL   *This,
  IN OUT VOID                              *CommBuffer,
  IN OUT UINTN                             *CommSize OPTIONAL
  );

///
/// EFI MM Communication Protocol provides runtime services for communicating
/// between DXE drivers and a registered MMI handler.
///
struct _EFI_MM_COMMUNICATION_PROTOCOL {
  EFI_MM_COMMUNICATE    Communicate;
};

extern EFI_GUID  gEfiMmCommunicationProtocolGuid;

#endif
