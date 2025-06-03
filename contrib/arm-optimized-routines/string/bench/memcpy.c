/*
 * memcpy benchmark.
 *
 * Copyright (c) 2020-2023, Arm Limited.
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
#define ITERS3 200000
#define NUM_TESTS 16384
#define MIN_SIZE 32768
#define MAX_SIZE (1024 * 1024)

static uint8_t a[MAX_SIZE + 4096 + 64] __attribute__((__aligned__(4096)));
static uint8_t b[MAX_SIZE + 4096 + 64] __attribute__((__aligned__(4096)));

#define DOTEST(STR,TESTFN)			\
  printf (STR);					\
  RUN (TESTFN, memcpy);				\
  RUNA64 (TESTFN, __memcpy_aarch64);		\
  RUNA64 (TESTFN, __memcpy_aarch64_simd);	\
  RUNSVE (TESTFN, __memcpy_aarch64_sve);	\
  RUNMOPS (TESTFN, __memcpy_aarch64_mops);	\
  RUNA32 (TESTFN, __memcpy_arm);		\
  printf ("\n");

typedef struct { uint16_t size; uint16_t freq; } freq_data_t;
typedef struct { uint8_t align; uint16_t freq; } align_data_t;

#define SIZE_NUM 65536
#define SIZE_MASK (SIZE_NUM-1)
static uint8_t size_arr[SIZE_NUM];

/* Frequency data for memcpy of less than 4096 bytes based on SPEC2017.  */
static freq_data_t size_freq[] =
{
{32,22320}, { 16,9554}, {  8,8915}, {152,5327}, {  4,2159}, {292,2035},
{ 12,1608}, { 24,1343}, {1152,895}, {144, 813}, {884, 733}, {284, 721},
{120, 661}, {  2, 649}, {882, 550}, {  5, 475}, {  7, 461}, {108, 460},
{ 10, 361}, {  9, 361}, {  6, 334}, {  3, 326}, {464, 308}, {2048,303},
{  1, 298}, { 64, 250}, { 11, 197}, {296, 194}, { 68, 187}, { 15, 185},
{192, 184}, {1764,183}, { 13, 173}, {560, 126}, {160, 115}, {288,  96},
{104,  96}, {1144, 83}, { 18,  80}, { 23,  78}, { 40,  77}, { 19,  68},
{ 48,  63}, { 17,  57}, { 72,  54}, {1280, 51}, { 20,  49}, { 28,  47},
{ 22,  46}, {640,  45}, { 25,  41}, { 14,  40}, { 56,  37}, { 27,  35},
{ 35,  33}, {384,  33}, { 29,  32}, { 80,  30}, {4095, 22}, {232,  22},
{ 36,  19}, {184,  17}, { 21,  17}, {256,  16}, { 44,  15}, { 26,  15},
{ 31,  14}, { 88,  14}, {176,  13}, { 33,  12}, {1024, 12}, {208,  11},
{ 62,  11}, {128,  10}, {704,  10}, {324,  10}, { 96,  10}, { 60,   9},
{136,   9}, {124,   9}, { 34,   8}, { 30,   8}, {480,   8}, {1344,  8},
{273,   7}, {520,   7}, {112,   6}, { 52,   6}, {344,   6}, {336,   6},
{504,   5}, {168,   5}, {424,   5}, {  0,   4}, { 76,   3}, {200,   3},
{512,   3}, {312,   3}, {240,   3}, {960,   3}, {264,   2}, {672,   2},
{ 38,   2}, {328,   2}, { 84,   2}, { 39,   2}, {216,   2}, { 42,   2},
{ 37,   2}, {1608,  2}, { 70,   2}, { 46,   2}, {536,   2}, {280,   1},
{248,   1}, { 47,   1}, {1088,  1}, {1288,  1}, {224,   1}, { 41,   1},
{ 50,   1}, { 49,   1}, {808,   1}, {360,   1}, {440,   1}, { 43,   1},
{ 45,   1}, { 78,   1}, {968,   1}, {392,   1}, { 54,   1}, { 53,   1},
{ 59,   1}, {376,   1}, {664,   1}, { 58,   1}, {272,   1}, { 66,   1},
{2688,  1}, {472,   1}, {568,   1}, {720,   1}, { 51,   1}, { 63,   1},
{ 86,   1}, {496,   1}, {776,   1}, { 57,   1}, {680,   1}, {792,   1},
{122,   1}, {760,   1}, {824,   1}, {552,   1}, { 67,   1}, {456,   1},
{984,   1}, { 74,   1}, {408,   1}, { 75,   1}, { 92,   1}, {576,   1},
{116,   1}, { 65,   1}, {117,   1}, { 82,   1}, {352,   1}, { 55,   1},
{100,   1}, { 90,   1}, {696,   1}, {111,   1}, {880,   1}, { 79,   1},
{488,   1}, { 61,   1}, {114,   1}, { 94,   1}, {1032,  1}, { 98,   1},
{ 87,   1}, {584,   1}, { 85,   1}, {648,   1}, {0, 0}
};

#define ALIGN_NUM 1024
#define ALIGN_MASK (ALIGN_NUM-1)
static uint8_t src_align_arr[ALIGN_NUM];
static uint8_t dst_align_arr[ALIGN_NUM];

/* Source alignment frequency for memcpy based on SPEC2017.  */
static align_data_t src_align_freq[] =
{
  {8, 300}, {16, 292}, {32, 168}, {64, 153}, {4, 79}, {2, 14}, {1, 18}, {0, 0}
};

static align_data_t dst_align_freq[] =
{
  {8, 265}, {16, 263}, {64, 209}, {32, 174}, {4, 90}, {2, 10}, {1, 13}, {0, 0}
};

typedef struct
{
  uint64_t src : 24;
  uint64_t dst : 24;
  uint64_t len : 16;
} copy_t;

static copy_t test_arr[NUM_TESTS];

typedef char *(*proto_t) (char *, const char *, size_t);

static void
init_copy_distribution (void)
{
  int i, j, freq, size, n;

  for (n = i = 0; (freq = size_freq[i].freq) != 0; i++)
    for (j = 0, size = size_freq[i].size; j < freq; j++)
      size_arr[n++] = size;
  assert (n == SIZE_NUM);

  for (n = i = 0; (freq = src_align_freq[i].freq) != 0; i++)
    for (j = 0, size = src_align_freq[i].align; j < freq; j++)
      src_align_arr[n++] = size - 1;
  assert (n == ALIGN_NUM);

  for (n = i = 0; (freq = dst_align_freq[i].freq) != 0; i++)
    for (j = 0, size = dst_align_freq[i].align; j < freq; j++)
      dst_align_arr[n++] = size - 1;
  assert (n == ALIGN_NUM);
}

static size_t
init_copies (size_t max_size)
{
  size_t total = 0;
  /* Create a random set of copies with the given size and alignment
     distributions.  */
  for (int i = 0; i < NUM_TESTS; i++)
    {
      test_arr[i].dst = (rand32 (0) & (max_size - 1));
      test_arr[i].dst &= ~dst_align_arr[rand32 (0) & ALIGN_MASK];
      test_arr[i].src = (rand32 (0) & (max_size - 1));
      test_arr[i].src &= ~src_align_arr[rand32 (0) & ALIGN_MASK];
      test_arr[i].len = size_arr[rand32 (0) & SIZE_MASK];
      total += test_arr[i].len;
    }

  return total;
}

static void inline __attribute ((always_inline))
memcpy_random (const char *name, void *(*fn)(void *, const void *, size_t))
{
  printf ("%22s ", name);
  uint64_t total = 0, tsum = 0;
  for (int size = MIN_SIZE; size <= MAX_SIZE; size *= 2)
    {
      uint64_t copy_size = init_copies (size) * ITERS;

      for (int c = 0; c < NUM_TESTS; c++)
	fn (b + test_arr[c].dst, a + test_arr[c].src, test_arr[c].len);

      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS; i++)
	for (int c = 0; c < NUM_TESTS; c++)
	  fn (b + test_arr[c].dst, a + test_arr[c].src, test_arr[c].len);
      t = clock_get_ns () - t;
      total += copy_size;
      tsum += t;
      printf ("%dK: %5.2f ", size / 1024, (double)copy_size / t);
    }
  printf( "avg %5.2f\n", (double)total / tsum);
}

static void inline __attribute ((always_inline))
memcpy_medium_aligned (const char *name, void *(*fn)(void *, const void *, size_t))
{
  printf ("%22s ", name);

  for (int size = 8; size <= 512; size *= 2)
    {
      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS2; i++)
	fn (b, a, size);
      t = clock_get_ns () - t;
      printf ("%dB: %5.2f ", size, (double)size * ITERS2 / t);
    }
  printf ("\n");
}

static void inline __attribute ((always_inline))
memcpy_medium_unaligned (const char *name, void *(*fn)(void *, const void *, size_t))
{
  printf ("%22s ", name);

  for (int size = 8; size <= 512; size *= 2)
    {
      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS2; i++)
	fn (b + 3, a + 1, size);
      t = clock_get_ns () - t;
      printf ("%dB: %5.2f ", size, (double)size * ITERS2 / t);
    }
  printf ("\n");
}

static void inline __attribute ((always_inline))
memcpy_large (const char *name, void *(*fn)(void *, const void *, size_t))
{
  printf ("%22s ", name);

  for (int size = 1024; size <= 65536; size *= 2)
    {
      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS3; i++)
	fn (b, a, size);
      t = clock_get_ns () - t;
      printf ("%dK: %5.2f ", size / 1024, (double)size * ITERS3 / t);
    }
  printf ("\n");
}

static void inline __attribute ((always_inline))
memmove_forward_unaligned (const char *name, void *(*fn)(void *, const void *, size_t))
{
  printf ("%22s ", name);

  for (int size = 1024; size <= 65536; size *= 2)
    {
      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS3; i++)
        fn (a, a + 256 + (i & 31), size);
      t = clock_get_ns () - t;
      printf ("%dK: %5.2f ", size / 1024, (double)size * ITERS3 / t);
    }

  printf ("\n");
}

static void inline __attribute ((always_inline))
memmove_backward_unaligned (const char *name, void *(*fn)(void *, const void *, size_t))
{
  printf ("%22s ", name);

  for (int size = 1024; size <= 65536; size *= 2)
    {
      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS3; i++)
	fn (a + 256 + (i & 31), a, size);
      t = clock_get_ns () - t;
      printf ("%dK: %5.2f ", size / 1024, (double)size * ITERS3 / t);
    }

  printf ("\n");
}

int main (void)
{
  init_copy_distribution ();

  memset (a, 1, sizeof (a));
  memset (b, 2, sizeof (b));

  DOTEST ("Random memcpy (bytes/ns):\n", memcpy_random);
  DOTEST ("Medium memcpy aligned (bytes/ns):\n", memcpy_medium_aligned);
  DOTEST ("Medium memcpy unaligned (bytes/ns):\n", memcpy_medium_unaligned);
  DOTEST ("Large memcpy (bytes/ns):\n", memcpy_large);
  DOTEST ("Forwards memmove unaligned (bytes/ns):\n", memmove_forward_unaligned);
  DOTEST ("Backwards memmove unaligned (bytes/ns):\n", memmove_backward_unaligned);

  return 0;
}
