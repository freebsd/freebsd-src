/** @file
  Internal include file for UefiLib.

  Copyright (c) 2007 - 2017, Intel Corporation. All rights reserved.<BR>
   This program and the accompanying materials
   are licensed and made available under the terms and conditions of the BSD License
   which accompanies this distribution. The full text of the license may be found at
   http://opensource.org/licenses/bsd-license.php.
   THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
   WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef __UEFI_LIB_INTERNAL_H_
#define __UEFI_LIB_INTERNAL_H_


#include <Uefi.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/DriverConfiguration.h>
#include <Protocol/DriverConfiguration2.h>
#include <Protocol/DriverDiagnostics.h>
#include <Protocol/DriverDiagnostics2.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/UgaDraw.h>
#include <Protocol/HiiFont.h>

#include <Guid/EventGroup.h>
#include <Guid/EventLegacyBios.h>
#include <Guid/GlobalVariable.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/DevicePathLib.h>

#endif
