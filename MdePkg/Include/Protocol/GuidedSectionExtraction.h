/** @file
  If a GUID-defined section is encountered when doing section
  extraction, the section extraction driver calls the appropriate
  instance of the GUIDed Section Extraction Protocol to extract
  the section stream contained therein.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference: PI
  Version 1.00.

**/

#ifndef __GUID_SECTION_EXTRACTION_PROTOCOL_H__
#define __GUID_SECTION_EXTRACTION_PROTOCOL_H__

//
// The protocol interface structures are identified by associating
// them with a GUID. Each instance of a protocol with a given
// GUID must have the same interface structure. While all instances
// of the GUIDed Section Extraction Protocol must have the same
// interface structure, they do not all have the same GUID. The
// GUID that is associated with an instance of the GUIDed Section
// Extraction Protocol is used to correlate it with the GUIDed
// section type that it is intended to process.
//

typedef struct _EFI_GUIDED_SECTION_EXTRACTION_PROTOCOL EFI_GUIDED_SECTION_EXTRACTION_PROTOCOL;

/**
  The ExtractSection() function processes the input section and
  allocates a buffer from the pool in which it returns the section
  contents. If the section being extracted contains
  authentication information (the section's
  GuidedSectionHeader.Attributes field has the
  EFI_GUIDED_SECTION_AUTH_STATUS_VALID bit set), the values
  returned in AuthenticationStatus must reflect the results of
  the authentication operation. Depending on the algorithm and
  size of the encapsulated data, the time that is required to do
  a full authentication may be prohibitively long for some
  classes of systems. To indicate this, use
  EFI_SECURITY_POLICY_PROTOCOL_GUID, which may be published by
  the security policy driver (see the Platform Initialization
  Driver Execution Environment Core Interface Specification for
  more details and the GUID definition). If the
  EFI_SECURITY_POLICY_PROTOCOL_GUID exists in the handle
  database, then, if possible, full authentication should be
  skipped and the section contents simply returned in the
  OutputBuffer. In this case, the
  EFI_AUTH_STATUS_PLATFORM_OVERRIDE bit AuthenticationStatus
  must be set on return. ExtractSection() is callable only from
  TPL_NOTIFY and below. Behavior of ExtractSection() at any
  EFI_TPL above TPL_NOTIFY is undefined. Type EFI_TPL is
  defined in RaiseTPL() in the UEFI 2.0 specification.


  @param This         Indicates the EFI_GUIDED_SECTION_EXTRACTION_PROTOCOL instance.

  @param InputSection Buffer containing the input GUIDed section
                      to be processed. OutputBuffer OutputBuffer
                      is allocated from boot services pool
                      memory and contains the new section
                      stream. The caller is responsible for
                      freeing this buffer.

  @param OutputSize   A pointer to a caller-allocated UINTN in
                      which the size of OutputBuffer allocation
                      is stored. If the function returns
                      anything other than EFI_SUCCESS, the value
                      of OutputSize is undefined.

  @param AuthenticationStatus A pointer to a caller-allocated
                              UINT32 that indicates the
                              authentication status of the
                              output buffer. If the input
                              section's
                              GuidedSectionHeader.Attributes
                              field has the
                              EFI_GUIDED_SECTION_AUTH_STATUS_VAL
                              bit as clear, AuthenticationStatus
                              must return zero. Both local bits
                              (19:16) and aggregate bits (3:0)
                              in AuthenticationStatus are
                              returned by ExtractSection().
                              These bits reflect the status of
                              the extraction operation. The bit
                              pattern in both regions must be
                              the same, as the local and
                              aggregate authentication statuses
                              have equivalent meaning at this
                              level. If the function returns
                              anything other than EFI_SUCCESS,
                              the value of AuthenticationStatus
                              is undefined.

  @retval EFI_SUCCESS           The InputSection was successfully
                                processed and the section contents were
                                returned.

  @retval EFI_OUT_OF_RESOURCES  The system has insufficient
                                resources to process the
                                request.

  @retval EFI_INVALID_PARAMETER The GUID in InputSection does
                                not match this instance of the
                                GUIDed Section Extraction
                                Protocol.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EXTRACT_GUIDED_SECTION)(
  IN CONST  EFI_GUIDED_SECTION_EXTRACTION_PROTOCOL  *This,
  IN CONST  VOID                                    *InputSection,
  OUT       VOID                                    **OutputBuffer,
  OUT       UINTN                                   *OutputSize,
  OUT       UINT32                                  *AuthenticationStatus
  );

///
/// Typically, protocol interface structures are identified by associating them with a GUID. Each
/// instance of a protocol with a given GUID must have the same interface structure. While all instances
/// of the GUIDed Section Extraction Protocol must have the same interface structure, they do not all
/// have the same GUID. The GUID that is associated with an instance of the GUIDed Section
/// Extraction Protocol is used to correlate it with the GUIDed section type that it is intended to process.
///
struct _EFI_GUIDED_SECTION_EXTRACTION_PROTOCOL {
  EFI_EXTRACT_GUIDED_SECTION    ExtractSection;
};

#endif
