/** @file
  Root include file of C runtime library to support building the third-party
  libfdt library.

  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef FDT_LIB_SUPPORT_H_
#define FDT_LIB_SUPPORT_H_

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>

typedef UINT8   uint8_t;
typedef UINT16  uint16_t;
typedef INT32   int32_t;
typedef UINT32  uint32_t;
typedef UINT64  uint64_t;
typedef UINTN   uintptr_t;
typedef UINTN   size_t;

#if defined __STDC_VERSION__ && __STDC_VERSION__ > 201710L
/* bool, true and false are keywords.  */
#else
typedef BOOLEAN bool;
#define true   (1 == 1)
#define false  (1 == 0)
#endif

//
// Definitions for global constants used by libfdt library routines
//
#define INT_MAX     0x7FFFFFFF           /* Maximum (signed) int value */
#define INT32_MAX   0x7FFFFFFF           /* Maximum (signed) int32 value */
#define UINT32_MAX  0xFFFFFFFF           /* Maximum unsigned int32 value */

//
// Function prototypes of libfdt Library routines
//
void *
memset     (
  void *,
  int,
  size_t
  );

int
memcmp      (
  const void *,
  const void *,
  size_t
  );

int
strcmp      (
  const char *,
  const char *
  );

char *
strchr     (
  const char *,
  int
  );

char *
fdt_strrchr    (
  const char *,
  int
  );

unsigned long
fdt_strtoul     (
  const char *,
  char **,
  int
  );

char *
strcpy (
  char        *strDest,
  const char  *strSource
  );

//
// Macros that directly map functions to BaseLib, BaseMemoryLib, and DebugLib functions
//
#define memcpy(dest, source, count)         CopyMem(dest,source, (UINTN)(count))
#define memset(dest, ch, count)             SetMem(dest, (UINTN)(count),(UINT8)(ch))
#define memchr(buf, ch, count)              ScanMem8(buf, (UINTN)(count),(UINT8)ch)
#define memcmp(buf1, buf2, count)           (int)(CompareMem(buf1, buf2, (UINTN)(count)))
#define memmove(dest, source, count)        CopyMem(dest, source, (UINTN)(count))
#define strlen(str)                         (size_t)(AsciiStrLen(str))
#define strnlen(str, count)                 (size_t)(AsciiStrnLenS(str, count))
#define strncpy(strDest, strSource, count)  AsciiStrnCpyS(strDest, MAX_STRING_SIZE, strSource, (UINTN)count)
#define strcat(strDest, strSource)          AsciiStrCatS(strDest, MAX_STRING_SIZE, strSource)
#define strchr(str, ch)                     ScanMem8(str, AsciiStrSize (str), (UINT8)ch)
#define strcmp(string1, string2, count)     (int)(AsciiStrCmp(string1, string2))
#define strncmp(string1, string2, count)    (int)(AsciiStrnCmp(string1, string2, (UINTN)(count)))
#define strrchr(str, ch)                    fdt_strrchr(str, ch)
#define strtoul(ptr, end_ptr, base)         fdt_strtoul(ptr, end_ptr, base)

#endif /* FDT_LIB_SUPPORT_H_ */
