/** @file
SMRAM Save State Map Definitions.

SMRAM Save State Map definitions based on contents of the
Intel(R) 64 and IA-32 Architectures Software Developer's Manual
  Volume 3C, Section 34.4 SMRAM
  Volume 3C, Section 34.5 SMI Handler Execution Environment
  Volume 3C, Section 34.7 Managing Synchronous and Asynchronous SMIs

Copyright (c) 2015 - 2019, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __INTEL_SMRAM_SAVE_STATE_MAP_H__
#define __INTEL_SMRAM_SAVE_STATE_MAP_H__

///
/// Default SMBASE address
///
#define SMM_DEFAULT_SMBASE           0x30000

///
/// Offset of SMM handler from SMBASE
///
#define SMM_HANDLER_OFFSET           0x8000

///
/// Offset of SMRAM Save State Map from SMBASE
///
#define SMRAM_SAVE_STATE_MAP_OFFSET  0xfc00

#pragma pack (1)

///
/// 32-bit SMRAM Save State Map
///
typedef struct {
  UINT8   Reserved[0x200];  // 7c00h
                            // Padded an extra 0x200 bytes so 32-bit and 64-bit
                            // SMRAM Save State Maps are the same size
  UINT8   Reserved1[0xf8];  // 7e00h
  UINT32  SMBASE;           // 7ef8h
  UINT32  SMMRevId;         // 7efch
  UINT16  IORestart;        // 7f00h
  UINT16  AutoHALTRestart;  // 7f02h
  UINT8   Reserved2[0x9C];  // 7f08h
  UINT32  IOMemAddr;        // 7fa0h
  UINT32  IOMisc;           // 7fa4h
  UINT32  _ES;              // 7fa8h
  UINT32  _CS;              // 7fach
  UINT32  _SS;              // 7fb0h
  UINT32  _DS;              // 7fb4h
  UINT32  _FS;              // 7fb8h
  UINT32  _GS;              // 7fbch
  UINT32  Reserved3;        // 7fc0h
  UINT32  _TR;              // 7fc4h
  UINT32  _DR7;             // 7fc8h
  UINT32  _DR6;             // 7fcch
  UINT32  _EAX;             // 7fd0h
  UINT32  _ECX;             // 7fd4h
  UINT32  _EDX;             // 7fd8h
  UINT32  _EBX;             // 7fdch
  UINT32  _ESP;             // 7fe0h
  UINT32  _EBP;             // 7fe4h
  UINT32  _ESI;             // 7fe8h
  UINT32  _EDI;             // 7fech
  UINT32  _EIP;             // 7ff0h
  UINT32  _EFLAGS;          // 7ff4h
  UINT32  _CR3;             // 7ff8h
  UINT32  _CR0;             // 7ffch
} SMRAM_SAVE_STATE_MAP32;

///
/// 64-bit SMRAM Save State Map
///
typedef struct {
  UINT8   Reserved1[0x1d0];  // 7c00h
  UINT32  GdtBaseHiDword;    // 7dd0h
  UINT32  LdtBaseHiDword;    // 7dd4h
  UINT32  IdtBaseHiDword;    // 7dd8h
  UINT8   Reserved2[0xc];    // 7ddch
  UINT64  IO_EIP;            // 7de8h
  UINT8   Reserved3[0x50];   // 7df0h
  UINT32  _CR4;              // 7e40h
  UINT8   Reserved4[0x48];   // 7e44h
  UINT32  GdtBaseLoDword;    // 7e8ch
  UINT32  Reserved5;         // 7e90h
  UINT32  IdtBaseLoDword;    // 7e94h
  UINT32  Reserved6;         // 7e98h
  UINT32  LdtBaseLoDword;    // 7e9ch
  UINT8   Reserved7[0x38];   // 7ea0h
  UINT64  EptVmxControl;     // 7ed8h
  UINT32  EnEptVmxControl;   // 7ee0h
  UINT8   Reserved8[0x14];   // 7ee4h
  UINT32  SMBASE;            // 7ef8h
  UINT32  SMMRevId;          // 7efch
  UINT16  IORestart;         // 7f00h
  UINT16  AutoHALTRestart;   // 7f02h
  UINT8   Reserved9[0x18];   // 7f04h
  UINT64  _R15;              // 7f1ch
  UINT64  _R14;
  UINT64  _R13;
  UINT64  _R12;
  UINT64  _R11;
  UINT64  _R10;
  UINT64  _R9;
  UINT64  _R8;
  UINT64  _RAX;              // 7f5ch
  UINT64  _RCX;
  UINT64  _RDX;
  UINT64  _RBX;
  UINT64  _RSP;
  UINT64  _RBP;
  UINT64  _RSI;
  UINT64  _RDI;
  UINT64  IOMemAddr;         // 7f9ch
  UINT32  IOMisc;            // 7fa4h
  UINT32  _ES;               // 7fa8h
  UINT32  _CS;
  UINT32  _SS;
  UINT32  _DS;
  UINT32  _FS;
  UINT32  _GS;
  UINT32  _LDTR;             // 7fc0h
  UINT32  _TR;
  UINT64  _DR7;              // 7fc8h
  UINT64  _DR6;
  UINT64  _RIP;              // 7fd8h
  UINT64  IA32_EFER;         // 7fe0h
  UINT64  _RFLAGS;           // 7fe8h
  UINT64  _CR3;              // 7ff0h
  UINT64  _CR0;              // 7ff8h
} SMRAM_SAVE_STATE_MAP64;

///
/// Union of 32-bit and 64-bit SMRAM Save State Maps
///
typedef union  {
  SMRAM_SAVE_STATE_MAP32  x86;
  SMRAM_SAVE_STATE_MAP64  x64;
} SMRAM_SAVE_STATE_MAP;

///
/// Minimum SMM Revision ID that supports IOMisc field in SMRAM Save State Map
///
#define SMRAM_SAVE_STATE_MIN_REV_ID_IOMISC  0x30004

///
/// SMRAM Save State Map IOMisc I/O Length Values
///
#define  SMM_IO_LENGTH_BYTE             0x01
#define  SMM_IO_LENGTH_WORD             0x02
#define  SMM_IO_LENGTH_DWORD            0x04

///
/// SMRAM Save State Map IOMisc I/O Instruction Type Values
///
#define  SMM_IO_TYPE_IN_IMMEDIATE       0x9
#define  SMM_IO_TYPE_IN_DX              0x1
#define  SMM_IO_TYPE_OUT_IMMEDIATE      0x8
#define  SMM_IO_TYPE_OUT_DX             0x0
#define  SMM_IO_TYPE_INS                0x3
#define  SMM_IO_TYPE_OUTS               0x2
#define  SMM_IO_TYPE_REP_INS            0x7
#define  SMM_IO_TYPE_REP_OUTS           0x6

///
/// SMRAM Save State Map IOMisc structure
///
typedef union {
  struct {
    UINT32  SmiFlag:1;
    UINT32  Length:3;
    UINT32  Type:4;
    UINT32  Reserved1:8;
    UINT32  Port:16;
  } Bits;
  UINT32  Uint32;
} SMRAM_SAVE_STATE_IOMISC;

#pragma pack ()

#endif
