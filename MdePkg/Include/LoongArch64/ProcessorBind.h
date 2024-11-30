/** @file
  Processor or Compiler specific defines and types for LoongArch

  Copyright (c) 2022, Loongson Technology Corporation Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PROCESSOR_BIND_H_
#define PROCESSOR_BIND_H_

//
// Define the processor type so other code can make processor based choices
//
#define MDE_CPU_LOONGARCH64

#define EFIAPI

//
// Make sure we are using the correct packing rules per EFI specification
//
#ifndef __GNUC__
  #pragma pack()
#endif

//
// Assume standard LoongArch 64-bit alignment.
// Need to check portability of long long
//
typedef unsigned long long  UINT64;
typedef long long           INT64;
typedef unsigned int        UINT32;
typedef int                 INT32;
typedef unsigned short      UINT16;
typedef unsigned short      CHAR16;
typedef short               INT16;
typedef unsigned char       BOOLEAN;
typedef unsigned char       UINT8;
typedef char                CHAR8;
typedef char                INT8;

//
// Unsigned value of native width.  (4 bytes on supported 32-bit processor instructions,
// 8 bytes on supported 64-bit processor instructions)
//

typedef UINT64 UINTN;

//
// Signed value of native width.  (4 bytes on supported 32-bit processor instructions,
// 8 bytes on supported 64-bit processor instructions)
//
typedef INT64 INTN;

//
// Processor specific defines
//

//
// A value of native width with the highest bit set.
//
#define MAX_BIT  0x8000000000000000ULL
//
// A value of native width with the two highest bits set.
//
#define MAX_2_BITS  0xC000000000000000ULL

//
// Maximum legal LoongArch 64-bit address
//
#define MAX_ADDRESS  0xFFFFFFFFFFFFFFFFULL

//
// Maximum usable address at boot time (48 bits using 4KB pages)
//
#define MAX_ALLOC_ADDRESS  0xFFFFFFFFFFFFULL

//
// Maximum legal LoongArch  64-bit INTN and UINTN values.
//
#define MAX_INTN   ((INTN)0x7FFFFFFFFFFFFFFFULL)
#define MAX_UINTN  ((UINTN)0xFFFFFFFFFFFFFFFFULL)

//
// Page allocation granularity for LoongArch
//
#define DEFAULT_PAGE_ALLOCATION_GRANULARITY  (0x1000)
#define RUNTIME_PAGE_ALLOCATION_GRANULARITY  (0x10000)

#if defined (__GNUC__)
//
// For GNU assembly code, .global or .globl can declare global symbols.
// Define this macro to unify the usage.
//
#define ASM_GLOBAL  .globl
#endif

//
// The stack alignment required for LoongArch
//
#define CPU_STACK_ALIGNMENT  16

/**
  Return the pointer to the first instruction of a function given a function pointer.
  On LOONGARCH CPU architectures, these two pointer values are the same,
  so the implementation of this macro is very simple.

  @param  FunctionPointer   A pointer to a function.

  @return The pointer to the first instruction of a function given a function pointer.

**/
#define FUNCTION_ENTRY_POINT(FunctionPointer)  (VOID *)(UINTN)(FunctionPointer)

#ifndef __USER_LABEL_PREFIX__
#define __USER_LABEL_PREFIX__
#endif

#endif
