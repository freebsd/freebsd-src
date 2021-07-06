/*
 * Memory tagging testing code.
 *
 * Copyright (c) 2020, Arm Limited.
 * SPDX-License-Identifier: MIT
 */

#ifndef __TEST_MTE_H
#define __TEST_MTE_H

#include <stdlib.h>

#if __ARM_FEATURE_MEMORY_TAGGING && WANT_MTE_TEST
#include <arm_acle.h>
#include <sys/mman.h>
#include <sys/prctl.h>

// These depend on a not yet merged kernel ABI.
#define PR_SET_TAGGED_ADDR_CTRL 55
#define PR_TAGGED_ADDR_ENABLE (1UL << 0)
#define PR_MTE_TCF_SHIFT 1
#define PR_MTE_TCF_SYNC (1UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TAG_SHIFT 3
#define PROT_MTE 0x20

#define MTE_GRANULE_SIZE 16

int
mte_enabled ()
{
  static int enabled = -1;
  if (enabled == -1)
    {
      int res = prctl (PR_SET_TAGGED_ADDR_CTRL,
		       PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC
			 | (0xfffe << PR_MTE_TAG_SHIFT),
		       0, 0, 0);
      enabled = (res == 0);
    }
  return enabled;
}

static void *
mte_mmap (size_t size)
{
  if (mte_enabled ())
    {
      return mmap (NULL, size, PROT_READ | PROT_WRITE | PROT_MTE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }
  else
    {
      return malloc (size);
    }
}

void *
alignup_mte (void *p)
{
  return (void *) (((uintptr_t) p + MTE_GRANULE_SIZE - 1)
		   & ~(MTE_GRANULE_SIZE - 1));
}

void *
aligndown_mte (void *p)
{
  return (void *) ((uintptr_t) p & ~(MTE_GRANULE_SIZE - 1));
}

void *
untag_pointer (void *p)
{
  return (void *) ((unsigned long long) p & (~0ULL >> 8));
}

void
tag_buffer_helper (void *p, int len)
{
  char *ptr = p;
  char *end = alignup_mte (ptr + len);
  ptr = aligndown_mte (p);
  for (; ptr < end; ptr += MTE_GRANULE_SIZE)
    {
      __arm_mte_set_tag (ptr);
    }
}

void *
tag_buffer (void *p, int len, int test_mte)
{
  if (test_mte && mte_enabled ())
    {
      p = __arm_mte_increment_tag (p, 1);
      tag_buffer_helper (p, len);
    }
  return p;
}

void *
untag_buffer (void *p, int len, int test_mte)
{
  p = untag_pointer (p);
  if (test_mte && mte_enabled ())
    {
      tag_buffer_helper (p, len);
    }
  return p;
}

#else  // __ARM_FEATURE_MEMORY_TAGGING
int
mte_enabled ()
{
  return 0;
}
static void *
mte_mmap (size_t size)
{
  return malloc (size);
}
void *
tag_buffer (void *p, int len, int test_mte)
{
  (void) len;
  (void) test_mte;
  return p;
}
void *
untag_buffer (void *p, int len, int test_mte)
{
  (void) len;
  (void) test_mte;
  return p;
}
void *
untag_pointer (void *p)
{
  return p;
}
#endif // __ARM_FEATURE_MEMORY_TAGGING

#endif
