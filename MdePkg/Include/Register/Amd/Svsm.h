/** @file
  Secure VM Service Module (SVSM) Definition.

  Provides data types allowing an SEV-SNP guest to interact with the SVSM.

  Copyright (C) 2024, Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
  Secure VM Service Module Specification

**/

#ifndef SVSM_H_
#define SVSM_H_

#include <Base.h>
#include <Uefi.h>
#include <Library/BaseLib.h>

//
// The SVSM definitions are part of the SNP Secrets Page:
//   An SVSM is considered present if the SvsmSize field is non-zero.
//
typedef PACKED struct {
  UINT8     Reserved1[320];

  UINT64    SvsmBase;
  UINT64    SvsmSize;
  UINT64    SvsmCaa;
  UINT32    SvsmMaxVersion;
  UINT8     SvsmGuestVmpl;
  UINT8     Reserved2[3];
} SVSM_INFORMATION;

typedef PACKED struct {
  UINT8    SvsmCallPending;
  UINT8    SvsmMemAvailable;
  UINT8    Reserved1[6];

  //
  // The remainder of the CAA 4KB area can be used for argument
  // passing to the SVSM.
  //
  UINT8    SvsmBuffer[SIZE_4KB - 8];
} SVSM_CAA;

#define SVSM_SUCCESS                   0x00000000
#define SVSM_ERR_INCOMPLETE            0x80000000
#define SVSM_ERR_UNSUPPORTED_PROTOCOL  0x80000001
#define SVSM_ERR_UNSUPPORTED_CALL      0x80000002
#define SVSM_ERR_INVALID_ADDRESS       0x80000003
#define SVSM_ERR_INVALID_FORMAT        0x80000004
#define SVSM_ERR_INVALID_PARAMETER     0x80000005
#define SVSM_ERR_INVALID_REQUEST       0x80000006
#define SVSM_ERR_BUSY                  0x80000007

#define SVSM_ERR_PVALIDATE_FAIL_INPUT          0x80001001
#define SVSM_ERR_PVALIDATE_FAIL_SIZE_MISMATCH  0x80001006
#define SVSM_ERR_PVALIDATE_FAIL_NO_CHANGE      0x80001010

typedef PACKED struct {
  UINT16    Entries;
  UINT16    Next;

  UINT8     Reserved[4];
} SVSM_PVALIDATE_HEADER;

typedef union {
  struct {
    UINT64    PageSize   : 2;
    UINT64    Action     : 1;
    UINT64    IgnoreCf   : 1;
    UINT64    Reserved_2 : 8;
    UINT64    Address    : 52;
  } Bits;
  UINT64    Uint64;
} SVSM_PVALIDATE_ENTRY;

typedef PACKED struct {
  SVSM_PVALIDATE_HEADER    Header;
  SVSM_PVALIDATE_ENTRY     Entry[];
} SVSM_PVALIDATE_REQUEST;

#define SVSM_PVALIDATE_MAX_ENTRY   \
  ((sizeof (((SVSM_CAA *)0)->SvsmBuffer) - sizeof (SVSM_PVALIDATE_HEADER)) / sizeof (SVSM_PVALIDATE_ENTRY))

typedef union {
  SVSM_PVALIDATE_REQUEST    PvalidateRequest;
} SVSM_REQUEST;

typedef union {
  struct {
    UINT32    CallId;
    UINT32    Protocol;
  } Id;

  UINT64    Uint64;
} SVSM_FUNCTION;

#endif
