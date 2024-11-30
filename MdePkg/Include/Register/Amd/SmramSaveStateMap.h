/** @file
  AMD SMRAM Save State Map Definitions.

  SMRAM Save State Map definitions based on contents of the
    AMD64 Architecture Programmer Manual:
    Volume 2, System Programming, Section 10.2 SMM Resources

  Copyright (c) 2015 - 2019, Intel Corporation. All rights reserved.<BR>
  Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved .<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef AMD_SMRAM_SAVE_STATE_MAP_H_
#define AMD_SMRAM_SAVE_STATE_MAP_H_

///
/// Default SMBASE address
///
#define SMM_DEFAULT_SMBASE  0x30000

///
/// Offset of SMM handler from SMBASE
///
#define SMM_HANDLER_OFFSET  0x8000

// SMM-Revision Identifier for AMD64 Architecture.
#define AMD_SMM_MIN_REV_ID_X64  0x30064

#pragma pack (1)

///
/// 32-bit SMRAM Save State Map
///
typedef struct {
  // Padded an extra 0x200 bytes to match Intel/EDK2
  UINT8     Reserved[0x200]; // fc00h
  // AMD Save State area starts @ 0xfe00
  UINT8     Reserved1[0xf8]; // fe00h
  UINT32    SMBASE;          // fef8h
  UINT32    SMMRevId;        // fefch
  UINT16    IORestart;       // ff00h
  UINT16    AutoHALTRestart; // ff02h
  UINT8     Reserved2[0x84]; // ff04h
  UINT32    GDTBase;         // ff88h
  UINT64    Reserved3;       // ff8ch
  UINT32    IDTBase;         // ff94h
  UINT8     Reserved4[0x10]; // ff98h
  UINT32    _ES;             // ffa8h
  UINT32    _CS;             // ffach
  UINT32    _SS;             // ffb0h
  UINT32    _DS;             // ffb4h
  UINT32    _FS;             // ffb8h
  UINT32    _GS;             // ffbch
  UINT32    LDTBase;         // ffc0h
  UINT32    _TR;             // ffc4h
  UINT32    _DR7;            // ffc8h
  UINT32    _DR6;            // ffcch
  UINT32    _EAX;            // ffd0h
  UINT32    _ECX;            // ffd4h
  UINT32    _EDX;            // ffd8h
  UINT32    _EBX;            // ffdch
  UINT32    _ESP;            // ffe0h
  UINT32    _EBP;            // ffe4h
  UINT32    _ESI;            // ffe8h
  UINT32    _EDI;            // ffech
  UINT32    _EIP;            // fff0h
  UINT32    _EFLAGS;         // fff4h
  UINT32    _CR3;            // fff8h
  UINT32    _CR0;            // fffch
} AMD_SMRAM_SAVE_STATE_MAP32;

///
/// 64-bit SMRAM Save State Map
///
typedef struct {
  // Padded an extra 0x200 bytes to match Intel/EDK2
  UINT8     Reserved[0x200]; // fc00h
  // AMD Save State area starts @ 0xfe00
  UINT16    _ES;              // fe00h
  UINT16    _ESAttributes;    // fe02h
  UINT32    _ESLimit;         // fe04h
  UINT64    _ESBase;          // fe08h

  UINT16    _CS;              // fe10h
  UINT16    _CSAttributes;    // fe12h
  UINT32    _CSLimit;         // fe14h
  UINT64    _CSBase;          // fe18h

  UINT16    _SS;              // fe20h
  UINT16    _SSAttributes;    // fe22h
  UINT32    _SSLimit;         // fe24h
  UINT64    _SSBase;          // fe28h

  UINT16    _DS;              // fe30h
  UINT16    _DSAttributes;    // fe32h
  UINT32    _DSLimit;         // fe34h
  UINT64    _DSBase;          // fe38h

  UINT16    _FS;              // fe40h
  UINT16    _FSAttributes;    // fe42h
  UINT32    _FSLimit;         // fe44h
  UINT64    _FSBase;          // fe48h

  UINT16    _GS;              // fe50h
  UINT16    _GSAttributes;    // fe52h
  UINT32    _GSLimit;         // fe54h
  UINT64    _GSBase;          // fe58h

  UINT32    _GDTRReserved1;   // fe60h
  UINT16    _GDTRLimit;       // fe64h
  UINT16    _GDTRReserved2;   // fe66h
  // UINT64  _GDTRBase;        // fe68h
  UINT32    _GDTRBaseLoDword;
  UINT32    _GDTRBaseHiDword;

  UINT16    _LDTR;            // fe70h
  UINT16    _LDTRAttributes;  // fe72h
  UINT32    _LDTRLimit;       // fe74h
  // UINT64  _LDTRBase;        // fe78h
  UINT32    _LDTRBaseLoDword;
  UINT32    _LDTRBaseHiDword;

  UINT32    _IDTRReserved1;   // fe80h
  UINT16    _IDTRLimit;       // fe84h
  UINT16    _IDTRReserved2;   // fe86h
  // UINT64  _IDTRBase;        // fe88h
  UINT32    _IDTRBaseLoDword;
  UINT32    _IDTRBaseHiDword;

  UINT16    _TR;              // fe90h
  UINT16    _TRAttributes;    // fe92h
  UINT32    _TRLimit;         // fe94h
  UINT64    _TRBase;          // fe98h

  UINT64    IO_RIP;           // fea0h
  UINT64    IO_RCX;           // fea8h
  UINT64    IO_RSI;           // feb0h
  UINT64    IO_RDI;           // feb8h
  UINT32    IO_DWord;         // fec0h
  UINT8     Reserved1[0x04];  // fec4h
  UINT8     IORestart;        // fec8h
  UINT8     AutoHALTRestart;  // fec9h
  UINT8     Reserved2[0x06];  // fecah
  UINT64    EFER;             // fed0h
  UINT64    SVM_Guest;        // fed8h
  UINT64    SVM_GuestVMCB;    // fee0h
  UINT64    SVM_GuestVIntr;   // fee8h
  UINT8     Reserved3[0x0c];  // fef0h
  UINT32    SMMRevId;         // fefch
  UINT32    SMBASE;           // ff00h
  UINT8     Reserved4[0x14];  // ff04h
  UINT64    SSP;              // ff18h
  UINT64    SVM_GuestPAT;     // ff20h
  UINT64    SVM_HostEFER;     // ff28h
  UINT64    SVM_HostCR4;      // ff30h
  UINT64    SVM_HostCR3;      // ff38h
  UINT64    SVM_HostCR0;      // ff40h
  UINT64    _CR4;             // ff48h
  UINT64    _CR3;             // ff50h
  UINT64    _CR0;             // ff58h
  UINT64    _DR7;             // ff60h
  UINT64    _DR6;             // ff68h
  UINT64    _RFLAGS;          // ff70h
  UINT64    _RIP;             // ff78h
  UINT64    _R15;             // ff80h
  UINT64    _R14;             // ff88h
  UINT64    _R13;             // ff90h
  UINT64    _R12;             // ff98h
  UINT64    _R11;             // ffa0h
  UINT64    _R10;             // ffa8h
  UINT64    _R9;              // ffb0h
  UINT64    _R8;              // ffb8h
  UINT64    _RDI;             // ffc0h
  UINT64    _RSI;             // ffc8h
  UINT64    _RBP;             // ffd0h
  UINT64    _RSP;             // ffd8h
  UINT64    _RBX;             // ffe0h
  UINT64    _RDX;             // ffe8h
  UINT64    _RCX;             // fff0h
  UINT64    _RAX;             // fff8h
} AMD_SMRAM_SAVE_STATE_MAP64;

///
/// Union of 32-bit and 64-bit SMRAM Save State Maps
///
typedef union  {
  AMD_SMRAM_SAVE_STATE_MAP32    x86;
  AMD_SMRAM_SAVE_STATE_MAP64    x64;
} AMD_SMRAM_SAVE_STATE_MAP;

#pragma pack ()

#endif
