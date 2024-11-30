/** @file
  GUID used as an FV filename for A Priori file. The A Priori file contains a
  list of FV filenames that the DXE dispatcher will schedule reguardless of
  the dependency grammar.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  GUID introduced in PI Version 1.0.

**/

#ifndef __APRIORI_GUID_H__
#define __APRIORI_GUID_H__

#define EFI_APRIORI_GUID \
  { \
    0xfc510ee7, 0xffdc, 0x11d4, {0xbd, 0x41, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 } \
  }

extern EFI_GUID  gAprioriGuid;

#endif
