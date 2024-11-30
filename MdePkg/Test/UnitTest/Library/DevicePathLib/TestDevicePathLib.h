/** @file
  UEFI OS based application for unit testing the DevicePathLib.

  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __TEST_DEVICE_PATH_LIB_H__
#define __TEST_DEVICE_PATH_LIB_H__

#include <PiPei.h>
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UnitTestLib.h>
#include <Protocol/DevicePath.h>
#include <Library/DevicePathLib.h>
#include <stdint.h>

EFI_STATUS
CreateDevicePathStringConversionsTestSuite (
  IN UNIT_TEST_FRAMEWORK_HANDLE  Framework
  );

#endif
