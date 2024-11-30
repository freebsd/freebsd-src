/** @file
  EFI MM Communication PPI definition.

  This PPI provides a means of communicating between drivers outside
  of MM and MMI handlers inside of MM in PEI phase.

  Copyright (c) 2010 - 2018, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef MM_COMMUNICATION_PPI_H_
#define MM_COMMUNICATION_PPI_H_

#define EFI_PEI_MM_COMMUNICATION_PPI_GUID \
  { \
    0xae933e1c, 0xcc47, 0x4e38, { 0x8f, 0xe, 0xe2, 0xf6, 0x1d, 0x26, 0x5, 0xdf } \
  }

typedef struct _EFI_PEI_MM_COMMUNICATION_PPI EFI_PEI_MM_COMMUNICATION_PPI;

/**
  Communicates with a registered handler.

  This function provides a service to send and receive messages from a registered PEI service.
  The EFI_PEI_MM_COMMUNICATION_PPI driver is responsible for doing any of the copies such that
  the data lives in PEI-service-accessible RAM.

  A given implementation of the EFI_PEI_MM_COMMUNICATION_PPI may choose to use the
  EFI_MM_CONTROL_PPI for effecting the mode transition, or it may use some other method.

  The agent invoking the communication interface must be physical/virtually 1:1 mapped.

  To avoid confusion in interpreting frames, the CommBuffer parameter should always begin with
  EFI_MM_COMMUNICATE_HEADER. The header data is mandatory for messages sent into the MM agent.

  Once inside of MM, the MM infrastructure will call all registered handlers with the same
  HandlerType as the GUID specified by HeaderGuid and the CommBuffer pointing to Data.

  This function is not reentrant.

  @param[in] This                 The EFI_PEI_MM_COMMUNICATION_PPI instance.
  @param[in] CommBuffer           Pointer to the buffer to convey into MMRAM.
  @param[in] CommSize             The size of the data buffer being passed in. On exit, the
                                  size of data being returned. Zero if the handler does not
                                  wish to reply with any data.

  @retval EFI_SUCCESS             The message was successfully posted.
  @retval EFI_INVALID_PARAMETER   The buffer was NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_MM_COMMUNICATE)(
  IN CONST EFI_PEI_MM_COMMUNICATION_PPI     *This,
  IN OUT VOID                               *CommBuffer,
  IN OUT UINTN                              *CommSize
  );

///
/// EFI MM Communication PPI provides services for communicating between PEIM and a registered
/// MMI handler.
///
struct _EFI_PEI_MM_COMMUNICATION_PPI {
  EFI_PEI_MM_COMMUNICATE    Communicate;
};

extern EFI_GUID  gEfiPeiMmCommunicationPpiGuid;

#endif
