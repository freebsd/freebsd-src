/** @file
  S3 SMM Save State Protocol as defined in PI1.2 Specification VOLUME 5 Standard.

  The EFI_S3_SMM_SAVE_STATE_PROTOCOL publishes the PI SMMboot script abstractions
  On an S3 resume boot path the data stored via this protocol is replayed in the order it was stored.
  The order of replay is the order either of the S3 Save State Protocol or S3 SMM Save State Protocol
  Write() functions were called during the boot process. Insert(), Label(), and
  Compare() operations are ordered relative other S3 SMM Save State Protocol write() operations
  and the order relative to S3 State Save Write() operations is not defined. Due to these ordering
  restrictions it is recommended that the S3 State Save Protocol be used during the DXE phase when
  every possible.
  The EFI_S3_SMM_SAVE_STATE_PROTOCOL can be called at runtime and
  EFI_OUT_OF_RESOURCES may be returned from a runtime call. It is the responsibility of the
  platform to ensure enough memory resource exists to save the system state. It is recommended that
  runtime calls be minimized by the caller.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is defined in UEFI Platform Initialization Specification 1.2 Volume 5:
  Standards

**/

#ifndef __S3_SMM_SAVE_STATE_H__
#define __S3_SMM_SAVE_STATE_H__

#include <Protocol/S3SaveState.h>

#define EFI_S3_SMM_SAVE_STATE_PROTOCOL_GUID \
    {0x320afe62, 0xe593, 0x49cb, { 0xa9, 0xf1, 0xd4, 0xc2, 0xf4, 0xaf, 0x1, 0x4c }}

typedef EFI_S3_SAVE_STATE_PROTOCOL EFI_S3_SMM_SAVE_STATE_PROTOCOL;

extern EFI_GUID  gEfiS3SmmSaveStateProtocolGuid;

#endif // __S3_SMM_SAVE_STATE_H__
