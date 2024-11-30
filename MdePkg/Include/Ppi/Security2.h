/** @file
  This file declares Pei Security2 PPI.

  This PPI is installed by some platform PEIM that abstracts the security
  policy to the PEI Foundation, namely the case of a PEIM's authentication
  state being returned during the PEI section extraction process.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.0.

**/

#ifndef __SECURITY2_PPI_H__
#define __SECURITY2_PPI_H__

#define EFI_PEI_SECURITY2_PPI_GUID \
  { 0xdcd0be23, 0x9586, 0x40f4, { 0xb6, 0x43, 0x6, 0x52, 0x2c, 0xed, 0x4e, 0xde } }

typedef struct _EFI_PEI_SECURITY2_PPI EFI_PEI_SECURITY2_PPI;

/**
  Allows the platform builder to implement a security policy
  in response to varying file authentication states.

  This service is published by some platform PEIM. The purpose of
  this service is to expose a given platform's policy-based
  response to the PEI Foundation. For example, if there is a PEIM
  in a GUIDed encapsulation section and the extraction of the PEI
  file section yields an authentication failure, there is no a
  priori policy in the PEI Foundation. Specifically, this
  situation leads to the question whether PEIMs that are either
  not in GUIDed sections or are in sections whose authentication
  fails should still be executed.

  @param PeiServices             An indirect pointer to the PEI Services
                                 Table published by the PEI Foundation.
  @param This                    Interface pointer that implements the
                                 particular EFI_PEI_SECURITY2_PPI instance.
  @param AuthenticationStatus    Authentication status of the file.
                                 xx00 Image was not signed.
                                 xxx1 Platform security policy override.
                                      Assumes same meaning as 0010 (the image was signed, the
                                      signature was tested, and the signature passed authentication test).
                                 0010 Image was signed, the signature was tested,
                                      and the signature passed authentication test.
                                 0110 Image was signed and the signature was not tested.
                                 1010 Image was signed, the signature was tested,
                                      and the signature failed the authentication test.
  @param FvHandle                Handle of the volume in which the file
                                 resides. This allows different policies
                                 depending on different firmware volumes.
  @param FileHandle              Handle of the file under review.
  @param DeferExecution          Pointer to a variable that alerts the
                                 PEI Foundation to defer execution of a
                                 PEIM.

  @retval EFI_SUCCESS            The service performed its action successfully.
  @retval EFI_SECURITY_VIOLATION The object cannot be trusted.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_SECURITY_AUTHENTICATION_STATE)(
  IN CONST  EFI_PEI_SERVICES      **PeiServices,
  IN CONST  EFI_PEI_SECURITY2_PPI *This,
  IN UINT32                       AuthenticationStatus,
  IN EFI_PEI_FV_HANDLE            FvHandle,
  IN EFI_PEI_FILE_HANDLE          FileHandle,
  IN OUT    BOOLEAN               *DeferExecution
  );

///
/// This PPI is a means by which the platform builder can indicate
/// a response to a PEIM's authentication state. This can be in
/// the form of a requirement for the PEI Foundation to skip a
/// module using the DeferExecution Boolean output in the
/// AuthenticationState() member function. Alternately, the
/// Security PPI can invoke something like a cryptographic PPI
/// that hashes the PEIM contents to log attestations, for which
/// the FileHandle parameter in AuthenticationState() will be
/// useful. If this PPI does not exist, PEIMs will be considered
/// trusted.
///
struct _EFI_PEI_SECURITY2_PPI {
  EFI_PEI_SECURITY_AUTHENTICATION_STATE    AuthenticationState;
};

extern EFI_GUID  gEfiPeiSecurity2PpiGuid;

#endif
