/** @file
  Processor or Compiler specific defines and types for RISC-V

  Copyright (c) 2016 - 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PROCESSOR_BIND_H__
#define PROCESSOR_BIND_H__

///
/// Define the processor type so other code can make processor based choices
///
#define MDE_CPU_RISCV64

//
// Make sure we are using the correct packing rules per EFI specification
//
#if !defined(__GNUC__)
#pragma pack()
#endif

///
/// 8-byte unsigned value
///
typedef unsigned long long  UINT64  __attribute__ ((aligned (8)));
///
/// 8-byte signed value
///
typedef long long           INT64  __attribute__ ((aligned (8)));
///
/// 4-byte unsigned value
///
typedef unsigned int        UINT32 __attribute__ ((aligned (4)));
///
/// 4-byte signed value
///
typedef int                 INT32  __attribute__ ((aligned (4)));
///
/// 2-byte unsigned value
///
typedef unsigned short      UINT16  __attribute__ ((aligned (2)));
///
/// 2-byte Character.  Unless otherwise specified all strings are stored in the
/// UTF-16 encoding format as defined by Unicode 2.1 and ISO/IEC 10646 standards.
///
typedef unsigned short      CHAR16  __attribute__ ((aligned (2)));
///
/// 2-byte signed value
///
typedef short               INT16  __attribute__ ((aligned (2)));
///
/// Logical Boolean.  1-byte value containing 0 for FALSE or a 1 for TRUE.  Other
/// values are undefined.
///
typedef unsigned char       BOOLEAN;
///
/// 1-byte unsigned value
///
typedef unsigned char       UINT8;
///
/// 1-byte Character
///
typedef char                CHAR8;
///
/// 1-byte signed value
///
typedef signed char         INT8;
///
/// Unsigned value of native width.  (4 bytes on supported 32-bit processor instructions,
/// 8 bytes on supported 64-bit processor instructions)
///
typedef UINT64  UINTN __attribute__ ((aligned (8)));
///
/// Signed value of native width.  (4 bytes on supported 32-bit processor instructions,
/// 8 bytes on supported 64-bit processor instructions)
///
typedef INT64   INTN __attribute__ ((aligned (8)));

//
// Processor specific defines
//

///
/// A value of native width with the highest bit set.
///
#define MAX_BIT     0x8000000000000000ULL
///
/// A value of native width with the two highest bits set.
///
#define MAX_2_BITS  0xC000000000000000ULL

///
/// Maximum legal RV64 address
///
#define MAX_ADDRESS   0xFFFFFFFFFFFFFFFFULL

///
/// Maximum usable address at boot time (48 bits using 4 KB pages in Supervisor mode)
///
#define MAX_ALLOC_ADDRESS   0xFFFFFFFFFFFFULL

///
/// Maximum legal RISC-V INTN and UINTN values.
///
#define MAX_INTN   ((INTN)0x7FFFFFFFFFFFFFFFULL)
#define MAX_UINTN  ((UINTN)0xFFFFFFFFFFFFFFFFULL)

///
/// The stack alignment required for RISC-V
///
#define CPU_STACK_ALIGNMENT   16

///
/// Page allocation granularity for RISC-V
///
#define DEFAULT_PAGE_ALLOCATION_GRANULARITY   (0x1000)
#define RUNTIME_PAGE_ALLOCATION_GRANULARITY   (0x1000)

//
// Modifier to ensure that all protocol member functions and EFI intrinsics
// use the correct C calling convention. All protocol member functions and
// EFI intrinsics are required to modify their member functions with EFIAPI.
//
#ifdef EFIAPI
  ///
  /// If EFIAPI is already defined, then we use that definition.
  ///
#elif defined(__GNUC__)
  ///
  /// Define the standard calling convention regardless of optimization level
  /// The GCC support assumes a GCC compiler that supports the EFI ABI. The EFI
  /// ABI is much closer to the x64 Microsoft* ABI than standard x64 (x86-64)
  /// GCC ABI. Thus a standard x64 (x86-64) GCC compiler can not be used for
  /// x64. Warning the assembly code in the MDE x64 does not follow the correct
  /// ABI for the standard x64 (x86-64) GCC.
  ///
  #define EFIAPI
#else
  ///
  /// The default for a non Microsoft* or GCC compiler is to assume the EFI ABI
  /// is the standard.
  ///
  #define EFIAPI
#endif

#if defined(__GNUC__)
  ///
  /// For GNU assembly code, .global or .globl can declare global symbols.
  /// Define this macro to unify the usage.
  ///
  #define ASM_GLOBAL .globl
#endif

/**
  Return the pointer to the first instruction of a function given a function pointer.
  On x64 CPU architectures, these two pointer values are the same,
  so the implementation of this macro is very simple.

  @param  FunctionPointer   A pointer to a function.

  @return The pointer to the first instruction of a function given a function pointer.

**/
#define FUNCTION_ENTRY_POINT(FunctionPointer) (VOID *)(UINTN)(FunctionPointer)

#ifndef __USER_LABEL_PREFIX__
#define __USER_LABEL_PREFIX__
#endif

#endif
