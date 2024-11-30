/** @file
  EFI MM Status Code Protocol as defined in the PI 1.5 specification.

  This protocol provides the basic status code services while in MM.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _MM_STATUS_CODE_H__
#define _MM_STATUS_CODE_H__

#define EFI_MM_STATUS_CODE_PROTOCOL_GUID \
  { \
    0x6afd2b77, 0x98c1, 0x4acd, {0xa6, 0xf9, 0x8a, 0x94, 0x39, 0xde, 0xf, 0xb1} \
  }

typedef struct _EFI_MM_STATUS_CODE_PROTOCOL EFI_MM_STATUS_CODE_PROTOCOL;

/**
  Service to emit the status code in MM.

  The EFI_MM_STATUS_CODE_PROTOCOL.ReportStatusCode() function enables a driver
  to emit a status code while in MM.  The reason that there is a separate protocol definition from the
  DXE variant of this service is that the publisher of this protocol will provide a service that is
  capability of coexisting with a foreground operational environment, such as an operating system
  after the termination of boot services.

  @param[in] This                Points to this instance of the EFI_MM_STATUS_CODE_PROTOCOL.
  @param[in] CodeType            DIndicates the type of status code being reported.
  @param[in] Value               Describes the current status of a hardware or software entity.
  @param[in] Instance            The enumeration of a hardware or software entity within the system.
  @param[in] CallerId            This optional parameter may be used to identify the caller.
  @param[in] Data                This optional parameter may be used to pass additional data.

  @retval EFI_SUCCESS            The function completed successfully.
  @retval EFI_INVALID_PARAMETER  The function should not be completed due to a device error.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MM_REPORT_STATUS_CODE)(
  IN CONST EFI_MM_STATUS_CODE_PROTOCOL   *This,
  IN EFI_STATUS_CODE_TYPE                CodeType,
  IN EFI_STATUS_CODE_VALUE               Value,
  IN UINT32                              Instance,
  IN CONST EFI_GUID                      *CallerId,
  IN EFI_STATUS_CODE_DATA                *Data OPTIONAL
  );

struct _EFI_MM_STATUS_CODE_PROTOCOL {
  EFI_MM_REPORT_STATUS_CODE    ReportStatusCode;
};

extern EFI_GUID  gEfiMmStatusCodeProtocolGuid;

#endif
