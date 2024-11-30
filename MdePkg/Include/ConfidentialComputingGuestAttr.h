/** @file
Definitions for Confidential Computing Guest Attributes

Copyright (c) 2021 AMD Inc. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CONFIDENTIAL_COMPUTING_GUEST_ATTR_H_
#define CONFIDENTIAL_COMPUTING_GUEST_ATTR_H_

//
// Confidential computing guest type
//
typedef enum {
  CcGuestTypeNonEncrypted = 0,
  CcGuestTypeAmdSev,
  CcGuestTypeIntelTdx,
} CC_GUEST_TYPE;

typedef enum {
  /* The guest is running with memory encryption disabled. */
  CCAttrNotEncrypted = 0,

  /* The guest is running with AMD SEV memory encryption enabled. */
  CCAttrAmdSev    = 0x100,
  CCAttrAmdSevEs  = 0x101,
  CCAttrAmdSevSnp = 0x102,

  /* The guest is running with Intel TDX memory encryption enabled. */
  CCAttrIntelTdx = 0x200,

  CCAttrTypeMask = 0x000000000000ffff,

  /* Features */

  /* The AMD SEV-ES DebugVirtualization feature is enabled in SEV_STATUS */
  CCAttrFeatureAmdSevEsDebugVirtualization = 0x0000000000010000,

  CCAttrFeatureMask = 0xffffffffffff0000,
} CONFIDENTIAL_COMPUTING_GUEST_ATTR;

#define _CC_GUEST_IS_TDX(x)  ((x) == CCAttrIntelTdx)
#define CC_GUEST_IS_TDX(x)   _CC_GUEST_IS_TDX((x) & CCAttrTypeMask)
#define _CC_GUEST_IS_SEV(x)  ((x) == CCAttrAmdSev || (x) == CCAttrAmdSevEs || (x) == CCAttrAmdSevSnp)
#define CC_GUEST_IS_SEV(x)   _CC_GUEST_IS_SEV((x) & CCAttrTypeMask)

#endif
