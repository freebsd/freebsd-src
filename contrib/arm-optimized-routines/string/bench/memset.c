/*
 * memset benchmark.
 *
 * Copyright (c) 2021, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "stringlib.h"
#include "benchlib.h"

#define ITERS  5000
#define ITERS2 20000000
#define ITERS3 1000000
#define NUM_TESTS 16384
#define MIN_SIZE 32768
#define MAX_SIZE (1024 * 1024)

static uint8_t a[MAX_SIZE + 4096] __attribute__((__aligned__(4096)));

#define DOTEST(STR,TESTFN)			\
  printf (STR);					\
  RUN (TESTFN, memset);				\
  RUNA64 (TESTFN, __memset_aarch64);		\
  RUNSVE (TESTFN, __memset_aarch64_sve);	\
  RUNMOPS (TESTFN, __memset_mops);		\
  RUNA32 (TESTFN, __memset_arm);		\
  printf ("\n");

typedef struct { uint32_t offset : 20, len : 12; } memset_test_t;
static memset_test_t test_arr[NUM_TESTS];

typedef struct { uint16_t size; uint16_t freq; } freq_data_t;
typedef struct { uint8_t align; uint16_t freq; } align_data_t;

#define SIZE_NUM 65536
#define SIZE_MASK (SIZE_NUM-1)
static uint8_t len_arr[SIZE_NUM];

/* Frequency data for memset sizes up to 4096 based on SPEC2017.  */
static freq_data_t memset_len_freq[] =
{
{40,28817}, {32,15336}, { 16,3823}, {296,3545}, { 24,3454}, {  8,1412},
{292,1202}, { 48, 927}, { 12, 613}, { 11, 539}, {284, 493}, {108, 414},
{ 88, 380}, { 20, 295}, {312, 271}, { 72, 233}, {  2, 200}, {  4, 192},
{ 15, 180}, { 14, 174}, { 13, 160}, { 56, 151}, { 36, 144}, { 64, 140},
{4095,133}, { 10, 130}, {  9, 124}, {  3, 124}, { 28, 120}, {  0, 118},
{288, 110}, {1152, 96}, {104,  90}, {  1,  86}, {832,  76}, {248,  74},
{1024, 69}, {120,  64}, {512,  63}, {384,  60}, {  6,  59}, { 80,  54},
{ 17,  50}, {  7,  49}, {520,  47}, {2048, 39}, {256,  37}, {864,  33},
{1440, 28}, { 22,  27}, {2056, 24}, {260,  23}, { 68,  23}, {  5,  22},
{ 18,  21}, {200,  18}, {2120, 18}, { 60,  17}, { 52,  16}, {336,  15},
{ 44,  13}, {192,  13}, {160,  12}, {2064, 12}, {128,  12}, { 76,  11},
{164,  11}, {152,  10}, {136,   9}, {488,   7}, { 96,   6}, {560,   6},
{1016,  6}, {112,   5}, {232,   5}, {168,   5}, {952,   5}, {184,   5},
{144,   4}, {252,   4}, { 84,   3}, {960,   3}, {3808,  3}, {244,   3},
{280,   3}, {224,   3}, {156,   3}, {1088,  3}, {440,   3}, {216,   2},
{304,   2}, { 23,   2}, { 25,   2}, { 26,   2}, {264,   2}, {328,   2},
{1096,  2}, {240,   2}, {1104,  2}, {704,   2}, {1664,  2}, {360,   2},
{808,   1}, {544,   1}, {236,   1}, {720,   1}, {368,   1}, {424,   1},
{640,   1}, {1112,  1}, {552,   1}, {272,   1}, {776,   1}, {376,   1},
{ 92,   1}, {536,   1}, {824,   1}, {496,   1}, {760,   1}, {792,   1},
{504,   1}, {344,   1}, {1816,  1}, {880,   1}, {176,   1}, {320,   1},
{352,   1}, {2008,  1}, {208,   1}, {408,   1}, {228,   1}, {2072,  1},
{568,   1}, {220,   1}, {616,   1}, {600,   1}, {392,   1}, {696,   1},
{2144,  1}, {1280,  1}, {2136,  1}, {632,   1}, {584,   1}, {456,   1},
{472,   1}, {3440,  1}, {2088,  1}, {680,   1}, {2928,  1}, {212,   1},
{648,   1}, {1752,  1}, {664,   1}, {3512,  1}, {1032,  1}, {528,   1},
{4072,  1}, {204,   1}, {2880,  1}, {3392,  1}, {712,   1}, { 59,   1},
{736,   1}, {592,   1}, {2520,  1}, {744,   1}, {196,   1}, {172,   1},
{728,   1}, {2040,  1}, {1192,  1}, {3600,  1}, {0, 0}
};

#define ALIGN_NUM 1024
#define ALIGN_MASK (ALIGN_NUM-1)
static uint8_t align_arr[ALIGN_NUM];

/* Alignment data for memset based on SPEC2017.  */
static align_data_t memset_align_freq[] =
{
 {16, 338}, {8, 307}, {32, 148}, {64, 131}, {4, 72}, {1, 23}, {2, 5}, {0, 0}
};

static void
init_memset_distribution (void)
{
  int i, j, freq, size, n;

  for (n = i = 0; (freq = memset_len_freq[i].freq) != 0; i++)
    for (j = 0, size = memset_len_freq[i].size; j < freq; j++)
      len_arr[n++] = size;
  assert (n == SIZE_NUM);

  for (n = i = 0; (freq = memset_align_freq[i].freq) != 0; i++)
    for (j = 0, size = memset_align_freq[i].align; j < freq; j++)
      align_arr[n++] = size - 1;
  assert (n == ALIGN_NUM);
}

static size_t
init_memset (size_t max_size)
{
  size_t total = 0;
  /* Create a random set of memsets with the given size and alignment
     distributions.  */
  for (int i = 0; i < NUM_TESTS; i++)
    {
      test_arr[i].offset = (rand32 (0) & (max_size - 1));
      test_arr[i].offset &= ~align_arr[rand32 (0) & ALIGN_MASK];
      test_arr[i].len = len_arr[rand32 (0) & SIZE_MASK];
      total += test_arr[i].len;
    }

  return total;
}

static void inline __attribute ((always_inline))
memset_random (const char *name, void *(*set)(void *, int, size_t))
{
  uint64_t total_size = 0;
  uint64_t tsum = 0;
  printf ("%22s ", name);
  rand32 (0x12345678);

  for (int size = MIN_SIZE; size <= MAX_SIZE; size *= 2)
    {
      uint64_t memset_size = init_memset (size) * ITERS;

      for (int c = 0; c < NUM_TESTS; c++)
	set (a + test_arr[c].offset, 0, test_arr[c].len);

      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS; i++)
	for (int c = 0; c < NUM_TESTS; c++)
	  set (a + test_arr[c].offset, 0, test_arr[c].len);
      t = clock_get_ns () - t;
      total_size += memset_size;
      tsum += t;
      printf ("%dK: %5.2f ", size / 1024, (double)memset_size / t);
    }
  printf( "avg %5.2f\n", (double)total_size / tsum);
}

static void inline __attribute ((always_inline))
memset_medium (const char *name, void *(*set)(void *, int, size_t))
{
  printf ("%22s ", name);

  for (int size = 8; size <= 512; size *= 2)
    {
      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS2; i++)
	set (a, 0, size);
      t = clock_get_ns () - t;
      printf ("%dB: %5.2f ", size, (double)size * ITERS2 / t);
    }
  printf ("\n");
}

static void inline __attribute ((always_inline))
memset_large (const char *name, void *(*set)(void *, int, size_t))
{
  printf ("%22s ", name);

  for (int size = 1024; size <= 65536; size *= 2)
    {
      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS3; i++)
	set (a, 0, size);
      t = clock_get_ns () - t;
      printf ("%dKB: %6.2f ", size / 1024, (double)size * ITERS3 / t);
    }
  printf ("\n");
}

int main (void)
{
  init_memset_distribution ();

  memset (a, 1, sizeof (a));

  DOTEST ("Random memset (bytes/ns):\n", memset_random);
  DOTEST ("Medium memset (bytes/ns):\n", memset_medium);
  DOTEST ("Large memset (bytes/ns):\n", memset_large);
  return 0;
}
