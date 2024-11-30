// ------------------------------------------------------------------------------
//
// Copyright (c) 2019, Pete Batard. All rights reserved.
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

int
memcmp (
  void *,
  void *,
  size_t
  );

#pragma intrinsic(memcmp)
#pragma function(memcmp)
int
memcmp (
  const void  *s1,
  const void  *s2,
  size_t      n
  )
{
  unsigned char const  *t1;
  unsigned char const  *t2;

  t1 = s1;
  t2 = s2;

  while (n-- != 0) {
    if (*t1 != *t2) {
      return (int)*t1 - (int)*t2;
    }

    t1++;
    t2++;
  }

  return 0;
}
