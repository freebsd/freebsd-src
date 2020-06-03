/** @file
  Provides a service to retrieve a pointer to the Standalone MM Services Table.
  Only available to MM_STANDALONE, SMM/DXE Combined and SMM module types.

Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2016 - 2018, ARM Limited. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MM_SERVICES_TABLE_LIB_H__
#define __MM_SERVICES_TABLE_LIB_H__

#include <PiMm.h>

extern EFI_MM_SYSTEM_TABLE         *gMmst;

#endif
