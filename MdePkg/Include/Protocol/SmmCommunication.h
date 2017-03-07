/** @file
  EFI SMM Communication Protocol as defined in the PI 1.2 specification.

  This protocol provides a means of communicating between drivers outside of SMM and SMI 
  handlers inside of SMM.  

  Copyright (c) 2009 - 2011, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef _SMM_COMMUNICATION_H_
#define _SMM_COMMUNICATION_H_

//
// Need include this header file for EFI_SMM_COMMUNICATE_HEADER data structure.
//
#include <Uefi/UefiAcpiDataTable.h>

#define EFI_SMM_COMMUNICATION_PROTOCOL_GUID \
  { \
    0xc68ed8e2, 0x9dc6, 0x4cbd, { 0x9d, 0x94, 0xdb, 0x65, 0xac, 0xc5, 0xc3, 0x32 } \
  }

typedef struct _EFI_SMM_COMMUNICATION_PROTOCOL  EFI_SMM_COMMUNICATION_PROTOCOL;

/**
  Communicates with a registered handler.
  
  This function provides a service to send and receive messages from a registered UEFI service.

  @param[in] This                The EFI_SMM_COMMUNICATION_PROTOCOL instance.
  @param[in] CommBuffer          A pointer to the buffer to convey into SMRAM.
  @param[in] CommSize            The size of the data buffer being passed in.On exit, the size of data
                                 being returned. Zero if the handler does not wish to reply with any data.

  @retval EFI_SUCCESS            The message was successfully posted.
  @retval EFI_INVALID_PARAMETER  The CommBuffer was NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_COMMUNICATE2)(
  IN CONST EFI_SMM_COMMUNICATION_PROTOCOL  *This,
  IN OUT VOID                              *CommBuffer,
  IN OUT UINTN                             *CommSize
  );

///
/// EFI SMM Communication Protocol provides runtime services for communicating
/// between DXE drivers and a registered SMI handler.
///
struct _EFI_SMM_COMMUNICATION_PROTOCOL {
  EFI_SMM_COMMUNICATE2  Communicate;
};

extern EFI_GUID gEfiSmmCommunicationProtocolGuid;

#endif

