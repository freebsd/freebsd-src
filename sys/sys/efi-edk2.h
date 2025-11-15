/*
 * Copyright (c) 2017-2025 Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_SYS_EFI_EDK2_H_
#define	_SYS_EFI_EDK2_H_

/*
 * Defines to adjust the types that EDK2 uses for FreeBSD so we can
 * use the code and headers mostly unchanged. The headers are imported
 * all into one directory to avoid case issues with filenames and
 * included. The actual code is heavily modified since it has too many
 * annoying dependencies that are difficult to satisfy.
 */

#include <stdlib.h>
#include <stdint.h>

typedef int8_t INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int64_t INT64;
typedef intptr_t INTN;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef uintptr_t UINTN;
//typedef uintptr_t EFI_PHYSICAL_ADDRESS;
//typedef uint32_t EFI_IPv4_ADDRESS;
//typedef uint8_t EFI_MAC_ADDRESS[6];
//typedef uint8_t EFI_IPv6_ADDRESS[16];
typedef uint8_t CHAR8;
typedef uint16_t CHAR16;
typedef UINT8 BOOLEAN;
typedef void VOID;
//typedef uuid_t GUID;
//typedef uuid_t EFI_GUID;

/* We can't actually call this stuff, so snip out API syntactic sugar */
#define INTERFACE_DECL(x) struct x
#ifdef _STANDALONE
#if defined(__amd64__)
#define EFIAPI    __attribute__((ms_abi))
#endif
#ifndef EFIAPI                  // Forces EFI calling conventions reguardless of compiler options
    #ifdef _MSC_EXTENSIONS
        #define EFIAPI __cdecl  // Force C calling convention for Microsoft C compiler
    #else
        #define EFIAPI          // Substitute expresion to force C calling convention
    #endif
#endif
#else
#define EFIAPI
#endif
#define IN
#define OUT
#define CONST const
#define OPTIONAL
//#define TRUE 1
//#define FALSE 0

/*
 * EDK2 has fine definitions for these, so let it define them.
 */
#undef NULL
#undef EFI_PAGE_SIZE
#undef EFI_PAGE_MASK

/*
 * Note: the EDK2 code assumed #pragma packed works and PACKED is a
 * workaround for some old toolchain issues for EDK2 that aren't
 * relevant to FreeBSD.
 */
#define PACKED

/*
 * For userland and the kernel, we're not compiling for the UEFI boot time
 * (which use ms abi conventions on amd64), tell EDK2 to define VA_START
 * correctly. For the boot loader, we can't do that, so don't.
 */
#ifndef _STANDALONE
#define NO_MSABI_VA_FUNCS 1
#endif

/*
 * Finally, we need to define the processor we are in EDK2 terms.
 */
#if defined(__i386__)
#define MDE_CPU_IA32
#elif defined(__amd64__)
#define MDE_CPU_X64
#elif defined(__arm__)
#define MDE_CPU_ARM
#elif defined(__aarch64__)
#define MDE_CPU_AARCH64
#elif defined(__riscv)
#define MDE_CPU_RISCV64
#endif
/* FreeBSD doesn't have/use MDE_CPU_EBC or MDE_CPU_IPF (ia64) */

#if __SIZEOF_LONG__ == 4
#define MAX_BIT      0x80000000
#else
#define MAX_BIT      0x8000000000000000
#endif

/*
 * Sometimes EFI is included after sys/param.h, and that causes a collision. We
 * get a collision the other way too, so when including both, you have to
 * include sys/param.h first.
 */
#undef MAX
#undef MIN

#endif /* _SYS_EFI_EDK2_H_ */
