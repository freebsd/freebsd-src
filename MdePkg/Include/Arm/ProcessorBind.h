/** @file
  Processor or Compiler specific defines and types for ARM.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PROCESSOR_BIND_H__
#define __PROCESSOR_BIND_H__

///
/// Define the processor type so other code can make processor based choices
///
#define MDE_CPU_ARM

//
// Make sure we are using the correct packing rules per EFI specification
//
#if !defined(__GNUC__) && !defined(__ASSEMBLER__)
#pragma pack()
#endif

#if defined(_MSC_EXTENSIONS)

//
// Disable some level 4 compilation warnings (same as IA32 and X64)
//

//
// Disabling bitfield type checking warnings.
//
#pragma warning ( disable : 4214 )

//
// Disabling the unreferenced formal parameter warnings.
//
#pragma warning ( disable : 4100 )

//
// Disable slightly different base types warning as CHAR8 * can not be set
// to a constant string.
//
#pragma warning ( disable : 4057 )

//
// ASSERT(FALSE) or while (TRUE) are legal constructs so suppress this warning
//
#pragma warning ( disable : 4127 )

//
// This warning is caused by functions defined but not used. For precompiled header only.
//
#pragma warning ( disable : 4505 )

//
// This warning is caused by empty (after preprocessing) source file. For precompiled header only.
//
#pragma warning ( disable : 4206 )

//
// Disable 'potentially uninitialized local variable X used' warnings
//
#pragma warning ( disable : 4701 )

//
// Disable 'potentially uninitialized local pointer variable X used' warnings
//
#pragma warning ( disable : 4703 )

#endif

//
// RVCT and MSFT don't support the __builtin_unreachable() macro
//
#if defined(__ARMCC_VERSION) || defined(_MSC_EXTENSIONS)
#define UNREACHABLE()
#endif

#if defined(_MSC_EXTENSIONS)
  //
  // use Microsoft* C compiler dependent integer width types
  //
  typedef unsigned __int64    UINT64;
  typedef __int64             INT64;
  typedef unsigned __int32    UINT32;
  typedef __int32             INT32;
  typedef unsigned short      UINT16;
  typedef unsigned short      CHAR16;
  typedef short               INT16;
  typedef unsigned char       BOOLEAN;
  typedef unsigned char       UINT8;
  typedef char                CHAR8;
  typedef signed char         INT8;
#else
  //
  // Assume standard ARM alignment.
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
  typedef signed char         INT8;
#endif

///
/// Unsigned value of native width.  (4 bytes on supported 32-bit processor instructions,
/// 8 bytes on supported 64-bit processor instructions)
///
typedef UINT32  UINTN;

///
/// Signed value of native width.  (4 bytes on supported 32-bit processor instructions,
/// 8 bytes on supported 64-bit processor instructions)
///
typedef INT32   INTN;

//
// Processor specific defines
//

///
/// A value of native width with the highest bit set.
///
#define MAX_BIT      0x80000000

///
/// A value of native width with the two highest bits set.
///
#define MAX_2_BITS   0xC0000000

///
/// Maximum legal ARM address
///
#define MAX_ADDRESS  0xFFFFFFFF

///
/// Maximum usable address at boot time
///
#define MAX_ALLOC_ADDRESS   MAX_ADDRESS

///
/// Maximum legal ARM INTN and UINTN values.
///
#define MAX_INTN   ((INTN)0x7FFFFFFF)
#define MAX_UINTN  ((UINTN)0xFFFFFFFF)

///
/// Minimum legal ARM INTN value.
///
#define MIN_INTN   (((INTN)-2147483647) - 1)

///
/// The stack alignment required for ARM
///
#define CPU_STACK_ALIGNMENT  sizeof(UINT64)

///
/// Page allocation granularity for ARM
///
#define DEFAULT_PAGE_ALLOCATION_GRANULARITY   (0x1000)
#define RUNTIME_PAGE_ALLOCATION_GRANULARITY   (0x1000)

//
// Modifier to ensure that all protocol member functions and EFI intrinsics
// use the correct C calling convention. All protocol member functions and
// EFI intrinsics are required to modify their member functions with EFIAPI.
//
#define EFIAPI

// When compiling with Clang, we still use GNU as for the assembler, so we still
// need to define the GCC_ASM* macros.
#if defined(__GNUC__) || defined(__clang__)
  ///
  /// For GNU assembly code, .global or .globl can declare global symbols.
  /// Define this macro to unify the usage.
  ///
  #define ASM_GLOBAL .globl

  #if !defined(__APPLE__)
    ///
    /// ARM EABI defines that the linker should not manipulate call relocations
    /// (do bl/blx conversion) unless the target symbol has function type.
    /// CodeSourcery 2010.09 started requiring the .type to function properly
    ///
    #define INTERWORK_FUNC(func__)   .type ASM_PFX(func__), %function

    #define GCC_ASM_EXPORT(func__)  \
             .global  _CONCATENATE (__USER_LABEL_PREFIX__, func__)    ;\
             .type ASM_PFX(func__), %function

    #define GCC_ASM_IMPORT(func__)  \
             .extern  _CONCATENATE (__USER_LABEL_PREFIX__, func__)

  #else
    //
    // .type not supported by Apple Xcode tools
    //
    #define INTERWORK_FUNC(func__)

    #define GCC_ASM_EXPORT(func__)  \
             .globl  _CONCATENATE (__USER_LABEL_PREFIX__, func__)    \

    #define GCC_ASM_IMPORT(name)

  #endif
#elif defined(_MSC_EXTENSIONS)
  //
  // PRESERVE8 is not supported by the MSFT assembler.
  //
  #define PRESERVE8
#endif

/**
  Return the pointer to the first instruction of a function given a function pointer.
  On ARM CPU architectures, these two pointer values are the same,
  so the implementation of this macro is very simple.

  @param  FunctionPointer   A pointer to a function.

  @return The pointer to the first instruction of a function given a function pointer.

**/
#define FUNCTION_ENTRY_POINT(FunctionPointer) (VOID *)(UINTN)(FunctionPointer)

#ifndef __USER_LABEL_PREFIX__
#define __USER_LABEL_PREFIX__
#endif

#endif


