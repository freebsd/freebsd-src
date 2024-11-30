/** @file
  This protocol is defined to abstract TPM2 hardware access in boot phase.

Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __TREE_H__
#define __TREE_H__

#include <IndustryStandard/UefiTcgPlatform.h>
#include <IndustryStandard/Tpm20.h>

#define EFI_TREE_PROTOCOL_GUID \
  {0x607f766c, 0x7455, 0x42be, 0x93, 0x0b, 0xe4, 0xd7, 0x6d, 0xb2, 0x72, 0x0f}

typedef struct _EFI_TREE_PROTOCOL EFI_TREE_PROTOCOL;

typedef struct _TREE_VERSION {
  UINT8    Major;
  UINT8    Minor;
} TREE_VERSION;

typedef UINT32 TREE_EVENT_LOG_BITMAP;
typedef UINT32 TREE_EVENT_LOG_FORMAT;

#define TREE_EVENT_LOG_FORMAT_TCG_1_2  0x00000001

typedef struct _TREE_BOOT_SERVICE_CAPABILITY {
  //
  // Allocated size of the structure passed in
  //
  UINT8                    Size;
  //
  // Version of the TREE_BOOT_SERVICE_CAPABILITY structure itself.
  // For this version of the protocol, the Major version shall be set to 1
  // and the Minor version shall be set to 0.
  //
  TREE_VERSION             StructureVersion;
  //
  // Version of the TrEE protocol.
  // For this version of the protocol, the Major version shall be set to 1
  // and the Minor version shall be set to 0.
  //
  TREE_VERSION             ProtocolVersion;
  //
  // Supported hash algorithms
  //
  UINT32                   HashAlgorithmBitmap;
  //
  // Bitmap of supported event log formats
  //
  TREE_EVENT_LOG_BITMAP    SupportedEventLogs;
  //
  // False = TrEE not present
  //
  BOOLEAN                  TrEEPresentFlag;
  //
  // Max size (in bytes) of a command that can be sent to the TrEE
  //
  UINT16                   MaxCommandSize;
  //
  // Max size (in bytes) of a response that can be provided by the TrEE
  //
  UINT16                   MaxResponseSize;
  //
  // 4-byte Vendor ID (see Trusted Computing Group, "TCG Vendor ID Registry,"
  // Version 1.0, Revision 0.1, August 31, 2007, "TPM Capabilities Vendor ID" section)
  //
  UINT32                   ManufacturerID;
} TREE_BOOT_SERVICE_CAPABILITY_1_0;

typedef TREE_BOOT_SERVICE_CAPABILITY_1_0 TREE_BOOT_SERVICE_CAPABILITY;

#define TREE_BOOT_HASH_ALG_SHA1    0x00000001
#define TREE_BOOT_HASH_ALG_SHA256  0x00000002
#define TREE_BOOT_HASH_ALG_SHA384  0x00000004
#define TREE_BOOT_HASH_ALG_SHA512  0x00000008

//
// This bit is shall be set when an event shall be extended but not logged.
//
#define TREE_EXTEND_ONLY  0x0000000000000001
//
// This bit shall be set when the intent is to measure a PE/COFF image.
//
#define PE_COFF_IMAGE  0x0000000000000010

typedef UINT32 TrEE_PCRINDEX;
typedef UINT32 TrEE_EVENTTYPE;

#define MAX_PCR_INDEX              23
#define TREE_EVENT_HEADER_VERSION  1

#pragma pack(1)

typedef struct {
  //
  // Size of the event header itself (sizeof(TrEE_EVENT_HEADER)).
  //
  UINT32            HeaderSize;
  //
  // Header version. For this version of this specification, the value shall be 1.
  //
  UINT16            HeaderVersion;
  //
  // Index of the PCR that shall be extended (0 - 23).
  //
  TrEE_PCRINDEX     PCRIndex;
  //
  // Type of the event that shall be extended (and optionally logged).
  //
  TrEE_EVENTTYPE    EventType;
} TrEE_EVENT_HEADER;

typedef struct {
  //
  // Total size of the event including the Size component, the header and the Event data.
  //
  UINT32               Size;
  TrEE_EVENT_HEADER    Header;
  UINT8                Event[1];
} TrEE_EVENT;

#pragma pack()

/**
  The EFI_TREE_PROTOCOL GetCapability function call provides protocol
  capability information and state information about the TrEE.

  @param[in]  This               Indicates the calling context
  @param[out] ProtocolCapability The caller allocates memory for a TREE_BOOT_SERVICE_CAPABILITY
                                 structure and sets the size field to the size of the structure allocated.
                                 The callee fills in the fields with the EFI protocol capability information
                                 and the current TrEE state information up to the number of fields which
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
(EFIAPI *EFI_TREE_GET_CAPABILITY)(
  IN EFI_TREE_PROTOCOL                *This,
  IN OUT TREE_BOOT_SERVICE_CAPABILITY *ProtocolCapability
  );

/**
  The EFI_TREE_PROTOCOL Get Event Log function call allows a caller to
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
(EFIAPI *EFI_TREE_GET_EVENT_LOG)(
  IN EFI_TREE_PROTOCOL     *This,
  IN TREE_EVENT_LOG_FORMAT EventLogFormat,
  OUT EFI_PHYSICAL_ADDRESS *EventLogLocation,
  OUT EFI_PHYSICAL_ADDRESS *EventLogLastEntry,
  OUT BOOLEAN              *EventLogTruncated
  );

/**
  The EFI_TREE_PROTOCOL HashLogExtendEvent function call provides callers with
  an opportunity to extend and optionally log events without requiring
  knowledge of actual TPM commands.
  The extend operation will occur even if this function cannot create an event
  log entry (e.g. due to the event log being full).

  @param[in]  This               Indicates the calling context
  @param[in]  Flags              Bitmap providing additional information.
  @param[in]  DataToHash         Physical address of the start of the data buffer to be hashed.
  @param[in]  DataToHashLen      The length in bytes of the buffer referenced by DataToHash.
  @param[in]  Event              Pointer to data buffer containing information about the event.

  @retval EFI_SUCCESS            Operation completed successfully.
  @retval EFI_DEVICE_ERROR       The command was unsuccessful.
  @retval EFI_VOLUME_FULL        The extend operation occurred, but the event could not be written to one or more event logs.
  @retval EFI_INVALID_PARAMETER  One or more of the parameters are incorrect.
  @retval EFI_UNSUPPORTED        The PE/COFF image type is not supported.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TREE_HASH_LOG_EXTEND_EVENT)(
  IN EFI_TREE_PROTOCOL    *This,
  IN UINT64               Flags,
  IN EFI_PHYSICAL_ADDRESS DataToHash,
  IN UINT64               DataToHashLen,
  IN TrEE_EVENT           *Event
  );

/**
  This service enables the sending of commands to the TrEE.

  @param[in]  This                     Indicates the calling context
  @param[in]  InputParameterBlockSize  Size of the TrEE input parameter block.
  @param[in]  InputParameterBlock      Pointer to the TrEE input parameter block.
  @param[in]  OutputParameterBlockSize Size of the TrEE output parameter block.
  @param[in]  OutputParameterBlock     Pointer to the TrEE output parameter block.

  @retval EFI_SUCCESS            The command byte stream was successfully sent to the device and a response was successfully received.
  @retval EFI_DEVICE_ERROR       The command was not successfully sent to the device or a response was not successfully received from the device.
  @retval EFI_INVALID_PARAMETER  One or more of the parameters are incorrect.
  @retval EFI_BUFFER_TOO_SMALL   The output parameter block is too small.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TREE_SUBMIT_COMMAND)(
  IN EFI_TREE_PROTOCOL *This,
  IN UINT32            InputParameterBlockSize,
  IN UINT8             *InputParameterBlock,
  IN UINT32            OutputParameterBlockSize,
  IN UINT8             *OutputParameterBlock
  );

struct _EFI_TREE_PROTOCOL {
  EFI_TREE_GET_CAPABILITY           GetCapability;
  EFI_TREE_GET_EVENT_LOG            GetEventLog;
  EFI_TREE_HASH_LOG_EXTEND_EVENT    HashLogExtendEvent;
  EFI_TREE_SUBMIT_COMMAND           SubmitCommand;
};

extern EFI_GUID  gEfiTrEEProtocolGuid;

#endif
