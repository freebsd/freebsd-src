/** @file
  Null Base Report Status Code Library instance with empty functions.

  Copyright (c) 2009 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Base.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/DebugLib.h>

/**
  Converts a status code to an 8-bit POST code value.

  Converts the status code specified by CodeType and Value to an 8-bit POST code
  and returns the 8-bit POST code in PostCode.  If CodeType is an
  EFI_PROGRESS_CODE or CodeType is an EFI_ERROR_CODE, then bits 0..4 of PostCode
  are set to bits 16..20 of Value, and bits 5..7 of PostCode are set to bits
  24..26 of Value., and TRUE is returned.  Otherwise, FALSE is returned.

  If PostCode is NULL, then ASSERT().

  @param  CodeType  The type of status code being converted.
  @param  Value     The status code value being converted.
  @param  PostCode  A pointer to the 8-bit POST code value to return.

  @retval  TRUE   The status code specified by CodeType and Value was converted
                  to an 8-bit POST code and returned in  PostCode.
  @retval  FALSE  The status code specified by CodeType and Value could not be
                  converted to an 8-bit POST code value.

**/
BOOLEAN
EFIAPI
CodeTypeToPostCode (
  IN  EFI_STATUS_CODE_TYPE   CodeType,
  IN  EFI_STATUS_CODE_VALUE  Value,
  OUT UINT8                  *PostCode
  )
{
  ASSERT (PostCode != NULL);

  return FALSE;
}


/**
  Extracts ASSERT() information from a status code structure.

  Converts the status code specified by CodeType, Value, and Data to the ASSERT()
  arguments specified by Filename, Description, and LineNumber.  If CodeType is
  an EFI_ERROR_CODE, and CodeType has a severity of EFI_ERROR_UNRECOVERED, and
  Value has an operation mask of EFI_SW_EC_ILLEGAL_SOFTWARE_STATE, extract
  Filename, Description, and LineNumber from the optional data area of the
  status code buffer specified by Data.  The optional data area of Data contains
  a Null-terminated ASCII string for the FileName, followed by a Null-terminated
  ASCII string for the Description, followed by a 32-bit LineNumber.  If the
  ASSERT() information could be extracted from Data, then return TRUE.
  Otherwise, FALSE is returned.

  If Data is NULL, then ASSERT().
  If Filename is NULL, then ASSERT().
  If Description is NULL, then ASSERT().
  If LineNumber is NULL, then ASSERT().

  @param  CodeType     The type of status code being converted.
  @param  Value        The status code value being converted.
  @param  Data         The pointer to the status code data buffer.
  @param  Filename     The pointer to the source file name that generated the ASSERT().
  @param  Description  The pointer to the description of the ASSERT().
  @param  LineNumber   The pointer to the source line number that generated the ASSERT().

  @retval  TRUE   The status code specified by CodeType, Value, and Data was
                  converted ASSERT() arguments specified by Filename, Description,
                  and LineNumber.
  @retval  FALSE  The status code specified by CodeType, Value, and Data could
                  not be converted to ASSERT() arguments.

**/
BOOLEAN
EFIAPI
ReportStatusCodeExtractAssertInfo (
  IN EFI_STATUS_CODE_TYPE        CodeType,
  IN EFI_STATUS_CODE_VALUE       Value,
  IN CONST EFI_STATUS_CODE_DATA  *Data,
  OUT CHAR8                      **Filename,
  OUT CHAR8                      **Description,
  OUT UINT32                     *LineNumber
  )
{
  return FALSE;
}


/**
  Extracts DEBUG() information from a status code structure.

  Converts the status code specified by Data to the DEBUG() arguments specified
  by ErrorLevel, Marker, and Format.  If type GUID in Data is
  EFI_STATUS_CODE_DATA_TYPE_DEBUG_GUID, then extract ErrorLevel, Marker, and
  Format from the optional data area of the status code buffer specified by Data.
  The optional data area of Data contains a 32-bit ErrorLevel followed by Marker
  which is 12 UINTN parameters, followed by a Null-terminated ASCII string for
  the Format.  If the DEBUG() information could be extracted from Data, then
  return TRUE.  Otherwise, FALSE is returned.

  If Data is NULL, then ASSERT().
  If ErrorLevel is NULL, then ASSERT().
  If Marker is NULL, then ASSERT().
  If Format is NULL, then ASSERT().

  @param  Data        The pointer to the status code data buffer.
  @param  ErrorLevel  The pointer to the error level mask for a debug message.
  @param  Marker      The pointer to the variable argument list associated with Format.
  @param  Format      The pointer to a Null-terminated ASCII format string of a
                      debug message.

  @retval  TRUE   The status code specified by Data was converted DEBUG() arguments
                  specified by ErrorLevel, Marker, and Format.
  @retval  FALSE  The status code specified by Data could not be converted to
                  DEBUG() arguments.

**/
BOOLEAN
EFIAPI
ReportStatusCodeExtractDebugInfo (
  IN CONST EFI_STATUS_CODE_DATA  *Data,
  OUT UINT32                     *ErrorLevel,
  OUT BASE_LIST                  *Marker,
  OUT CHAR8                      **Format
  )
{
  ASSERT (Data       != NULL);
  ASSERT (ErrorLevel != NULL);
  ASSERT (Marker     != NULL);
  ASSERT (Format     != NULL);

  return FALSE;
}


/**
  Reports a status code.

  Reports the status code specified by the parameters Type and Value.  Status
  code also require an instance, caller ID, and extended data.  This function
  passed in a zero instance, NULL extended data, and a caller ID of
  gEfiCallerIdGuid, which is the GUID for the module.

  ReportStatusCode()must actively prevent recursion.  If ReportStatusCode()
  is called while processing another any other Report Status Code Library function,
  then ReportStatusCode() must return immediately.

  @param  Type   The status code type.
  @param  Value  The status code value.

  @retval  EFI_SUCCESS       The status code was reported.
  @retval  EFI_DEVICE_ERROR  There status code could not be reported due to a
                             device error.
  @retval  EFI_UNSUPPORTED   The report status code is not supported.

**/
EFI_STATUS
EFIAPI
ReportStatusCode (
  IN EFI_STATUS_CODE_TYPE   Type,
  IN EFI_STATUS_CODE_VALUE  Value
  )
{
  return EFI_SUCCESS;
}


/**
  Reports a status code with a Device Path Protocol as the extended data.

  Allocates and fills in the extended data section of a status code with the
  Device Path Protocol specified by DevicePath.  This function is responsible
  for allocating a buffer large enough for the standard header and the device
  path.  The standard header is filled in with a GUID of
  gEfiStatusCodeSpecificDataGuid.  The status code is reported with a zero
  instance and a caller ID of gEfiCallerIdGuid.

  ReportStatusCodeWithDevicePath()must actively prevent recursion.  If
  ReportStatusCodeWithDevicePath() is called while processing another any other
  Report Status Code Library function, then ReportStatusCodeWithDevicePath()
  must return EFI_DEVICE_ERROR immediately.

  If DevicePath is NULL, then ASSERT().

  @param  Type        Status code type.
  @param  Value       Status code value.
  @param  DevicePath  The pointer to the Device Path Protocol to be reported.

  @retval  EFI_SUCCESS           The status code was reported with the extended
                                 data specified by DevicePath.
  @retval  EFI_OUT_OF_RESOURCES  There were not enough resources to allocate the
                                 extended data section.
  @retval  EFI_UNSUPPORTED       The report status code is not supported

**/
EFI_STATUS
EFIAPI
ReportStatusCodeWithDevicePath (
  IN EFI_STATUS_CODE_TYPE            Type,
  IN EFI_STATUS_CODE_VALUE           Value,
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  ASSERT (DevicePath != NULL);
  
  return EFI_SUCCESS;
}


/**
  Reports a status code with an extended data buffer.

  Allocates and fills in the extended data section of a status code with the
  extended data specified by ExtendedData and ExtendedDataSize.  ExtendedData
  is assumed to be one of the data structures specified in Related Definitions.
  These data structure do not have the standard header, so this function is
  responsible for allocating a buffer large enough for the standard header and
  the extended data passed into this function.  The standard header is filled
  in with a GUID of  gEfiStatusCodeSpecificDataGuid.  The status code is reported
  with a zero instance and a caller ID of gEfiCallerIdGuid.

  ReportStatusCodeWithExtendedData()must actively prevent recursion.  If
  ReportStatusCodeWithExtendedData() is called while processing another any other
  Report Status Code Library function, then ReportStatusCodeWithExtendedData()
  must return EFI_DEVICE_ERROR immediately.

  If ExtendedData is NULL, then ASSERT().
  If ExtendedDataSize is 0, then ASSERT().

  @param  Type              Status code type.
  @param  Value             Status code value.
  @param  ExtendedData      The pointer to the extended data buffer to be reported.
  @param  ExtendedDataSize  The size, in bytes, of the extended data buffer to
                            be reported.

  @retval  EFI_SUCCESS           The status code was reported with the extended
                                 data specified by ExtendedData and ExtendedDataSize.
  @retval  EFI_OUT_OF_RESOURCES  There were not enough resources to allocate the
                                 extended data section.
  @retval  EFI_UNSUPPORTED       The report status code is not supported.

**/
EFI_STATUS
EFIAPI
ReportStatusCodeWithExtendedData (
  IN EFI_STATUS_CODE_TYPE   Type,
  IN EFI_STATUS_CODE_VALUE  Value,
  IN CONST VOID             *ExtendedData,
  IN UINTN                  ExtendedDataSize
  )
{
  ASSERT (ExtendedData     != NULL);
  ASSERT (ExtendedDataSize != 0);
  return EFI_SUCCESS;
}


/**
  Reports a status code with full parameters.

  The function reports a status code.  If ExtendedData is NULL and ExtendedDataSize
  is 0, then an extended data buffer is not reported.  If ExtendedData is not
  NULL and ExtendedDataSize is not 0, then an extended data buffer is allocated.
  ExtendedData is assumed not have the standard status code header, so this function
  is responsible for allocating a buffer large enough for the standard header and
  the extended data passed into this function.  The standard header is filled in
  with a GUID specified by ExtendedDataGuid.  If ExtendedDataGuid is NULL, then a
  GUID of gEfiStatusCodeSpecificDataGuid is used.  The status code is reported with
  an instance specified by Instance and a caller ID specified by CallerId.  If
  CallerId is NULL, then a caller ID of gEfiCallerIdGuid is used.

  ReportStatusCodeEx()must actively prevent recursion.  If ReportStatusCodeEx()
  is called while processing another any other Report Status Code Library function,
  then ReportStatusCodeEx() must return EFI_DEVICE_ERROR immediately.

  If ExtendedData is NULL and ExtendedDataSize is not zero, then ASSERT().
  If ExtendedData is not NULL and ExtendedDataSize is zero, then ASSERT().

  @param  Type              The status code type.
  @param  Value             The status code value.
  @param  Instance          Status code instance number.
  @param  CallerId          The pointer to a GUID that identifies the caller of this
                            function.  If this parameter is NULL, then a caller
                            ID of gEfiCallerIdGuid is used.
  @param  ExtendedDataGuid  The pointer to the GUID for the extended data buffer.
                            If this parameter is NULL, then a the status code
                            standard header is filled in with
                            gEfiStatusCodeSpecificDataGuid.
  @param  ExtendedData      The pointer to the extended data buffer.  This is an
                            optional parameter that may be NULL.
  @param  ExtendedDataSize  The size, in bytes, of the extended data buffer.

  @retval  EFI_SUCCESS           The status code was reported.
  @retval  EFI_OUT_OF_RESOURCES  There were not enough resources to allocate
                                 the extended data section if it was specified.
  @retval  EFI_UNSUPPORTED       The report status code is not supported.

**/
EFI_STATUS
EFIAPI
ReportStatusCodeEx (
  IN EFI_STATUS_CODE_TYPE   Type,
  IN EFI_STATUS_CODE_VALUE  Value,
  IN UINT32                 Instance,
  IN CONST EFI_GUID         *CallerId          OPTIONAL,
  IN CONST EFI_GUID         *ExtendedDataGuid  OPTIONAL,
  IN CONST VOID             *ExtendedData      OPTIONAL,
  IN UINTN                  ExtendedDataSize
  )
{
  return EFI_SUCCESS;
}


/**
  Returns TRUE if status codes of type EFI_PROGRESS_CODE are enabled

  This function returns TRUE if the REPORT_STATUS_CODE_PROPERTY_PROGRESS_CODE_ENABLED
  bit of PcdReportStatusCodeProperyMask is set.  Otherwise FALSE is returned.

  @retval  TRUE   The REPORT_STATUS_CODE_PROPERTY_PROGRESS_CODE_ENABLED bit of
                  PcdReportStatusCodeProperyMask is set.
  @retval  FALSE  The REPORT_STATUS_CODE_PROPERTY_PROGRESS_CODE_ENABLED bit of
                  PcdReportStatusCodeProperyMask is clear.

**/
BOOLEAN
EFIAPI
ReportProgressCodeEnabled (
  VOID
  )
{
  return FALSE;
}


/**
  Returns TRUE if status codes of type EFI_ERROR_CODE are enabled

  This function returns TRUE if the REPORT_STATUS_CODE_PROPERTY_ERROR_CODE_ENABLED
  bit of PcdReportStatusCodeProperyMask is set.  Otherwise FALSE is returned.

  @retval  TRUE   The REPORT_STATUS_CODE_PROPERTY_ERROR_CODE_ENABLED bit of
                  PcdReportStatusCodeProperyMask is set.
  @retval  FALSE  The REPORT_STATUS_CODE_PROPERTY_ERROR_CODE_ENABLED bit of
                  PcdReportStatusCodeProperyMask is clear.

**/
BOOLEAN
EFIAPI
ReportErrorCodeEnabled (
  VOID
  )
{
  return FALSE;
}


/**
  Returns TRUE if status codes of type EFI_DEBUG_CODE are enabled

  This function returns TRUE if the REPORT_STATUS_CODE_PROPERTY_DEBUG_CODE_ENABLED
  bit of PcdReportStatusCodeProperyMask is set.  Otherwise FALSE is returned.

  @retval  TRUE   The REPORT_STATUS_CODE_PROPERTY_DEBUG_CODE_ENABLED bit of
                  PcdReportStatusCodeProperyMask is set.
  @retval  FALSE  The REPORT_STATUS_CODE_PROPERTY_DEBUG_CODE_ENABLED bit of
                  PcdReportStatusCodeProperyMask is clear.

**/
BOOLEAN
EFIAPI
ReportDebugCodeEnabled (
  VOID
  )
{
  return FALSE;
}

