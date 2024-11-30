/** @file
  If CC Guest firmware supports measurement and an event is created,
  CC Guest firmware is designed to report the event log with the same
  data structure in TCG-Platform-Firmware-Profile specification with
  EFI_TCG2_EVENT_LOG_FORMAT_TCG_2 format.

  The CC Guest firmware supports measurement, the CC Guest Firmware is
  designed to produce EFI_CC_MEASUREMENT_PROTOCOL with new GUID
  EFI_CC_MEASUREMENT_PROTOCOL_GUID to report event log and provides hash
  capability.

Copyright (c) 2020 - 2021, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CC_MEASUREMENT_PROTOCOL_H_
#define CC_MEASUREMENT_PROTOCOL_H_

#include <IndustryStandard/UefiTcgPlatform.h>

#define EFI_CC_MEASUREMENT_PROTOCOL_GUID  \
  { 0x96751a3d, 0x72f4, 0x41a6, { 0xa7, 0x94, 0xed, 0x5d, 0x0e, 0x67, 0xae, 0x6b }}
extern EFI_GUID  gEfiCcMeasurementProtocolGuid;

typedef struct _EFI_CC_MEASUREMENT_PROTOCOL EFI_CC_MEASUREMENT_PROTOCOL;

typedef struct {
  UINT8    Major;
  UINT8    Minor;
} EFI_CC_VERSION;

//
// EFI_CC Type/SubType definition
//
#define EFI_CC_TYPE_NONE  0
#define EFI_CC_TYPE_SEV   1
#define EFI_CC_TYPE_TDX   2

typedef struct {
  UINT8    Type;
  UINT8    SubType;
} EFI_CC_TYPE;

typedef UINT32 EFI_CC_EVENT_LOG_BITMAP;
typedef UINT32 EFI_CC_EVENT_LOG_FORMAT;
typedef UINT32 EFI_CC_EVENT_ALGORITHM_BITMAP;
typedef UINT32 EFI_CC_MR_INDEX;

//
// Intel TDX measure register index
//
#define TDX_MR_INDEX_MRTD   0
#define TDX_MR_INDEX_RTMR0  1
#define TDX_MR_INDEX_RTMR1  2
#define TDX_MR_INDEX_RTMR2  3
#define TDX_MR_INDEX_RTMR3  4

#define EFI_CC_EVENT_LOG_FORMAT_TCG_2  0x00000002
#define EFI_CC_BOOT_HASH_ALG_SHA384    0x00000004

//
// This bit is shall be set when an event shall be extended but not logged.
//
#define EFI_CC_FLAG_EXTEND_ONLY  0x0000000000000001
//
// This bit shall be set when the intent is to measure a PE/COFF image.
//
#define EFI_CC_FLAG_PE_COFF_IMAGE  0x0000000000000010

#pragma pack (1)

#define EFI_CC_EVENT_HEADER_VERSION  1

typedef struct {
  //
  // Size of the event header itself (sizeof(EFI_CC_EVENT_HEADER)).
  //
  UINT32             HeaderSize;
  //
  // Header version. For this version of this specification, the value shall be 1.
  //
  UINT16             HeaderVersion;
  //
  // Index of the MR (measurement register) that shall be extended.
  //
  EFI_CC_MR_INDEX    MrIndex;
  //
  // Type of the event that shall be extended (and optionally logged).
  //
  UINT32             EventType;
} EFI_CC_EVENT_HEADER;

typedef struct {
  //
  // Total size of the event including the Size component, the header and the Event data.
  //
  UINT32                 Size;
  EFI_CC_EVENT_HEADER    Header;
  UINT8                  Event[1];
} EFI_CC_EVENT;

#pragma pack()

typedef struct {
  //
  // Allocated size of the structure
  //
  UINT8                            Size;
  //
  // Version of the EFI_CC_BOOT_SERVICE_CAPABILITY structure itself.
  // For this version of the protocol, the Major version shall be set to 1
  // and the Minor version shall be set to 0.
  //
  EFI_CC_VERSION                   StructureVersion;
  //
  // Version of the EFI CC Measurement protocol.
  // For this version of the protocol, the Major version shall be set to 1
  // and the Minor version shall be set to 0.
  //
  EFI_CC_VERSION                   ProtocolVersion;
  //
  // Supported hash algorithms
  //
  EFI_CC_EVENT_ALGORITHM_BITMAP    HashAlgorithmBitmap;
  //
  // Bitmap of supported event log formats
  //
  EFI_CC_EVENT_LOG_BITMAP          SupportedEventLogs;

  //
  // Indicates the CC type
  //
  EFI_CC_TYPE                      CcType;
} EFI_CC_BOOT_SERVICE_CAPABILITY;

/**
  The EFI_CC_MEASUREMENT_PROTOCOL GetCapability function call provides protocol
  capability information and state information.

  @param[in]      This               Indicates the calling context
  @param[in, out] ProtocolCapability The caller allocates memory for a EFI_CC_BOOT_SERVICE_CAPABILITY
                                     structure and sets the size field to the size of the structure allocated.
                                     The callee fills in the fields with the EFI CC BOOT Service capability
                                     information and the current CC information.

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
(EFIAPI *EFI_CC_GET_CAPABILITY)(
  IN     EFI_CC_MEASUREMENT_PROTOCOL       *This,
  IN OUT EFI_CC_BOOT_SERVICE_CAPABILITY    *ProtocolCapability
  );

/**
  The EFI_CC_MEASUREMENT_PROTOCOL Get Event Log function call allows a caller to
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
(EFIAPI *EFI_CC_GET_EVENT_LOG)(
  IN  EFI_CC_MEASUREMENT_PROTOCOL     *This,
  IN  EFI_CC_EVENT_LOG_FORMAT         EventLogFormat,
  OUT EFI_PHYSICAL_ADDRESS            *EventLogLocation,
  OUT EFI_PHYSICAL_ADDRESS            *EventLogLastEntry,
  OUT BOOLEAN                         *EventLogTruncated
  );

/**
  The EFI_CC_MEASUREMENT_PROTOCOL HashLogExtendEvent function call provides
  callers with an opportunity to extend and optionally log events without requiring
  knowledge of actual CC commands.
  The extend operation will occur even if this function cannot create an event
  log entry (e.g. due to the event log being full).

  @param[in]  This               Indicates the calling context
  @param[in]  Flags              Bitmap providing additional information.
  @param[in]  DataToHash         Physical address of the start of the data buffer to be hashed.
  @param[in]  DataToHashLen      The length in bytes of the buffer referenced by DataToHash.
  @param[in]  EfiCcEvent        Pointer to data buffer containing information about the event.

  @retval EFI_SUCCESS            Operation completed successfully.
  @retval EFI_DEVICE_ERROR       The command was unsuccessful.
  @retval EFI_VOLUME_FULL        The extend operation occurred, but the event could not be written to one or more event logs.
  @retval EFI_INVALID_PARAMETER  One or more of the parameters are incorrect.
  @retval EFI_UNSUPPORTED        The PE/COFF image type is not supported.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CC_HASH_LOG_EXTEND_EVENT)(
  IN EFI_CC_MEASUREMENT_PROTOCOL    *This,
  IN UINT64                         Flags,
  IN EFI_PHYSICAL_ADDRESS           DataToHash,
  IN UINT64                         DataToHashLen,
  IN EFI_CC_EVENT                   *EfiCcEvent
  );

/**
  The EFI_CC_MEASUREMENT_PROTOCOL MapPcrToMrIndex function call provides callers
  the info on TPM PCR <-> CC MR mapping information.

  @param[in]  This               Indicates the calling context
  @param[in]  PcrIndex           TPM PCR index.
  @param[out] MrIndex            CC MR index.

  @retval EFI_SUCCESS            The MrIndex is returned.
  @retval EFI_INVALID_PARAMETER  The MrIndex is NULL.
  @retval EFI_UNSUPPORTED        The PcrIndex is invalid.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CC_MAP_PCR_TO_MR_INDEX)(
  IN  EFI_CC_MEASUREMENT_PROTOCOL    *This,
  IN  TCG_PCRINDEX                   PcrIndex,
  OUT EFI_CC_MR_INDEX                *MrIndex
  );

struct _EFI_CC_MEASUREMENT_PROTOCOL {
  EFI_CC_GET_CAPABILITY           GetCapability;
  EFI_CC_GET_EVENT_LOG            GetEventLog;
  EFI_CC_HASH_LOG_EXTEND_EVENT    HashLogExtendEvent;
  EFI_CC_MAP_PCR_TO_MR_INDEX      MapPcrToMrIndex;
};

//
// CC event log
//

#pragma pack(1)

//
// Crypto Agile Log Entry Format.
// It is similar with TCG_PCR_EVENT2 except the field of MrIndex and PCRIndex.
//
typedef struct {
  EFI_CC_MR_INDEX       MrIndex;
  UINT32                EventType;
  TPML_DIGEST_VALUES    Digests;
  UINT32                EventSize;
  UINT8                 Event[1];
} CC_EVENT;

//
// EFI CC Event Header
// It is similar with TCG_PCR_EVENT2_HDR except the field of MrIndex and PCRIndex
//
typedef struct {
  EFI_CC_MR_INDEX       MrIndex;
  UINT32                EventType;
  TPML_DIGEST_VALUES    Digests;
  UINT32                EventSize;
} CC_EVENT_HDR;

#pragma pack()

//
// Log entries after Get Event Log service
//

#define EFI_CC_FINAL_EVENTS_TABLE_VERSION  1

typedef struct {
  //
  // The version of this structure. It shall be set to 1.
  //
  UINT64    Version;
  //
  // Number of events recorded after invocation of GetEventLog API
  //
  UINT64    NumberOfEvents;
  //
  // List of events of type CC_EVENT.
  //
  // CC_EVENT              Event[1];
} EFI_CC_FINAL_EVENTS_TABLE;

#define EFI_CC_FINAL_EVENTS_TABLE_GUID \
  {0xdd4a4648, 0x2de7, 0x4665, {0x96, 0x4d, 0x21, 0xd9, 0xef, 0x5f, 0xb4, 0x46}}

extern EFI_GUID  gEfiCcFinalEventsTableGuid;

//
// Define the CC Measure EventLog ACPI Table
//
#pragma pack(1)

typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER    Header;
  EFI_CC_TYPE                    CcType;
  UINT16                         Rsvd;
  UINT64                         Laml;
  UINT64                         Lasa;
} EFI_CC_EVENTLOG_ACPI_TABLE;

#pragma pack()

//
// Define the signature and revision of CC Measurement EventLog ACPI Table
//
#define EFI_CC_EVENTLOG_ACPI_TABLE_SIGNATURE  SIGNATURE_32('C', 'C', 'E', 'L')
#define EFI_CC_EVENTLOG_ACPI_TABLE_REVISION   1

#endif
