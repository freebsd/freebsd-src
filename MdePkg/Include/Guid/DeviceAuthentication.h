/** @file
  Guid & data structure used for Device Security.

  Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef EFI_DEVICE_AUTHENTICATION_GUID_H_
#define EFI_DEVICE_AUTHENTICATION_GUID_H_

/**
  This is a signature database for device authentication, instead of image authentication.

  The content of the signature database is same as the one in db/dbx. (a list of EFI_SIGNATURE_LIST)
**/
#define EFI_DEVICE_SIGNATURE_DATABASE_GUID \
  {0xb9c2b4f4, 0xbf5f, 0x462d, 0x8a, 0xdf, 0xc5, 0xc7, 0xa, 0xc3, 0x5d, 0xad}
#define EFI_DEVICE_SECURITY_DATABASE  L"devdb"

extern EFI_GUID  gEfiDeviceSignatureDatabaseGuid;

/**
  Signature Database:

  +---------------------------------------+ <-----------------
  | SignatureType (GUID)                  |                  |
  +---------------------------------------+                  |
  | SignatureListSize (UINT32)            |                  |
  +---------------------------------------+                  |
  | SignatureHeaderSize (UINT32)          |                  |
  +---------------------------------------+                  |
  | SignatureSize (UINT32)                |                  |-EFI_SIGNATURE_LIST (1)
  +---------------------------------------+                  |
  | SignatureHeader (SignatureHeaderSize) |                  |
  +---------------------------------------+ <--              |
  | SignatureOwner (GUID)                 |   |              |
  +---------------------------------------+   |-EFI_SIGNATURE_DATA (1)
  | SignatureData (SignatureSize - 16)    |   |              |
  +---------------------------------------+ <--              |
  | SignatureOwner (GUID)                 |   |              |
  +---------------------------------------+   |-EFI_SIGNATURE_DATA (n)
  | SignatureData (SignatureSize - 16)    |   |              |
  +---------------------------------------+ <-----------------
  | SignatureType (GUID)                  |                  |
  +---------------------------------------+                  |
  | SignatureListSize (UINT32)            |                  |-EFI_SIGNATURE_LIST (n)
  +---------------------------------------+                  |
  | ...                                   |                  |
  +---------------------------------------+ <-----------------

  SignatureType := EFI_CERT_SHAxxx_GUID |
                   EFI_CERT_RSA2048_GUID |
                   EFI_CERT_RSA2048_SHAxxx_GUID |
                   EFI_CERT_X509_GUID |
                   EFI_CERT_X509_SHAxxx_GUID
  (xxx = 256, 384, 512)

**/

#endif
