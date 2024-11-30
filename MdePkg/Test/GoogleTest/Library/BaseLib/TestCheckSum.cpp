/** @file
  Unit tests for BaseLib's checksum capabilities.

  Copyright (c) 2023 Pedro Falcato. All rights reserved<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <gtest/gtest.h>
extern "C" {
  #include <Base.h>
  #include <Library/BaseLib.h>
}

// Precomputed crc32c and crc16-ansi for "hello" (without the null byte)
constexpr STATIC UINT32  mHelloCrc32c = 0x9A71BB4C;
constexpr STATIC UINT16  mHelloCrc16  = 0x34F6;

TEST (Crc32c, BasicCheck) {
  // Note: The magic numbers below are precomputed checksums
  // Check for basic operation on even and odd numbers of bytes
  EXPECT_EQ (CalculateCrc32c ("hello", 5, 0), mHelloCrc32c);
  EXPECT_EQ (
    CalculateCrc32c ("longlonglonglonglong", sizeof ("longlonglonglonglong") - 1, 0),
    0xC50F869D
    );
  EXPECT_EQ (CalculateCrc32c ("h", 1, 0), 0xB96298FC);

  // Check if a checksum with no bytes correctly yields 0
  EXPECT_EQ (CalculateCrc32c ("", 0, 0), 0U);
}

TEST (Crc32c, MultipartCheck) {
  // Test multi-part crc32c calculation. So that given a string of bytes
  // s[N], crc32c(s, N, 0) == crc32c(s[N - 1], 1, crc32c(s, N - 1, 0))
  // and all other sorts of combinations one might imagine.
  UINT32  val;

  val = CalculateCrc32c ("hel", 3, 0);
  EXPECT_EQ (CalculateCrc32c (&"hello"[3], 2, val), mHelloCrc32c);
}

TEST (Crc16, BasicCheck) {
  // Note: The magic numbers below are precomputed checksums
  // Check for basic operation on even and odd numbers of bytes
  EXPECT_EQ (CalculateCrc16Ansi ("hello", 5, CRC16ANSI_INIT), mHelloCrc16);
  EXPECT_EQ (
    CalculateCrc16Ansi ("longlonglonglonglong", sizeof ("longlonglonglonglong") - 1, CRC16ANSI_INIT),
    0xF723
    );
  EXPECT_EQ (CalculateCrc16Ansi ("h", 1, CRC16ANSI_INIT), 0xAEBE);

  // Check if a checksum with no bytes correctly yields CRC16ANSI_INIT
  EXPECT_EQ (CalculateCrc16Ansi ("", 0, CRC16ANSI_INIT), CRC16ANSI_INIT);
}

TEST (Crc16, MultipartCheck) {
  // Test multi-part crc16 calculation. So that given a string of bytes
  // s[N], crc16(s, N, 0) == crc16(s[N - 1], 1, crc16(s, N - 1, 0))
  // and all other sorts of combinations one might imagine.
  UINT16  val;

  val = CalculateCrc16Ansi ("hel", 3, CRC16ANSI_INIT);
  EXPECT_EQ (CalculateCrc16Ansi (&"hello"[3], 2, val), mHelloCrc16);
}
