/** @file
  Processor or compiler specific defines and types for EBC.

  We currently only have one EBC compiler so there may be some Intel compiler
  specific functions in this file.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PROCESSOR_BIND_H__
#define __PROCESSOR_BIND_H__

///
/// Define the processor type so other code can make processor based choices
///
#define MDE_CPU_EBC

//
// Native integer types
//

///
/// 1-byte signed value
///
typedef signed char INT8;
///
/// Logical Boolean.  1-byte value containing 0 for FALSE or a 1 for TRUE.  Other
/// values are undefined.
///
typedef unsigned char BOOLEAN;
///
/// 1-byte unsigned value.
///
typedef unsigned char UINT8;
///
/// 1-byte Character.
///
typedef char CHAR8;
///
/// 2-byte signed value.
///
typedef short INT16;
///
/// 2-byte unsigned value.
///
typedef unsigned short UINT16;
///
/// 2-byte Character.  Unless otherwise specified all strings are stored in the
/// UTF-16 encoding format as defined by Unicode 2.1 and ISO/IEC 10646 standards.
///
typedef unsigned short CHAR16;
///
/// 4-byte signed value.
///
typedef int INT32;
///
/// 4-byte unsigned value.
///
typedef unsigned int UINT32;
///
/// 8-byte signed value.
///
typedef __int64 INT64;
///
/// 8-byte unsigned value.
///
typedef unsigned __int64 UINT64;

///
/// Signed value of native width.  (4 bytes on supported 32-bit processor instructions,
/// 8 bytes on supported 64-bit processor instructions)
/// "long" type scales to the processor native size with EBC compiler
///
typedef long INTN;
///
/// The unsigned value of native width.  (4 bytes on supported 32-bit processor instructions;
/// 8 bytes on supported 64-bit processor instructions)
/// "long" type scales to the processor native size with the EBC compiler.
///
typedef unsigned long UINTN;

///
/// A value of native width with the highest bit set.
/// Scalable macro to set the most significant bit in a natural number.
///
#define MAX_BIT  ((UINTN)((1ULL << (sizeof (INTN) * 8 - 1))))
///
/// A value of native width with the two highest bits set.
/// Scalable macro to set the most 2 significant bits in a natural number.
///
#define MAX_2_BITS  ((UINTN)(3ULL << (sizeof (INTN) * 8 - 2)))

///
/// Maximum legal EBC address
///
#define MAX_ADDRESS  ((UINTN)(~0ULL >> (64 - sizeof (INTN) * 8)))

///
/// Maximum usable address at boot time (48 bits using 4 KB pages)
///
#define MAX_ALLOC_ADDRESS  MAX_ADDRESS

///
/// Maximum legal EBC INTN and UINTN values.
///
#define MAX_UINTN  ((UINTN)(~0ULL >> (64 - sizeof (INTN) * 8)))
#define MAX_INTN   ((INTN)(~0ULL >> (65 - sizeof (INTN) * 8)))

///
/// Minimum legal EBC INTN value.
///
#define MIN_INTN  (((INTN)-MAX_INTN) - 1)

///
/// The stack alignment required for EBC
///
#define CPU_STACK_ALIGNMENT  sizeof(UINTN)

///
/// Page allocation granularity for EBC
///
#define DEFAULT_PAGE_ALLOCATION_GRANULARITY  (0x1000)
#define RUNTIME_PAGE_ALLOCATION_GRANULARITY  (0x1000)

///
/// Modifier to ensure that all protocol member functions and EFI intrinsics
/// use the correct C calling convention. All protocol member functions and
/// EFI intrinsics are required to modify their member functions with EFIAPI.
///
#ifdef EFIAPI
///
/// If EFIAPI is already defined, then we use that definition.
///
#else
#define EFIAPI
#endif

/**
  Return the pointer to the first instruction of a function given a function pointer.
  On EBC architectures, these two pointer values are the same,
  so the implementation of this macro is very simple.

  @param  FunctionPointer   A pointer to a function.

  @return The pointer to the first instruction of a function given a function pointer.
**/
#define FUNCTION_ENTRY_POINT(FunctionPointer)  (VOID *)(UINTN)(FunctionPointer)

#ifndef __USER_LABEL_PREFIX__
#define __USER_LABEL_PREFIX__
#endif

#endif
