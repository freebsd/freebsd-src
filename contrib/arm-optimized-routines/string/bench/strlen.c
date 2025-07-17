/*
 * strlen benchmark.
 *
 * Copyright (c) 2020-2021, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "stringlib.h"
#include "benchlib.h"

#define ITERS 5000
#define ITERS2 40000000
#define ITERS3 4000000
#define NUM_TESTS 65536

#define MAX_ALIGN 32
#define MAX_STRLEN 128

static char a[(MAX_STRLEN + 1) * MAX_ALIGN] __attribute__((__aligned__(4096)));

#define DOTEST(STR,TESTFN)			\
  printf (STR);					\
  RUN (TESTFN, strlen);				\
  RUNA64 (TESTFN, __strlen_aarch64);		\
  RUNA64 (TESTFN, __strlen_aarch64_mte);	\
  RUNSVE (TESTFN, __strlen_aarch64_sve);	\
  RUNT32 (TESTFN, __strlen_armv6t2);		\
  printf ("\n");

static uint16_t strlen_tests[NUM_TESTS];

typedef struct { uint16_t size; uint16_t freq; } freq_data_t;
typedef struct { uint8_t align; uint16_t freq; } align_data_t;

#define SIZE_NUM 65536
#define SIZE_MASK (SIZE_NUM - 1)
static uint8_t strlen_len_arr[SIZE_NUM];

/* Frequency data for strlen sizes up to 128 based on SPEC2017.  */
static freq_data_t strlen_len_freq[] =
{
  { 12,22671}, { 18,12834}, { 13, 9555}, {  6, 6348}, { 17, 6095}, { 11, 2115},
  { 10, 1335}, {  7,  814}, {  2,  646}, {  9,  483}, {  8,  471}, { 16,  418},
  {  4,  390}, {  1,  388}, {  5,  233}, {  3,  204}, {  0,   79}, { 14,   79},
  { 15,   69}, { 26,   36}, { 22,   35}, { 31,   24}, { 32,   24}, { 19,   21},
  { 25,   17}, { 28,   15}, { 21,   14}, { 33,   14}, { 20,   13}, { 24,    9},
  { 29,    9}, { 30,    9}, { 23,    7}, { 34,    7}, { 27,    6}, { 44,    5},
  { 42,    4}, { 45,    3}, { 47,    3}, { 40,    2}, { 41,    2}, { 43,    2},
  { 58,    2}, { 78,    2}, { 36,    2}, { 48,    1}, { 52,    1}, { 60,    1},
  { 64,    1}, { 56,    1}, { 76,    1}, { 68,    1}, { 80,    1}, { 84,    1},
  { 72,    1}, { 86,    1}, { 35,    1}, { 39,    1}, { 50,    1}, { 38,    1},
  { 37,    1}, { 46,    1}, { 98,    1}, {102,    1}, {128,    1}, { 51,    1},
  {107,    1}, { 0,     0}
};

#define ALIGN_NUM 1024
#define ALIGN_MASK (ALIGN_NUM - 1)
static uint8_t strlen_align_arr[ALIGN_NUM];

/* Alignment data for strlen based on SPEC2017.  */
static align_data_t string_align_freq[] =
{
  {8, 470}, {32, 427}, {16, 99}, {1, 19}, {2, 6}, {4, 3}, {0, 0}
};

static void
init_strlen_distribution (void)
{
  int i, j, freq, size, n;

  for (n = i = 0; (freq = strlen_len_freq[i].freq) != 0; i++)
    for (j = 0, size = strlen_len_freq[i].size; j < freq; j++)
      strlen_len_arr[n++] = size;
  assert (n == SIZE_NUM);

  for (n = i = 0; (freq = string_align_freq[i].freq) != 0; i++)
    for (j = 0, size = string_align_freq[i].align; j < freq; j++)
      strlen_align_arr[n++] = size;
  assert (n == ALIGN_NUM);
}

static void
init_strlen_tests (void)
{
  uint16_t index[MAX_ALIGN];

  memset (a, 'x', sizeof (a));

  /* Create indices for strings at all alignments.  */
  for (int i = 0; i < MAX_ALIGN; i++)
    {
      index[i] = i * (MAX_STRLEN + 1);
      a[index[i] + MAX_STRLEN] = 0;
    }

  /* Create a random set of strlen input strings using the string length
     and alignment distributions.  */
  for (int n = 0; n < NUM_TESTS; n++)
    {
      int align = strlen_align_arr[rand32 (0) & ALIGN_MASK];
      int exp_len = strlen_len_arr[rand32 (0) & SIZE_MASK];

      strlen_tests[n] =
	index[(align + exp_len) & (MAX_ALIGN - 1)] + MAX_STRLEN - exp_len;
      assert ((strlen_tests[n] & (align - 1)) == 0);
      assert (strlen (a + strlen_tests[n]) == exp_len);
    }
}

static volatile size_t maskv = 0;

static void inline __attribute ((always_inline))
strlen_random (const char *name, size_t (*fn)(const char *))
{
  size_t res = 0, mask = maskv;
  uint64_t strlen_size = 0;
  printf ("%22s ", name);

  for (int c = 0; c < NUM_TESTS; c++)
    strlen_size += fn (a + strlen_tests[c]) + 1;
  strlen_size *= ITERS;

  /* Measure throughput of strlen.  */
  uint64_t t = clock_get_ns ();
  for (int i = 0; i < ITERS; i++)
    for (int c = 0; c < NUM_TESTS; c++)
      res += fn (a + strlen_tests[c]);
  t = clock_get_ns () - t;
  printf ("tp: %.3f ", (double)strlen_size / t);

  /* Measure latency of strlen result with (res & mask).  */
  t = clock_get_ns ();
  for (int i = 0; i < ITERS; i++)
    for (int c = 0; c < NUM_TESTS; c++)
      res += fn (a + strlen_tests[c] + (res & mask));
  t = clock_get_ns () - t;
  printf ("lat: %.3f\n", (double)strlen_size / t);
  maskv = res & mask;
}

static void inline __attribute ((always_inline))
strlen_small_aligned (const char *name, size_t (*fn)(const char *))
{
  printf ("%22s ", name);

  size_t res = 0, mask = maskv;
  for (int size = 1; size <= 64; size *= 2)
    {
      memset (a, 'x', size);
      a[size - 1] = 0;

      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS2; i++)
	res += fn (a + (i & mask));
      t = clock_get_ns () - t;
      printf ("%d%c: %5.2f ", size < 1024 ? size : size / 1024,
	      size < 1024 ? 'B' : 'K', (double)size * ITERS2 / t);
    }
  maskv &= res;
  printf ("\n");
}

static void inline __attribute ((always_inline))
strlen_small_unaligned (const char *name, size_t (*fn)(const char *))
{
  printf ("%22s ", name);

  size_t res = 0, mask = maskv;
  int align = 9;
  for (int size = 1; size <= 64; size *= 2)
    {
      memset (a + align, 'x', size);
      a[align + size - 1] = 0;

      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS2; i++)
	res += fn (a + align + (i & mask));
      t = clock_get_ns () - t;
      printf ("%d%c: %5.2f ", size < 1024 ? size : size / 1024,
	      size < 1024 ? 'B' : 'K', (double)size * ITERS2 / t);
    }
  maskv &= res;
  printf ("\n");
}

static void inline __attribute ((always_inline))
strlen_medium (const char *name, size_t (*fn)(const char *))
{
  printf ("%22s ", name);

  size_t res = 0, mask = maskv;
  for (int size = 128; size <= 4096; size *= 2)
    {
      memset (a, 'x', size);
      a[size - 1] = 0;

      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS3; i++)
	res += fn (a + (i & mask));
      t = clock_get_ns () - t;
      printf ("%d%c: %5.2f ", size < 1024 ? size : size / 1024,
	      size < 1024 ? 'B' : 'K', (double)size * ITERS3 / t);
    }
  maskv &= res;
  printf ("\n");
}

int main (void)
{
  rand32 (0x12345678);
  init_strlen_distribution ();
  init_strlen_tests ();

  DOTEST ("Random strlen (bytes/ns):\n", strlen_random);
  DOTEST ("Small aligned strlen (bytes/ns):\n", strlen_small_aligned);
  DOTEST ("Small unaligned strlen (bytes/ns):\n", strlen_small_unaligned);
  DOTEST ("Medium strlen (bytes/ns):\n", strlen_medium);

  return 0;
}
