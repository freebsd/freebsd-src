/** @file
  ISO C implementations of strchr, strrchr and strtoul.

  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2023 Pedro Falcato All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>

#define ULONG_MAX  0xFFFFFFFF            /* Maximum unsigned long value */

// Very quick notes:
// We only go through the string once for both functions
// They are minimal implementations (not speed optimized) of ISO C semantics
// strchr and strrchr also include the null terminator as part of the string
// so the code gets a bit clunky to handle that case specifically.

char *
fdt_strrchr (
  const char  *Str,
  int         Char
  )
{
  char  *S, *last;

  S    = (char *)Str;
  last = NULL;

  for ( ; ; S++) {
    if (*S == Char) {
      last = S;
    }

    if (*S == '\0') {
      return last;
    }
  }
}

STATIC
int
__isspace (
  int  ch
  )
{
  // basic ASCII ctype.h:isspace(). Not efficient
  return ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f';
}

unsigned long
fdt_strtoul (
  const char  *Nptr,
  char        **EndPtr,
  int         Base
  )
{
  BOOLEAN        Negate;
  BOOLEAN        Overflow;
  unsigned long  Val;

  Negate   = FALSE;
  Overflow = FALSE;
  Val      = 0;

  // Reject bad numeric bases
  if ((Base < 0) || (Base == 1) || (Base > 36)) {
    return 0;
  }

  // Skip whitespace
  while (__isspace (*Nptr)) {
    Nptr++;
  }

  // Check for + or - prefixes
  if (*Nptr == '-') {
    Negate = TRUE;
    Nptr++;
  } else if (*Nptr == '+') {
    Nptr++;
  }

  // Consume the start, autodetecting base if needed
  if ((Nptr[0] == '0') && ((Nptr[1] == 'x') || (Nptr[1] == 'X')) && ((Base == 0) || (Base == 16))) {
    // Hex
    Nptr += 2;
    Base  = 16;
  } else if ((Nptr[0] == '0') && ((Nptr[1] == 'b') || (Nptr[1] == 'B')) && ((Base == 0) || (Base == 2))) {
    // Binary (standard pending C23)
    Nptr += 2;
    Base  = 2;
  } else if ((Nptr[0] == '0') && ((Base == 0) || (Base == 8))) {
    // Octal
    Nptr++;
    Base = 8;
  } else {
    if (Base == 0) {
      // Assume decimal
      Base = 10;
    }
  }

  while (TRUE) {
    int            Digit;
    char           C;
    unsigned long  NewVal;

    C     = *Nptr;
    Digit = -1;

    if ((C >= '0') && (C <= '9')) {
      Digit = C - '0';
    } else if ((C >= 'a') && (C <= 'z')) {
      Digit = C - 'a' + 10;
    } else if ((C >= 'A') && (C <= 'Z')) {
      Digit = C - 'A' + 10;
    }

    if ((Digit == -1) || (Digit >= Base)) {
      // Note that this case also handles the \0
      if (EndPtr) {
        *EndPtr = (char *)Nptr;
      }

      break;
    }

    NewVal = Val * Base + Digit;

    if (NewVal < Val) {
      // Overflow
      Overflow = TRUE;
    }

    Val = NewVal;

    Nptr++;
  }

  if (Negate) {
    Val = -Val;
  }

  if (Overflow) {
    Val = ULONG_MAX;
  }

  // TODO: We're lacking errno here.
  return Val;
}
