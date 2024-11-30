/** @file
  GUID definition for the Linux Initrd media device path

  Linux distro boot generally relies on an initial ramdisk (initrd) which is
  provided by the loader, and which contains additional kernel modules (for
  storage and network, for instance), and the initial user space startup code,
  i.e., the code which brings up the user space side of the entire OS.

  In order to provide a standard method to locate this initrd, the GUID defined
  in this file is used to describe the device path for a LoadFile2 Protocol
  instance that is responsible for loading the initrd file.

  The kernel EFI Stub will locate and use this instance to load the initrd,
  therefore the firmware/loader should install an instance of this to load the
  relevant initrd.

  Copyright (c) 2020, Arm, Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef LINUX_EFI_INITRD_MEDIA_GUID_H_
#define LINUX_EFI_INITRD_MEDIA_GUID_H_

#define LINUX_EFI_INITRD_MEDIA_GUID \
  {0x5568e427, 0x68fc, 0x4f3d, {0xac, 0x74, 0xca, 0x55, 0x52, 0x31, 0xcc, 0x68}}

extern EFI_GUID  gLinuxEfiInitrdMediaGuid;

#endif
