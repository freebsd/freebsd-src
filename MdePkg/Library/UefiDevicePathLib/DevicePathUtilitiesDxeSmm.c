/** @file
  Device Path services. The thing to remember is device paths are built out of
  nodes. The device path is terminated by an end node that is length
  sizeof(EFI_DEVICE_PATH_PROTOCOL). That would be why there is sizeof(EFI_DEVICE_PATH_PROTOCOL)
  all over this file.

  The only place where multi-instance device paths are supported is in
  environment varibles. Multi-instance device paths should never be placed
  on a Handle.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UefiDevicePathLib.h"

/**
  Retrieves the device path protocol from a handle.

  This function returns the device path protocol from the handle specified by Handle.
  If Handle is NULL or Handle does not contain a device path protocol, then NULL
  is returned.

  @param  Handle                     The handle from which to retrieve the device
                                     path protocol.

  @return The device path protocol from the handle specified by Handle.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
DevicePathFromHandle (
  IN EFI_HANDLE  Handle
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_STATUS                Status;

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID *)&DevicePath
                  );
  if (EFI_ERROR (Status)) {
    DevicePath = NULL;
  }

  return DevicePath;
}
