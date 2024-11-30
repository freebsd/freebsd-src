// ------------------------------------------------------------------------------
//
// Copyright (c) 2017, Pete Batard. All rights reserved.<BR>
// Copyright (c) 2021, Arm Limited. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
// ------------------------------------------------------------------------------

#if defined (_M_ARM64)
typedef unsigned __int64 size_t;
#else
typedef unsigned __int32 size_t;
#endif

void *
memset (
  void *,
  int,
  size_t
  );

#pragma intrinsic(memset)
#pragma function(memset)
void *
memset (
  void    *s,
  int     c,
  size_t  n
  )
{
  unsigned char  *d;

  d = s;

  while (n-- != 0) {
    *d++ = (unsigned char)c;
  }

  return s;
}
