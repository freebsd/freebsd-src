/** @file
  This file declares Status Code PPI.
  This ppi provides a service that allows PEIMs to report status codes.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.0.

**/

#ifndef __STATUS_CODE_PPI_H__
#define __STATUS_CODE_PPI_H__

#define EFI_PEI_REPORT_PROGRESS_CODE_PPI_GUID \
  { 0x229832d3, 0x7a30, 0x4b36, {0xb8, 0x27, 0xf4, 0xc, 0xb7, 0xd4, 0x54, 0x36 } }

//
// EFI_PEI_PROGRESS_CODE_PPI.ReportStatusCode() is equivalent to the
// PEI Service ReportStatusCode().
// It is introduced in PIPeiCis.h.
//

///
/// This PPI provides the service to report status code.
/// There can be only one instance of this service in the system.
///
typedef struct {
  EFI_PEI_REPORT_STATUS_CODE    ReportStatusCode;
} EFI_PEI_PROGRESS_CODE_PPI;

extern EFI_GUID  gEfiPeiStatusCodePpiGuid;

#endif
