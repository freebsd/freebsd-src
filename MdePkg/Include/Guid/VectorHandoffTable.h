/** @file
  GUID for system configuration table entry that points to the table
  in case an entity in DXE wishes to update/change the vector table contents.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  GUID defined in PI 1.2.1 spec.
**/

#ifndef __EFI_VECTOR_HANDOFF_TABLE_H__
#define __EFI_VECTOR_HANDOFF_TABLE_H__

#include <Ppi/VectorHandoffInfo.h>

//
// System configuration table entry that points to the table
// in case an entity in DXE wishes to update/change the vector
// table contents.
//
#define EFI_VECTOR_HANDOF_TABLE_GUID \
  { 0x996ec11c, 0x5397, 0x4e73, { 0xb5, 0x8f, 0x82, 0x7e, 0x52, 0x90, 0x6d, 0xef }}

extern EFI_GUID gEfiVectorHandoffTableGuid;

#endif
