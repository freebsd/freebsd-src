/** @file
  The file provides services to manage the movement of
  configuration data from drivers to configuration applications.
  It then serves as the single point to receive configuration
  information from configuration applications, routing the
  results to the appropriate drivers.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol was introduced in UEFI Specification 2.1.


**/

#ifndef __HII_CONFIG_ROUTING_H__
#define __HII_CONFIG_ROUTING_H__

#define EFI_HII_CONFIG_ROUTING_PROTOCOL_GUID \
  { 0x587e72d7, 0xcc50, 0x4f79, { 0x82, 0x09, 0xca, 0x29, 0x1f, 0xc1, 0xa1, 0x0f } }


typedef struct _EFI_HII_CONFIG_ROUTING_PROTOCOL EFI_HII_CONFIG_ROUTING_PROTOCOL;

/**

  This function allows the caller to request the current
  configuration for one or more named elements from one or more
  drivers. The resulting string is in the standard HII
  configuration string format. If Successful, Results contains an
  equivalent string with "=" and the values associated with all
  names added in. The expected implementation is for each
  <ConfigRequest> substring in the Request to call the HII
  Configuration Routing Protocol ExtractProtocol function for the
  driver corresponding to the <ConfigHdr> at the start of the
  <ConfigRequest> substring. The request fails if no driver
  matches the <ConfigRequest> substring. Note: Alternative
  configuration strings may also be appended to the end of the
  current configuration string. If they are, they must appear
  after the current configuration. They must contain the same
  routing (GUID, NAME, PATH) as the current configuration string.
  They must have an additional description indicating the type of
  alternative configuration the string represents,
  "ALTCFG=<StringToken>". That <StringToken> (when converted from
  hexadecimal (encoded as text) to binary) is a reference to a string in the
  associated string pack. As an example, assume that the Request
  string is:
  GUID=...&NAME=00480050&PATH=...&Fred&George&Ron&Neville A result
  might be:
  GUID=...&NAME=00480050&PATH=...&Fred=16&George=16&Ron=12&Neville=11&
  GUID=...&NAME=00480050&PATH=...&ALTCFG=0037&Fred=12&Neville=7

  @param This       Points to the EFI_HII_CONFIG_ROUTING_PROTOCOL
                    instance.

  @param Request    A null-terminated string in <MultiConfigRequest> format.

  @param Progress   On return, points to a character in the
                    Request string. Points to the string's null
                    terminator if the request was successful. Points
                    to the most recent '&' before the first
                    failing name / value pair (or the beginning
                    of the string if the failure is in the first
                    name / value pair) if the request was not
                    successful

  @param Results    A null-terminated string in <MultiConfigAltResp> format
                    which has all values filled in for the names in the
                    Request string.

  @retval EFI_SUCCESS             The Results string is filled with the
                                  values corresponding to all requested
                                  names.

  @retval EFI_OUT_OF_RESOURCES    Not enough memory to store the
                                  parts of the results that must be
                                  stored awaiting possible future
                                  protocols.

  @retval EFI_INVALID_PARAMETER   For example, passing in a NULL
                                  for the Request parameter
                                  would result in this type of
                                  error. The Progress parameter
                                  is set to NULL.

  @retval EFI_NOT_FOUND           Routing data doesn't match any
                                  known driver. Progress set to
                                  the "G" in "GUID" of the
                                  routing header that doesn't
                                  match. Note: There is no
                                  requirement that all routing
                                  data be validated before any
                                  configuration extraction.

  @retval EFI_INVALID_PARAMETER   Illegal syntax. Progress set
                                  to the most recent & before the
                                  error, or the beginning of the
                                  string.
  @retval EFI_INVALID_PARAMETER   The ExtractConfig function of the
                                  underlying HII Configuration
                                  Access Protocol returned
                                  EFI_INVALID_PARAMETER. Progress
                                  set to most recent & before the
                                  error or the beginning of the
                                  string.

**/
typedef
EFI_STATUS
(EFIAPI * EFI_HII_EXTRACT_CONFIG)(
  IN CONST  EFI_HII_CONFIG_ROUTING_PROTOCOL *This,
  IN CONST  EFI_STRING                      Request,
  OUT       EFI_STRING                      *Progress,
  OUT       EFI_STRING                      *Results
);

/**
  This function allows the caller to request the current configuration
  for the entirety of the current HII database and returns the data in
  a null-terminated string.

  This function allows the caller to request the current
  configuration for all of the current HII database. The results
  include both the current and alternate configurations as
  described in ExtractConfig() above.

  @param This     Points to the EFI_HII_CONFIG_ROUTING_PROTOCOL instance.

  @param  Results Null-terminated Unicode string in
                  <MultiConfigAltResp> format which has all values
                  filled in for the entirety of the current HII
                  database. String to be allocated by the  called
                  function. De-allocation is up to the caller.

  @retval EFI_SUCCESS             The Results string is filled with the
                                  values corresponding to all requested
                                  names.

  @retval EFI_OUT_OF_RESOURCES    Not enough memory to store the
                                  parts of the results that must be
                                  stored awaiting possible future
                                  protocols.

  @retval EFI_INVALID_PARAMETERS  For example, passing in a NULL
                                  for the Results parameter
                                  would result in this type of
                                  error.

**/
typedef
EFI_STATUS
(EFIAPI * EFI_HII_EXPORT_CONFIG)(
  IN CONST  EFI_HII_CONFIG_ROUTING_PROTOCOL *This,
  OUT       EFI_STRING                      *Results
);

/**

  This function routes the results of processing forms to the
  appropriate targets. It scans for <ConfigHdr> within the string
  and passes the header and subsequent body to the driver whose
  location is described in the <ConfigHdr>. Many <ConfigHdr>s may
  appear as a single request. The expected implementation is to
  hand off the various <ConfigResp> substrings to the
  Configuration Access Protocol RouteConfig routine corresponding
  to the driver whose routing information is defined by the
  <ConfigHdr> in turn.

  @param This           Points to the EFI_HII_CONFIG_ROUTING_PROTOCOL instance.

  @param Configuration  A null-terminated string in <MulltiConfigResp> format.

  @param Progress       A pointer to a string filled in with the
                        offset of the most recent '&' before the
                        first failing name / value pair (or the
                        beginning of the string if the failure is in
                        the first name / value pair), or the
                        terminating NULL if all was successful.

  @retval EFI_SUCCESS             The results have been distributed or are
                                  awaiting distribution.

  @retval EFI_OUT_OF_RESOURCES    Not enough memory to store the
                                  parts of the results that must be
                                  stored awaiting possible future
                                  protocols.

  @retval EFI_INVALID_PARAMETERS  Passing in a NULL for the
                                  Results parameter would result
                                  in this type of error.

  @retval EFI_NOT_FOUND           The target for the specified routing data
                                  was not found.

**/
typedef
EFI_STATUS
(EFIAPI * EFI_HII_ROUTE_CONFIG)(
  IN CONST  EFI_HII_CONFIG_ROUTING_PROTOCOL *This,
  IN CONST  EFI_STRING                      Configuration,
  OUT       EFI_STRING                      *Progress
);


/**

  This function extracts the current configuration from a block of
  bytes. To do so, it requires that the ConfigRequest string
  consists of a list of <BlockName> formatted names. It uses the
  offset in the name to determine the index into the Block to
  start the extraction and the width of each name to determine the
  number of bytes to extract. These are mapped to a string
  using the equivalent of the C "%x" format (with optional leading
  spaces). The call fails if, for any (offset, width) pair in
  ConfigRequest, offset+value >= BlockSize.

  @param This      Points to the EFI_HII_CONFIG_ROUTING_PROTOCOL instance.

  @param ConfigRequest  A null-terminated string in <ConfigRequest> format.

  @param Block      An array of bytes defining the block's
                    configuration.

  @param BlockSize  The length in bytes of Block.

  @param Config     The filled-in configuration string. String
                    allocated by the function. Returned only if
                    call is successful. The null-terminated string
                    will be <ConfigResp> format.

  @param Progress   A pointer to a string filled in with the
                    offset of the most recent '&' before the
                    first failing name / value pair (or the
                    beginning of the string if the failure is in
                    the first name / value pair), or the
                    terminating NULL if all was successful.

  @retval EFI_SUCCESS             The request succeeded. Progress points
                                  to the null terminator at the end of the
                                  ConfigRequest string.

  @retval EFI_OUT_OF_RESOURCES    Not enough memory to allocate
                                  Config. Progress points to the
                                  first character of ConfigRequest.

  @retval EFI_INVALID_PARAMETERS  Passing in a NULL for the
                                  ConfigRequest or Block
                                  parameter would result in this
                                  type of error. Progress points
                                  to the first character of
                                  ConfigRequest.

  @retval EFI_NOT_FOUND           The target for the specified routing data
                                  was not found. Progress points to the
                                  'G' in "GUID" of the errant routing
                                  data.
  @retval EFI_DEVICE_ERROR        The block is not large enough. Progress undefined.

  @retval EFI_INVALID_PARAMETER   Encountered non <BlockName>
                                  formatted string. Block is
                                  left updated and Progress
                                  points at the '&' preceding
                                  the first non-<BlockName>.

**/
typedef
EFI_STATUS
(EFIAPI * EFI_HII_BLOCK_TO_CONFIG)(
  IN CONST  EFI_HII_CONFIG_ROUTING_PROTOCOL *This,
  IN CONST  EFI_STRING                      ConfigRequest,
  IN CONST  UINT8                           *Block,
  IN CONST  UINTN                           BlockSize,
  OUT       EFI_STRING                      *Config,
  OUT       EFI_STRING                      *Progress
);



/**
  This function maps a configuration containing a series of
  <BlockConfig> formatted name value pairs in ConfigResp into a
  Block so it may be stored in a linear mapped storage such as a
  UEFI Variable. If present, the function skips GUID, NAME, and
  PATH in <ConfigResp>. It stops when it finds a non-<BlockConfig>
  name / value pair (after skipping the routing header) or when it
  reaches the end of the string.
  Example Assume an existing block containing: 00 01 02 03 04 05
  And the ConfigResp string is:
  OFFSET=4&WIDTH=1&VALUE=7&OFFSET=0&WIDTH=2&VALUE=AA55
  The results are
  55 AA 02 07 04 05

  @param This           Points to the EFI_HII_CONFIG_ROUTING_PROTOCOL instance.

  @param ConfigResp     A null-terminated string in <ConfigResp> format.

  @param Block          A possibly null array of bytes
                        representing the current block. Only
                        bytes referenced in the ConfigResp
                        string in the block are modified. If
                        this parameter is null or if the
                        BlockLength parameter is (on input)
                        shorter than required by the
                        Configuration string, only the BlockSize
                        parameter is updated, and an appropriate
                        status (see below) is returned.

  @param BlockSize      The length of the Block in units of UINT8.
                        On input, this is the size of the Block. On
                        output, if successful, contains the largest
                        index of the modified byte in the Block, or
                        the required buffer size if the Block is not
                        large enough.

  @param Progress       On return, points to an element of the
                        ConfigResp string filled in with the offset
                        of the most recent "&" before the first
                        failing name / value pair (or the beginning
                        of the string if the failure is in the first
                        name / value pair), or the terminating NULL
                        if all was successful.

  @retval EFI_SUCCESS            The request succeeded. Progress points to the null
                                 terminator at the end of the ConfigResp string.
  @retval EFI_OUT_OF_RESOURCES   Not enough memory to allocate Config. Progress
                                 points to the first character of ConfigResp.
  @retval EFI_INVALID_PARAMETER  Passing in a NULL for the ConfigResp or
                                 Block parameter would result in this type of
                                 error. Progress points to the first character of
                                         ConfigResp.
  @retval EFI_INVALID_PARAMETER  Encountered non <BlockName> formatted name /
                                 value pair. Block is left updated and
                                 Progress points at the '&' preceding the first
                                 non-<BlockName>.
  @retval EFI_DEVICE_ERROR       Block not large enough. Progress undefined.
  @retval EFI_NOT_FOUND          Target for the specified routing data was not found.
                                 Progress points to the "G" in "GUID" of the errant
                                 routing data.
  @retval EFI_BUFFER_TOO_SMALL   Block not large enough. Progress undefined.
                                 BlockSize is updated with the required buffer size.

**/
typedef
EFI_STATUS
(EFIAPI * EFI_HII_CONFIG_TO_BLOCK)(
  IN CONST  EFI_HII_CONFIG_ROUTING_PROTOCOL *This,
  IN CONST  EFI_STRING                      ConfigResp,
  IN OUT    UINT8                           *Block,
  IN OUT    UINTN                           *BlockSize,
  OUT       EFI_STRING                      *Progress
);

/**
  This helper function is to be called by drivers to extract portions of
  a larger configuration string.

  @param This              A pointer to the EFI_HII_CONFIG_ROUTING_PROTOCOL instance.
  @param ConfigResp        A null-terminated string in <ConfigAltResp> format.
  @param Guid              A pointer to the GUID value to search for in the
                           routing portion of the ConfigResp string when retrieving
                           the requested data. If Guid is NULL, then all GUID
                           values will be searched for.
  @param Name              A pointer to the NAME value to search for in the
                           routing portion of the ConfigResp string when retrieving
                           the requested data. If Name is NULL, then all Name
                           values will be searched for.
  @param DevicePath        A pointer to the PATH value to search for in the
                           routing portion of the ConfigResp string when retrieving
                           the requested data. If DevicePath is NULL, then all
                           DevicePath values will be searched for.
  @param AltCfgId          A pointer to the ALTCFG value to search for in the
                           routing portion of the ConfigResp string when retrieving
                           the requested data.  If this parameter is NULL,
                           then the current setting will be retrieved.
  @param AltCfgResp        A pointer to a buffer which will be allocated by the
                           function which contains the retrieved string as requested.
                           This buffer is only allocated if the call was successful.
                           The null-terminated string will be <ConfigResp> format.

  @retval EFI_SUCCESS             The request succeeded. The requested data was extracted
                                  and placed in the newly allocated AltCfgResp buffer.
  @retval EFI_OUT_OF_RESOURCES    Not enough memory to allocate AltCfgResp.
  @retval EFI_INVALID_PARAMETER   Any parameter is invalid.
  @retval EFI_NOT_FOUND           The target for the specified routing data was not found.
**/
typedef
EFI_STATUS
(EFIAPI * EFI_HII_GET_ALT_CFG)(
  IN  CONST EFI_HII_CONFIG_ROUTING_PROTOCOL    *This,
  IN  CONST EFI_STRING                         ConfigResp,
  IN  CONST EFI_GUID                           *Guid,
  IN  CONST EFI_STRING                         Name,
  IN  CONST EFI_DEVICE_PATH_PROTOCOL           *DevicePath,
  IN  CONST UINT16                             *AltCfgId,
  OUT EFI_STRING                               *AltCfgResp
  );

///
/// This protocol defines the configuration routing interfaces
/// between external applications and the HII. There may only be one
/// instance of this protocol in the system.
///
struct _EFI_HII_CONFIG_ROUTING_PROTOCOL {
  EFI_HII_EXTRACT_CONFIG  ExtractConfig;
  EFI_HII_EXPORT_CONFIG   ExportConfig;
  EFI_HII_ROUTE_CONFIG    RouteConfig;
  EFI_HII_BLOCK_TO_CONFIG BlockToConfig;
  EFI_HII_CONFIG_TO_BLOCK ConfigToBlock;
  EFI_HII_GET_ALT_CFG     GetAltConfig;
};

extern EFI_GUID gEfiHiiConfigRoutingProtocolGuid;


#endif

