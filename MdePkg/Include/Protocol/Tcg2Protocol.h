/** @file
  TPM2 Protocol as defined in TCG PC Client Platform EFI Protocol Specification Family "2.0".
  See http://trustedcomputinggroup.org for the latest specification

Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __TCG2_PROTOCOL_H__
#define __TCG2_PROTOCOL_H__

#include <IndustryStandard/UefiTcgPlatform.h>
#include <IndustryStandard/Tpm20.h>

#define EFI_TCG2_PROTOCOL_GUID \
  {0x607f766c, 0x7455, 0x42be, { 0x93, 0x0b, 0xe4, 0xd7, 0x6d, 0xb2, 0x72, 0x0f }}

typedef struct tdEFI_TCG2_PROTOCOL EFI_TCG2_PROTOCOL;

typedef struct tdEFI_TCG2_VERSION {
  UINT8    Major;
  UINT8    Minor;
} EFI_TCG2_VERSION;

typedef UINT32 EFI_TCG2_EVENT_LOG_BITMAP;
typedef UINT32 EFI_TCG2_EVENT_LOG_FORMAT;
typedef UINT32 EFI_TCG2_EVENT_ALGORITHM_BITMAP;

#define EFI_TCG2_EVENT_LOG_FORMAT_TCG_1_2  0x00000001
#define EFI_TCG2_EVENT_LOG_FORMAT_TCG_2    0x00000002

typedef struct tdEFI_TCG2_BOOT_SERVICE_CAPABILITY {
  //
  // Allocated size of the structure
  //
  UINT8                              Size;
  //
  // Version of the EFI_TCG2_BOOT_SERVICE_CAPABILITY structure itself.
  // For this version of the protocol, the Major version shall be set to 1
  // and the Minor version shall be set to 1.
  //
  EFI_TCG2_VERSION                   StructureVersion;
  //
  // Version of the EFI TCG2 protocol.
  // For this version of the protocol, the Major version shall be set to 1
  // and the Minor version shall be set to 1.
  //
  EFI_TCG2_VERSION                   ProtocolVersion;
  //
  // Supported hash algorithms (this bitmap is determined by the supported PCR
  // banks in the TPM and the hashing algorithms supported by the firmware)
  //
  EFI_TCG2_EVENT_ALGORITHM_BITMAP    HashAlgorithmBitmap;
  //
  // Bitmap of supported event log formats
  //
  EFI_TCG2_EVENT_LOG_BITMAP          SupportedEventLogs;
  //
  // False = TPM not present
  //
  BOOLEAN                            TPMPresentFlag;
  //
  // Max size (in bytes) of a command that can be sent to the TPM
  //
  UINT16                             MaxCommandSize;
  //
  // Max size (in bytes) of a response that can be provided by the TPM
  //
  UINT16                             MaxResponseSize;
  //
  // 4-byte Vendor ID
  // (see TCG Vendor ID registry, Section "TPM Capabilities Vendor ID")
  //
  UINT32                             ManufacturerID;
  //
  // Maximum number of PCR banks (hashing algorithms) supported.
  // No granularity is provided to support a specific set of algorithms.
  // Minimum value is 1.
  //
  UINT32                             NumberOfPCRBanks;
  //
  // A bitmap of currently active PCR banks (hashing algorithms).
  // This is a subset of the supported hashing algorithms reported in HashAlgorithmBitMap.
  // NumberOfPcrBanks defines the number of bits that are set.
  //
  EFI_TCG2_EVENT_ALGORITHM_BITMAP    ActivePcrBanks;
} EFI_TCG2_BOOT_SERVICE_CAPABILITY;

#define EFI_TCG2_BOOT_HASH_ALG_SHA1     0x00000001
#define EFI_TCG2_BOOT_HASH_ALG_SHA256   0x00000002
#define EFI_TCG2_BOOT_HASH_ALG_SHA384   0x00000004
#define EFI_TCG2_BOOT_HASH_ALG_SHA512   0x00000008
#define EFI_TCG2_BOOT_HASH_ALG_SM3_256  0x00000010

//
// This bit is shall be set when an event shall be extended but not logged.
//
#define EFI_TCG2_EXTEND_ONLY  0x0000000000000001
//
// This bit shall be set when the intent is to measure a PE/COFF image.
//
#define PE_COFF_IMAGE  0x0000000000000010

#define MAX_PCR_INDEX  23

#pragma pack(1)

#define EFI_TCG2_EVENT_HEADER_VERSION  1

typedef struct {
  //
  // Size of the event header itself (sizeof(EFI_TCG2_EVENT_HEADER)).
  //
  UINT32           HeaderSize;
  //
  // Header version. For this version of this specification, the value shall be 1.
  //
  UINT16           HeaderVersion;
  //
  // Index of the PCR that shall be extended (0 - 23).
  //
  TCG_PCRINDEX     PCRIndex;
  //
  // Type of the event that shall be extended (and optionally logged).
  //
  TCG_EVENTTYPE    EventType;
} EFI_TCG2_EVENT_HEADER;

typedef struct tdEFI_TCG2_EVENT {
  //
  // Total size of the event including the Size component, the header and the Event data.
  //
  UINT32                   Size;
  EFI_TCG2_EVENT_HEADER    Header;
  UINT8                    Event[1];
} EFI_TCG2_EVENT;

#pragma pack()

/**
  The EFI_TCG2_PROTOCOL GetCapability function call provides protocol
  capability information and state information.

  @param[in]      This               Indicates the calling context
  @param[in, out] ProtocolCapability The caller allocates memory for a EFI_TCG2_BOOT_SERVICE_CAPABILITY
                                     structure and sets the size field to the size of the structure allocated.
                                     The callee fills in the fields with the EFI protocol capability information
                                     and the current EFI TCG2 state information up to the number of fields which
                                     fit within the size of the structure passed in.

  @retval EFI_SUCCESS            Operation completed successfully.
  @retval EFI_DEVICE_ERROR       The command was unsuccessful.
                                 The ProtocolCapability variable will not be populated.
  @retval EFI_INVALID_PARAMETER  One or more of the parameters are incorrect.
                                 The ProtocolCapability variable will not be populated.
  @retval EFI_BUFFER_TOO_SMALL   The ProtocolCapability variable is too small to hold the full response.
                                 It will be partially populated (required Size field will be set).
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCG2_GET_CAPABILITY)(
  IN EFI_TCG2_PROTOCOL                    *This,
  IN OUT EFI_TCG2_BOOT_SERVICE_CAPABILITY *ProtocolCapability
  );

/**
  The EFI_TCG2_PROTOCOL Get Event Log function call allows a caller to
  retrieve the address of a given event log and its last entry.

  @param[in]  This               Indicates the calling context
  @param[in]  EventLogFormat     The type of the event log for which the information is requested.
  @param[out] EventLogLocation   A pointer to the memory address of the event log.
  @param[out] EventLogLastEntry  If the Event Log contains more than one entry, this is a pointer to the
                                 address of the start of the last entry in the event log in memory.
  @param[out] EventLogTruncated  If the Event Log is missing at least one entry because an event would
                                 have exceeded the area allocated for events, this value is set to TRUE.
                                 Otherwise, the value will be FALSE and the Event Log will be complete.

  @retval EFI_SUCCESS            Operation completed successfully.
  @retval EFI_INVALID_PARAMETER  One or more of the parameters are incorrect
                                 (e.g. asking for an event log whose format is not supported).
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCG2_GET_EVENT_LOG)(
  IN EFI_TCG2_PROTOCOL         *This,
  IN EFI_TCG2_EVENT_LOG_FORMAT EventLogFormat,
  OUT EFI_PHYSICAL_ADDRESS     *EventLogLocation,
  OUT EFI_PHYSICAL_ADDRESS     *EventLogLastEntry,
  OUT BOOLEAN                  *EventLogTruncated
  );

/**
  The EFI_TCG2_PROTOCOL HashLogExtendEvent function call provides callers with
  an opportunity to extend and optionally log events without requiring
  knowledge of actual TPM commands.
  The extend operation will occur even if this function cannot create an event
  log entry (e.g. due to the event log being full).

  @param[in]  This               Indicates the calling context
  @param[in]  Flags              Bitmap providing additional information.
  @param[in]  DataToHash         Physical address of the start of the data buffer to be hashed.
  @param[in]  DataToHashLen      The length in bytes of the buffer referenced by DataToHash.
  @param[in]  EfiTcgEvent        Pointer to data buffer containing information about the event.

  @retval EFI_SUCCESS            Operation completed successfully.
  @retval EFI_DEVICE_ERROR       The command was unsuccessful.
  @retval EFI_VOLUME_FULL        The extend operation occurred, but the event could not be written to one or more event logs.
  @retval EFI_INVALID_PARAMETER  One or more of the parameters are incorrect.
  @retval EFI_UNSUPPORTED        The PE/COFF image type is not supported.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCG2_HASH_LOG_EXTEND_EVENT)(
  IN EFI_TCG2_PROTOCOL    *This,
  IN UINT64               Flags,
  IN EFI_PHYSICAL_ADDRESS DataToHash,
  IN UINT64               DataToHashLen,
  IN EFI_TCG2_EVENT       *EfiTcgEvent
  );

/**
  This service enables the sending of commands to the TPM.

  @param[in]  This                     Indicates the calling context
  @param[in]  InputParameterBlockSize  Size of the TPM input parameter block.
  @param[in]  InputParameterBlock      Pointer to the TPM input parameter block.
  @param[in]  OutputParameterBlockSize Size of the TPM output parameter block.
  @param[in]  OutputParameterBlock     Pointer to the TPM output parameter block.

  @retval EFI_SUCCESS            The command byte stream was successfully sent to the device and a response was successfully received.
  @retval EFI_DEVICE_ERROR       The command was not successfully sent to the device or a response was not successfully received from the device.
  @retval EFI_INVALID_PARAMETER  One or more of the parameters are incorrect.
  @retval EFI_BUFFER_TOO_SMALL   The output parameter block is too small.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCG2_SUBMIT_COMMAND)(
  IN EFI_TCG2_PROTOCOL *This,
  IN UINT32            InputParameterBlockSize,
  IN UINT8             *InputParameterBlock,
  IN UINT32            OutputParameterBlockSize,
  IN UINT8             *OutputParameterBlock
  );

/**
  This service returns the currently active PCR banks.

  @param[in]  This            Indicates the calling context
  @param[out] ActivePcrBanks  Pointer to the variable receiving the bitmap of currently active PCR banks.

  @retval EFI_SUCCESS           The bitmap of active PCR banks was stored in the ActivePcrBanks parameter.
  @retval EFI_INVALID_PARAMETER One or more of the parameters are incorrect.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCG2_GET_ACTIVE_PCR_BANKS)(
  IN  EFI_TCG2_PROTOCOL *This,
  OUT UINT32            *ActivePcrBanks
  );

/**
  This service sets the currently active PCR banks.

  @param[in]  This            Indicates the calling context
  @param[in]  ActivePcrBanks  Bitmap of the requested active PCR banks. At least one bit SHALL be set.

  @retval EFI_SUCCESS           The bitmap in ActivePcrBank parameter is already active.
  @retval EFI_INVALID_PARAMETER One or more of the parameters are incorrect.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCG2_SET_ACTIVE_PCR_BANKS)(
  IN EFI_TCG2_PROTOCOL *This,
  IN UINT32            ActivePcrBanks
  );

/**
  This service retrieves the result of a previous invocation of SetActivePcrBanks.

  @param[in]  This              Indicates the calling context
  @param[out] OperationPresent  Non-zero value to indicate a SetActivePcrBank operation was invoked during the last boot.
  @param[out] Response          The response from the SetActivePcrBank request.

  @retval EFI_SUCCESS           The result value could be returned.
  @retval EFI_INVALID_PARAMETER One or more of the parameters are incorrect.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCG2_GET_RESULT_OF_SET_ACTIVE_PCR_BANKS)(
  IN  EFI_TCG2_PROTOCOL  *This,
  OUT UINT32             *OperationPresent,
  OUT UINT32             *Response
  );

struct tdEFI_TCG2_PROTOCOL {
  EFI_TCG2_GET_CAPABILITY                        GetCapability;
  EFI_TCG2_GET_EVENT_LOG                         GetEventLog;
  EFI_TCG2_HASH_LOG_EXTEND_EVENT                 HashLogExtendEvent;
  EFI_TCG2_SUBMIT_COMMAND                        SubmitCommand;
  EFI_TCG2_GET_ACTIVE_PCR_BANKS                  GetActivePcrBanks;
  EFI_TCG2_SET_ACTIVE_PCR_BANKS                  SetActivePcrBanks;
  EFI_TCG2_GET_RESULT_OF_SET_ACTIVE_PCR_BANKS    GetResultOfSetActivePcrBanks;
};

extern EFI_GUID  gEfiTcg2ProtocolGuid;

//
// Log entries after Get Event Log service
//

#define EFI_TCG2_FINAL_EVENTS_TABLE_GUID \
  {0x1e2ed096, 0x30e2, 0x4254, { 0xbd, 0x89, 0x86, 0x3b, 0xbe, 0xf8, 0x23, 0x25 }}

extern EFI_GUID  gEfiTcg2FinalEventsTableGuid;

typedef struct tdEFI_TCG2_FINAL_EVENTS_TABLE {
  //
  // The version of this structure.
  //
  UINT64    Version;
  //
  // Number of events recorded after invocation of GetEventLog API
  //
  UINT64    NumberOfEvents;
  //
  // List of events of type TCG_PCR_EVENT2.
  //
  // TCG_PCR_EVENT2          Event[1];
} EFI_TCG2_FINAL_EVENTS_TABLE;

#define EFI_TCG2_FINAL_EVENTS_TABLE_VERSION  1

#endif
