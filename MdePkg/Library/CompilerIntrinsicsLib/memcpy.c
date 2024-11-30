// ------------------------------------------------------------------------------
//
// Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>
// Copyright (c) 2021, Arm Limited. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
// ------------------------------------------------------------------------------

typedef __SIZE_TYPE__ size_t;

static void
__memcpy (
  void        *dest,
  const void  *src,
  size_t      n
  )
{
  unsigned char        *d;
  unsigned char const  *s;

  d = dest;
  s = src;

  while (n-- != 0) {
    *d++ = *s++;
  }
}

void *
memcpy (
  void        *dest,
  const void  *src,
  size_t      n
  )
{
  __memcpy (dest, src, n);
  return dest;
}

#ifdef __arm__

__attribute__ ((__alias__ ("__memcpy")))
void
__aeabi_memcpy (
  void        *dest,
  const void  *src,
  size_t      n
  );

__attribute__ ((__alias__ ("__memcpy")))
void
__aeabi_memcpy4 (
  void        *dest,
  const void  *src,
  size_t      n
  );

__attribute__ ((__alias__ ("__memcpy")))
void
__aeabi_memcpy8 (
  void        *dest,
  const void  *src,
  size_t      n
  );

#endif
