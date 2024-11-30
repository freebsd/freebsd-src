/** @file
  Support for Microsoft Secure MOR implementation, defined at
  Microsoft Secure MOR implementation.
  https://msdn.microsoft.com/en-us/library/windows/hardware/mt270973(v=vs.85).aspx

  Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MEMORY_OVERWRITE_REQUEST_CONTROL_LOCK_H__
#define __MEMORY_OVERWRITE_REQUEST_CONTROL_LOCK_H__

#define MEMORY_OVERWRITE_REQUEST_CONTROL_LOCK_GUID \
  { \
    0xBB983CCF, 0x151D, 0x40E1, {0xA0, 0x7B, 0x4A, 0x17, 0xBE, 0x16, 0x82, 0x92} \
  }

#define MEMORY_OVERWRITE_REQUEST_CONTROL_LOCK_NAME  L"MemoryOverwriteRequestControlLock"

//
// VendorGuid: {BB983CCF-151D-40E1-A07B-4A17BE168292}
// Name:       MemoryOverwriteRequestControlLock
// Attributes: NV+BS+RT
// GetVariable value in Data parameter: 0x0 (unlocked); 0x1 (locked without key); 0x2 (locked with key)
// SetVariable value in Data parameter: 0x0 (unlocked); 0x1 (locked);
//                                      Revision 2 additionally accepts an 8-byte value that represents a shared secret key.
//

//
// Note: Setting MemoryOverwriteRequestControlLock does not commit to flash (just changes the internal lock state).
// Getting the variable returns the internal state and never exposes the key.
//

extern EFI_GUID  gEfiMemoryOverwriteRequestControlLockGuid;

#endif
