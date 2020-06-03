/** @file
  If a GUID-defined section is encountered when doing section extraction,
  the PEI Foundation or the EFI_PEI_FILE_LOADER_PPI instance
  calls the appropriate instance of the GUIDed Section Extraction PPI
  to extract the section stream contained therein.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.0.

**/

#ifndef __EFI_GUIDED_SECTION_EXTRACTION_PPI_H__
#define __EFI_GUIDED_SECTION_EXTRACTION_PPI_H__

//
// Typically, protocol interface structures are identified
// by associating them with a GUID. Each instance of
// a protocol with a given GUID must have
// the same interface structure. While all instances of
// the GUIDed Section Extraction PPI must have
// the same interface structure, they do not all have
// te same GUID. The GUID that is associated with
// an instance of the GUIDed Section Extraction Protocol
// is used to correlate it with the GUIDed section type
// that it is intended to process.
//


typedef struct _EFI_PEI_GUIDED_SECTION_EXTRACTION_PPI   EFI_PEI_GUIDED_SECTION_EXTRACTION_PPI;


/**
  Processes the input section and returns the data contained therein
  along with the authentication status.

  The ExtractSection() function processes the input section and
  returns a pointer to the section contents. If the section being
  extracted does not require processing (if the section
  GuidedSectionHeader.Attributes has the
  EFI_GUIDED_SECTION_PROCESSING_REQUIRED field cleared), then
  OutputBuffer is just updated to point to the start of the
  section's contents. Otherwise, *Buffer must be allocated
  from PEI permanent memory.

  @param This                   Indicates the EFI_PEI_GUIDED_SECTION_EXTRACTION_PPI
                                instance.
  @param InputSection           Buffer containing the input GUIDed section to be
                                processed.
  @param OutputBuffer           *OutputBuffer is allocated from PEI permanent memory
                                and contains the new section stream.
  @param OutputSize             A pointer to a caller-allocated UINTN in which
                                the size of *OutputBuffer allocation is stored.
                                If the function returns anything other than
                                EFI_SUCCESS, the value of *OutputSize is undefined.
  @param AuthenticationStatus   A pointer to a caller-allocated UINT32 that indicates
                                the authentication status of the output buffer.
                                If the input section's
                                GuidedSectionHeader.Attributes field has the
                                EFI_GUIDED_SECTION_AUTH_STATUS_VALID bit as clear,
                                *AuthenticationStatus must return zero. These bits
                                reflect the status of the extraction operation.
                                If the function returns anything other than EFI_SUCCESS,
                                the value of *AuthenticationStatus is undefined.

  @retval EFI_SUCCESS           The InputSection was successfully processed and the
                                section contents were returned.
  @retval EFI_OUT_OF_RESOURCES  The system has insufficient resources to process the request.
  @retval EFI_INVALID_PARAMETER The GUID in InputSection does not match this instance of the
                                GUIDed Section Extraction PPI.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_EXTRACT_GUIDED_SECTION)(
  IN  CONST  EFI_PEI_GUIDED_SECTION_EXTRACTION_PPI *This,
  IN  CONST  VOID                                  *InputSection,
  OUT VOID                                         **OutputBuffer,
  OUT UINTN                                        *OutputSize,
  OUT UINT32                                       *AuthenticationStatus
);

///
/// If a GUID-defined section is encountered when doing section extraction,
/// the PEI Foundation or the EFI_PEI_FILE_LOADER_PPI instance
/// calls the appropriate instance of the GUIDed Section
/// Extraction PPI to extract the section stream contained
/// therein.
///
struct _EFI_PEI_GUIDED_SECTION_EXTRACTION_PPI {
  EFI_PEI_EXTRACT_GUIDED_SECTION ExtractSection;
};



#endif

