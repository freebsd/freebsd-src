/** @file
  This library provides common functions to process the different guided section data.

  This library provides functions to process GUIDed sections of FFS files.  Handlers may
  be registered to decode GUIDed sections of FFS files.  Services are provided to determine
  the set of supported section GUIDs, collection information about a specific GUIDed section,
  and decode a specific GUIDed section.

  A library instance that produces this library class may be used to produce a
  EFI_PEI_GUIDED_SECTION_EXTRACTION_PPI or a EFI_GUIDED_SECTION_EXTRACTION_PROTOCOL
  providing a simple method to extend the number of GUIDed sections types a platform supports.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#ifndef __EXTRACT_GUIDED_SECTION_H__
#define __EXTRACT_GUIDED_SECTION_H__

/**
  Examines a GUIDed section and returns the size of the decoded buffer and the
  size of an optional scratch buffer required to actually decode the data in a GUIDed section.

  Examines a GUIDed section specified by InputSection.
  If GUID for InputSection does not match the GUID that this handler supports,
  then RETURN_UNSUPPORTED is returned.
  If the required information can not be retrieved from InputSection,
  then RETURN_INVALID_PARAMETER is returned.
  If the GUID of InputSection does match the GUID that this handler supports,
  then the size required to hold the decoded buffer is returned in OututBufferSize,
  the size of an optional scratch buffer is returned in ScratchSize, and the Attributes field
  from EFI_GUID_DEFINED_SECTION header of InputSection is returned in SectionAttribute.

  If InputSection is NULL, then ASSERT().
  If OutputBufferSize is NULL, then ASSERT().
  If ScratchBufferSize is NULL, then ASSERT().
  If SectionAttribute is NULL, then ASSERT().


  @param[in]  InputSection       A pointer to a GUIDed section of an FFS formatted file.
  @param[out] OutputBufferSize   A pointer to the size, in bytes, of an output buffer required
                                 if the buffer specified by InputSection were decoded.
  @param[out] ScratchBufferSize  A pointer to the size, in bytes, required as scratch space
                                 if the buffer specified by InputSection were decoded.
  @param[out] SectionAttribute   A pointer to the attributes of the GUIDed section. See the Attributes
                                 field of EFI_GUID_DEFINED_SECTION in the PI Specification.

  @retval  RETURN_SUCCESS            The information about InputSection was returned.
  @retval  RETURN_UNSUPPORTED        The section specified by InputSection does not match the GUID this handler supports.
  @retval  RETURN_INVALID_PARAMETER  The information can not be retrieved from the section specified by InputSection.

**/
typedef
RETURN_STATUS
(EFIAPI *EXTRACT_GUIDED_SECTION_GET_INFO_HANDLER)(
  IN CONST  VOID    *InputSection,
  OUT       UINT32  *OutputBufferSize,
  OUT       UINT32  *ScratchBufferSize,
  OUT       UINT16  *SectionAttribute
  );

/**
  Decodes a GUIDed section into a caller allocated output buffer.

  Decodes the GUIDed section specified by InputSection.
  If GUID for InputSection does not match the GUID that this handler supports, then RETURN_UNSUPPORTED is returned.
  If the data in InputSection can not be decoded, then RETURN_INVALID_PARAMETER is returned.
  If the GUID of InputSection does match the GUID that this handler supports, then InputSection
  is decoded into the buffer specified by OutputBuffer and the authentication status of this
  decode operation is returned in AuthenticationStatus.  If the decoded buffer is identical to the
  data in InputSection, then OutputBuffer is set to point at the data in InputSection.  Otherwise,
  the decoded data will be placed in caller allocated buffer specified by OutputBuffer.

  If InputSection is NULL, then ASSERT().
  If OutputBuffer is NULL, then ASSERT().
  If ScratchBuffer is NULL and this decode operation requires a scratch buffer, then ASSERT().
  If AuthenticationStatus is NULL, then ASSERT().


  @param[in]  InputSection  A pointer to a GUIDed section of an FFS formatted file.
  @param[out] OutputBuffer  A pointer to a buffer that contains the result of a decode operation.
  @param[out] ScratchBuffer A caller allocated buffer that may be required by this function
                            as a scratch buffer to perform the decode operation.
  @param[out] AuthenticationStatus
                            A pointer to the authentication status of the decoded output buffer.
                            See the definition of authentication status in the EFI_PEI_GUIDED_SECTION_EXTRACTION_PPI
                            section of the PI Specification. EFI_AUTH_STATUS_PLATFORM_OVERRIDE must
                            never be set by this handler.

  @retval  RETURN_SUCCESS            The buffer specified by InputSection was decoded.
  @retval  RETURN_UNSUPPORTED        The section specified by InputSection does not match the GUID this handler supports.
  @retval  RETURN_INVALID_PARAMETER  The section specified by InputSection can not be decoded.

**/
typedef
RETURN_STATUS
(EFIAPI *EXTRACT_GUIDED_SECTION_DECODE_HANDLER)(
  IN CONST  VOID    *InputSection,
  OUT       VOID    **OutputBuffer,
  IN        VOID    *ScratchBuffer,        OPTIONAL
  OUT       UINT32  *AuthenticationStatus
  );

/**
  Registers handlers of type EXTRACT_GUIDED_SECTION_GET_INFO_HANDLER and EXTRACT_GUIDED_SECTION_DECODE_HANDLER
  for a specific GUID section type.

  Registers the handlers specified by GetInfoHandler and DecodeHandler with the GUID specified by SectionGuid.
  If the GUID value specified by SectionGuid has already been registered, then return RETURN_ALREADY_STARTED.
  If there are not enough resources available to register the handlers  then RETURN_OUT_OF_RESOURCES is returned.

  If SectionGuid is NULL, then ASSERT().
  If GetInfoHandler is NULL, then ASSERT().
  If DecodeHandler is NULL, then ASSERT().

  @param[in]  SectionGuid    A pointer to the GUID associated with the the handlers
                             of the GUIDed section type being registered.
  @param[in]  GetInfoHandler Pointer to a function that examines a GUIDed section and returns the
                             size of the decoded buffer and the size of an optional scratch buffer
                             required to actually decode the data in a GUIDed section.
  @param[in]  DecodeHandler  Pointer to a function that decodes a GUIDed section into a caller
                             allocated output buffer.

  @retval  RETURN_SUCCESS           The handlers were registered.
  @retval  RETURN_OUT_OF_RESOURCES  There are not enough resources available to register the handlers.

**/
RETURN_STATUS
EFIAPI
ExtractGuidedSectionRegisterHandlers (
  IN CONST  GUID                                     *SectionGuid,
  IN        EXTRACT_GUIDED_SECTION_GET_INFO_HANDLER  GetInfoHandler,
  IN        EXTRACT_GUIDED_SECTION_DECODE_HANDLER    DecodeHandler
  );

/**
  Retrieve the list GUIDs that have been registered through ExtractGuidedSectionRegisterHandlers().

  Sets ExtractHandlerGuidTable so it points at a callee allocated array of registered GUIDs.
  The total number of GUIDs in the array are returned. Since the array of GUIDs is callee allocated
  and caller must treat this array of GUIDs as read-only data.
  If ExtractHandlerGuidTable is NULL, then ASSERT().

  @param[out]  ExtractHandlerGuidTable  A pointer to the array of GUIDs that have been registered through
                                        ExtractGuidedSectionRegisterHandlers().

  @return the number of the supported extract guided Handler.

**/
UINTN
EFIAPI
ExtractGuidedSectionGetGuidList (
  OUT  GUID  **ExtractHandlerGuidTable
  );

/**
  Retrieves a GUID from a GUIDed section and uses that GUID to select an associated handler of type
  EXTRACT_GUIDED_SECTION_GET_INFO_HANDLER that was registered with ExtractGuidedSectionRegisterHandlers().
  The selected handler is used to retrieve and return the size of the decoded buffer and the size of an
  optional scratch buffer required to actually decode the data in a GUIDed section.

  Examines a GUIDed section specified by InputSection.
  If GUID for InputSection does not match any of the GUIDs registered through ExtractGuidedSectionRegisterHandlers(),
  then RETURN_UNSUPPORTED is returned.
  If the GUID of InputSection does match the GUID that this handler supports, then the the associated handler
  of type EXTRACT_GUIDED_SECTION_GET_INFO_HANDLER that was registered with ExtractGuidedSectionRegisterHandlers()
  is used to retrieve the OututBufferSize, ScratchSize, and Attributes values. The return status from the handler of
  type EXTRACT_GUIDED_SECTION_GET_INFO_HANDLER is returned.

  If InputSection is NULL, then ASSERT().
  If OutputBufferSize is NULL, then ASSERT().
  If ScratchBufferSize is NULL, then ASSERT().
  If SectionAttribute is NULL, then ASSERT().

  @param[in]  InputSection       A pointer to a GUIDed section of an FFS formatted file.
  @param[out] OutputBufferSize   A pointer to the size, in bytes, of an output buffer required if the buffer
                                 specified by InputSection were decoded.
  @param[out] ScratchBufferSize  A pointer to the size, in bytes, required as scratch space if the buffer specified by
                                 InputSection were decoded.
  @param[out] SectionAttribute   A pointer to the attributes of the GUIDed section.  See the Attributes field of
                                 EFI_GUID_DEFINED_SECTION in the PI Specification.

  @retval  RETURN_SUCCESS      Get the required information successfully.
  @retval  RETURN_UNSUPPORTED  The GUID from the section specified by InputSection does not match any of
                               the GUIDs registered with ExtractGuidedSectionRegisterHandlers().
  @retval  Others              The return status from the handler associated with the GUID retrieved from
                               the section specified by InputSection.

**/
RETURN_STATUS
EFIAPI
ExtractGuidedSectionGetInfo (
  IN  CONST VOID    *InputSection,
  OUT       UINT32  *OutputBufferSize,
  OUT       UINT32  *ScratchBufferSize,
  OUT       UINT16  *SectionAttribute
  );

/**
  Retrieves the GUID from a GUIDed section and uses that GUID to select an associated handler of type
  EXTRACT_GUIDED_SECTION_DECODE_HANDLER that was registered with ExtractGuidedSectionRegisterHandlers().
  The selected handler is used to decode the data in a GUIDed section and return the result in a caller
  allocated output buffer.

  Decodes the GUIDed section specified by InputSection.
  If GUID for InputSection does not match any of the GUIDs registered through ExtractGuidedSectionRegisterHandlers(),
  then RETURN_UNSUPPORTED is returned.
  If the GUID of InputSection does match the GUID that this handler supports, then the the associated handler
  of type EXTRACT_GUIDED_SECTION_DECODE_HANDLER that was registered with ExtractGuidedSectionRegisterHandlers()
  is used to decode InputSection into the buffer specified by OutputBuffer and the authentication status of this
  decode operation is returned in AuthenticationStatus.  If the decoded buffer is identical to the data in InputSection,
  then OutputBuffer is set to point at the data in InputSection.  Otherwise, the decoded data will be placed in caller
  allocated buffer specified by OutputBuffer.    This function is responsible for computing the  EFI_AUTH_STATUS_PLATFORM_OVERRIDE
  bit of in AuthenticationStatus.  The return status from the handler of type EXTRACT_GUIDED_SECTION_DECODE_HANDLER is returned.

  If InputSection is NULL, then ASSERT().
  If OutputBuffer is NULL, then ASSERT().
  If ScratchBuffer is NULL and this decode operation requires a scratch buffer, then ASSERT().
  If AuthenticationStatus is NULL, then ASSERT().

  @param[in]  InputSection   A pointer to a GUIDed section of an FFS formatted file.
  @param[out] OutputBuffer   A pointer to a buffer that contains the result of a decode operation.
  @param[in]  ScratchBuffer  A caller allocated buffer that may be required by this function as a scratch buffer to perform the decode operation.
  @param[out] AuthenticationStatus
                             A pointer to the authentication status of the decoded output buffer. See the definition
                             of authentication status in the EFI_PEI_GUIDED_SECTION_EXTRACTION_PPI section of the PI
                             Specification.

  @retval  RETURN_SUCCESS           The buffer specified by InputSection was decoded.
  @retval  RETURN_UNSUPPORTED       The section specified by InputSection does not match the GUID this handler supports.
  @retval  RETURN_INVALID_PARAMETER The section specified by InputSection can not be decoded.

**/
RETURN_STATUS
EFIAPI
ExtractGuidedSectionDecode (
  IN  CONST VOID    *InputSection,
  OUT       VOID    **OutputBuffer,
  IN        VOID    *ScratchBuffer,        OPTIONAL
  OUT       UINT32  *AuthenticationStatus
  );

/**
  Retrieves handlers of type EXTRACT_GUIDED_SECTION_GET_INFO_HANDLER and
  EXTRACT_GUIDED_SECTION_DECODE_HANDLER for a specific GUID section type.

  Retrieves the handlers associated with SectionGuid and returns them in
  GetInfoHandler and DecodeHandler.

  If the GUID value specified by SectionGuid has not been registered, then
  return RETURN_NOT_FOUND.

  If SectionGuid is NULL, then ASSERT().

  @param[in]  SectionGuid    A pointer to the GUID associated with the handlersof the GUIDed
                             section type being retrieved.
  @param[out] GetInfoHandler Pointer to a function that examines a GUIDed section and returns
                             the size of the decoded buffer and the size of an optional scratch
                             buffer required to actually decode the data in a GUIDed section.
                             This is an optional parameter that may be NULL. If it is NULL, then
                             the previously registered handler is not returned.
  @param[out] DecodeHandler  Pointer to a function that decodes a GUIDed section into a caller
                             allocated output buffer. This is an optional parameter that may be NULL.
                             If it is NULL, then the previously registered handler is not returned.

  @retval  RETURN_SUCCESS     The handlers were retrieved.
  @retval  RETURN_NOT_FOUND   No handlers have been registered with the specified GUID.

**/
RETURN_STATUS
EFIAPI
ExtractGuidedSectionGetHandlers (
  IN CONST   GUID                                     *SectionGuid,
  OUT        EXTRACT_GUIDED_SECTION_GET_INFO_HANDLER  *GetInfoHandler,  OPTIONAL
  OUT        EXTRACT_GUIDED_SECTION_DECODE_HANDLER    *DecodeHandler    OPTIONAL
  );

#endif
