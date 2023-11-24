/*
 * memcpy benchmark.
 *
 * Copyright (c) 2020-2022, Arm Limited.
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

static uint8_t a[MAX_SIZE + 4096 + 64] __attribute__((__aligned__(64)));
static uint8_t b[MAX_SIZE + 4096 + 64] __attribute__((__aligned__(64)));

#define F(x) {#x, x},

static const struct fun
{
  const char *name;
  void *(*fun)(void *, const void *, size_t);
} funtab[] =
{
#if __aarch64__
  F(__memcpy_aarch64)
# if __ARM_NEON
  F(__memcpy_aarch64_simd)
# endif
# if __ARM_FEATURE_SVE
  F(__memcpy_aarch64_sve)
# endif
#elif __arm__
  F(__memcpy_arm)
#endif
  F(memcpy)
#undef F
  {0, 0}
};

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

int main (void)
{
  init_copy_distribution ();

  memset (a, 1, sizeof (a));
  memset (b, 2, sizeof (b));

  printf("Random memcpy (bytes/ns):\n");
  for (int f = 0; funtab[f].name != 0; f++)
    {
      size_t total = 0;
      uint64_t tsum = 0;
      printf ("%22s ", funtab[f].name);
      rand32 (0x12345678);

      for (int size = MIN_SIZE; size <= MAX_SIZE; size *= 2)
	{
	  size_t copy_size = init_copies (size) * ITERS;

	  for (int c = 0; c < NUM_TESTS; c++)
	    funtab[f].fun (b + test_arr[c].dst, a + test_arr[c].src,
			   test_arr[c].len);

	  uint64_t t = clock_get_ns ();
	  for (int i = 0; i < ITERS; i++)
	    for (int c = 0; c < NUM_TESTS; c++)
	      funtab[f].fun (b + test_arr[c].dst, a + test_arr[c].src,
			     test_arr[c].len);
	  t = clock_get_ns () - t;
	  total += copy_size;
	  tsum += t;
	  printf ("%dK: %.2f ", size / 1024, (double)copy_size / t);
	}
      printf( "avg %.2f\n", (double)total / tsum);
    }

  size_t total = 0;
  uint64_t tsum = 0;
  printf ("%22s ", "memcpy_call");
  rand32 (0x12345678);

  for (int size = MIN_SIZE; size <= MAX_SIZE; size *= 2)
    {
      size_t copy_size = init_copies (size) * ITERS;

      for (int c = 0; c < NUM_TESTS; c++)
	memcpy (b + test_arr[c].dst, a + test_arr[c].src, test_arr[c].len);

      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS; i++)
	for (int c = 0; c < NUM_TESTS; c++)
	  memcpy (b + test_arr[c].dst, a + test_arr[c].src, test_arr[c].len);
      t = clock_get_ns () - t;
      total += copy_size;
      tsum += t;
      printf ("%dK: %.2f ", size / 1024, (double)copy_size / t);
    }
  printf( "avg %.2f\n", (double)total / tsum);


  printf ("\nAligned medium memcpy (bytes/ns):\n");
  for (int f = 0; funtab[f].name != 0; f++)
    {
      printf ("%22s ", funtab[f].name);

      for (int size = 8; size <= 512; size *= 2)
	{
	  uint64_t t = clock_get_ns ();
	  for (int i = 0; i < ITERS2; i++)
	    funtab[f].fun (b, a, size);
	  t = clock_get_ns () - t;
	  printf ("%dB: %.2f ", size, (double)size * ITERS2 / t);
	}
      printf ("\n");
    }

  printf ("%22s ", "memcpy_call");
  for (int size = 8; size <= 512; size *= 2)
    {
      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS2; i++)
	memcpy (b, a, size);
      t = clock_get_ns () - t;
      printf ("%dB: %.2f ", size, (double)size * ITERS2 / t);
    }
  printf ("\n");


  printf ("\nUnaligned medium memcpy (bytes/ns):\n");
  for (int f = 0; funtab[f].name != 0; f++)
    {
      printf ("%22s ", funtab[f].name);

      for (int size = 8; size <= 512; size *= 2)
	{
	  uint64_t t = clock_get_ns ();
	  for (int i = 0; i < ITERS2; i++)
	    funtab[f].fun (b + 3, a + 1, size);
	  t = clock_get_ns () - t;
	  printf ("%dB: %.2f ", size, (double)size * ITERS2 / t);
	}
      printf ("\n");
    }

  printf ("%22s ", "memcpy_call");
  for (int size = 8; size <= 512; size *= 2)
    {
      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS2; i++)
	memcpy (b + 3, a + 1, size);
      t = clock_get_ns () - t;
      printf ("%dB: %.2f ", size, (double)size * ITERS2 / t);
    }
  printf ("\n");


  printf ("\nLarge memcpy (bytes/ns):\n");
  for (int f = 0; funtab[f].name != 0; f++)
    {
      printf ("%22s ", funtab[f].name);

      for (int size = 1024; size <= 65536; size *= 2)
	{
	  uint64_t t = clock_get_ns ();
	  for (int i = 0; i < ITERS3; i++)
	    funtab[f].fun (b, a, size);
	  t = clock_get_ns () - t;
	  printf ("%dK: %.2f ", size / 1024, (double)size * ITERS3 / t);
	}
      printf ("\n");
    }

  printf ("%22s ", "memcpy_call");
  for (int size = 1024; size <= 65536; size *= 2)
    {
      uint64_t t = clock_get_ns ();
      for (int i = 0; i < ITERS3; i++)
	memcpy (b, a, size);
      t = clock_get_ns () - t;
      printf ("%dK: %.2f ", size / 1024, (double)size * ITERS3 / t);
    }
  printf ("\n");


  printf ("\nUnaligned forwards memmove (bytes/ns):\n");
  for (int f = 0; funtab[f].name != 0; f++)
    {
      printf ("%22s ", funtab[f].name);

      for (int size = 1024; size <= 65536; size *= 2)
	{
	  uint64_t t = clock_get_ns ();
	  for (int i = 0; i < ITERS3; i++)
	    funtab[f].fun (a, a + 256 + (i & 31), size);
	  t = clock_get_ns () - t;
	  printf ("%dK: %.2f ", size / 1024, (double)size * ITERS3 / t);
	}
      printf ("\n");
    }


  printf ("\nUnaligned backwards memmove (bytes/ns):\n");
  for (int f = 0; funtab[f].name != 0; f++)
    {
      printf ("%22s ", funtab[f].name);

      for (int size = 1024; size <= 65536; size *= 2)
	{
	  uint64_t t = clock_get_ns ();
	  for (int i = 0; i < ITERS3; i++)
	    funtab[f].fun (a + 256 + (i & 31), a, size);
	  t = clock_get_ns () - t;
	  printf ("%dK: %.2f ", size / 1024, (double)size * ITERS3 / t);
	}
      printf ("\n");
    }
  printf ("\n");

  return 0;
}
