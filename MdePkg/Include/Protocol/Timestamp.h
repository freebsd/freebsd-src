/** @file
  EFI Timestamp Protocol as defined in UEFI2.4 Specification.
  Used to provide a platform independent interface for retrieving a high resolution timestamp counter.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.4

**/

#ifndef __EFI_TIME_STAMP_PROTOCOL_H__
#define __EFI_TIME_STAMP_PROTOCOL_H__


#define EFI_TIMESTAMP_PROTOCOL_GUID \
  { 0xafbfde41, 0x2e6e, 0x4262, {0xba, 0x65, 0x62, 0xb9, 0x23, 0x6e, 0x54, 0x95 } }

///
/// Declare forward reference for the Time Stamp Protocol
///
typedef struct _EFI_TIMESTAMP_PROTOCOL  EFI_TIMESTAMP_PROTOCOL;

///
/// EFI_TIMESTAMP_PROPERTIES
///
typedef struct {
  ///
  /// The frequency of the timestamp counter in Hz.
  ///
  UINT64                               Frequency;
  ///
  /// The value that the timestamp counter ends with immediately before it rolls over.
  /// For example, a 64-bit free running counter would have an EndValue of 0xFFFFFFFFFFFFFFFF.
  /// A 24-bit free running counter would have an EndValue of 0xFFFFFF.
  ///
  UINT64                               EndValue;
} EFI_TIMESTAMP_PROPERTIES;

/**
  Retrieves the current value of a 64-bit free running timestamp counter.

  The counter shall count up in proportion to the amount of time that has passed. The counter value
  will always roll over to zero. The properties of the counter can be retrieved from GetProperties().
  The caller should be prepared for the function to return the same value twice across successive calls.
  The counter value will not go backwards other than when wrapping, as defined by EndValue in GetProperties().
  The frequency of the returned timestamp counter value must remain constant. Power management operations that
  affect clocking must not change the returned counter frequency. The quantization of counter value updates may
  vary as long as the value reflecting time passed remains consistent.

  @param  None.

  @retval The current value of the free running timestamp counter.

**/
typedef
UINT64
(EFIAPI *TIMESTAMP_GET)(
  VOID
  );

/**
  Obtains timestamp counter properties including frequency and value limits.

  @param[out]  Properties              The properties of the timestamp counter.

  @retval      EFI_SUCCESS             The properties were successfully retrieved.
  @retval      EFI_DEVICE_ERROR        An error occurred trying to retrieve the properties of the timestamp
                                       counter subsystem. Properties is not pedated.
  @retval      EFI_INVALID_PARAMETER   Properties is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *TIMESTAMP_GET_PROPERTIES)(
  OUT   EFI_TIMESTAMP_PROPERTIES       *Properties
  );



///
/// EFI_TIMESTAMP_PROTOCOL
/// The protocol provides a platform independent interface for retrieving a high resolution
/// timestamp counter.
///
struct _EFI_TIMESTAMP_PROTOCOL {
  TIMESTAMP_GET                        GetTimestamp;
  TIMESTAMP_GET_PROPERTIES             GetProperties;
};

extern EFI_GUID gEfiTimestampProtocolGuid;

#endif

