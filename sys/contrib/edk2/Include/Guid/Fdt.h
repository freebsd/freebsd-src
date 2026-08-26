/** @file
*
*  Copyright (c) 2013-2014, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __FDT_H__
#define __FDT_H__

#define FDT_TABLE_GUID \
  { 0xb1b621d5, 0xf19c, 0x41a5, { 0x83, 0x0b, 0xd9, 0x15, 0x2c, 0x69, 0xaa, 0xe0 } }

extern EFI_GUID  gFdtTableGuid;

#define FDT_VARIABLE_GUID \
  { 0x25a4fd4a, 0x9703, 0x4ba9, { 0xa1, 0x90, 0xb7, 0xc8, 0x4e, 0xfb, 0x3e, 0x57 } }

extern EFI_GUID  gFdtVariableGuid;

#endif /* __FDT_H__ */
