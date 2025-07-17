/*-
 * Copyright (c) 2023 Dag-Erling Smørgrav
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limits.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>

#include <atf-c.h>

static const struct sscanf_test_case {
	char input[8];
	struct {
		int ret, val, len;
	} b, o, d, x, i;
} sscanf_test_cases[] = {
//	input		binary		octal		decimal		hexadecimal	automatic
	// all digits
	{ "0",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 }, },
	{ "1",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 }, },
	{ "2",		{ 0,   0, 0 },	{ 1,   2, 1 },	{ 1,   2, 1 },	{ 1,   2, 1 },	{ 1,   2, 1 }, },
	{ "3",		{ 0,   0, 0 },	{ 1,   3, 1 },	{ 1,   3, 1 },	{ 1,   3, 1 },	{ 1,   3, 1 }, },
	{ "4",		{ 0,   0, 0 },	{ 1,   4, 1 },	{ 1,   4, 1 },	{ 1,   4, 1 },	{ 1,   4, 1 }, },
	{ "5",		{ 0,   0, 0 },	{ 1,   5, 1 },	{ 1,   5, 1 },	{ 1,   5, 1 },	{ 1,   5, 1 }, },
	{ "6",		{ 0,   0, 0 },	{ 1,   6, 1 },	{ 1,   6, 1 },	{ 1,   6, 1 },	{ 1,   6, 1 }, },
	{ "7",		{ 0,   0, 0 },	{ 1,   7, 1 },	{ 1,   7, 1 },	{ 1,   7, 1 },	{ 1,   7, 1 }, },
	{ "8",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,   8, 1 },	{ 1,   8, 1 },	{ 1,   8, 1 }, },
	{ "9",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,   9, 1 },	{ 1,   9, 1 },	{ 1,   9, 1 }, },
	{ "A",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  10, 1 },	{ 0,   0, 0 }, },
	{ "B",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  11, 1 },	{ 0,   0, 0 }, },
	{ "C",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  12, 1 },	{ 0,   0, 0 }, },
	{ "D",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  13, 1 },	{ 0,   0, 0 }, },
	{ "E",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  14, 1 },	{ 0,   0, 0 }, },
	{ "F",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  15, 1 },	{ 0,   0, 0 }, },
	{ "X",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 }, },
	{ "a",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  10, 1 },	{ 0,   0, 0 }, },
	{ "b",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  11, 1 },	{ 0,   0, 0 }, },
	{ "c",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  12, 1 },	{ 0,   0, 0 }, },
	{ "d",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  13, 1 },	{ 0,   0, 0 }, },
	{ "e",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  14, 1 },	{ 0,   0, 0 }, },
	{ "f",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  15, 1 },	{ 0,   0, 0 }, },
	{ "x",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 }, },
	// all digits with leading zero
	{ "00",		{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 }, },
	{ "01",		{ 1,   1, 2 },	{ 1,   1, 2 },	{ 1,   1, 2 },	{ 1,   1, 2 },	{ 1,   1, 2 }, },
	{ "02",		{ 1,   0, 1 },	{ 1,   2, 2 },	{ 1,   2, 2 },	{ 1,   2, 2 },	{ 1,   2, 2 }, },
	{ "03",		{ 1,   0, 1 },	{ 1,   3, 2 },	{ 1,   3, 2 },	{ 1,   3, 2 },	{ 1,   3, 2 }, },
	{ "04",		{ 1,   0, 1 },	{ 1,   4, 2 },	{ 1,   4, 2 },	{ 1,   4, 2 },	{ 1,   4, 2 }, },
	{ "05",		{ 1,   0, 1 },	{ 1,   5, 2 },	{ 1,   5, 2 },	{ 1,   5, 2 },	{ 1,   5, 2 }, },
	{ "06",		{ 1,   0, 1 },	{ 1,   6, 2 },	{ 1,   6, 2 },	{ 1,   6, 2 },	{ 1,   6, 2 }, },
	{ "07",		{ 1,   0, 1 },	{ 1,   7, 2 },	{ 1,   7, 2 },	{ 1,   7, 2 },	{ 1,   7, 2 }, },
	{ "08",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   8, 2 },	{ 1,   8, 2 },	{ 1,   0, 1 }, },
	{ "09",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   9, 2 },	{ 1,   9, 2 },	{ 1,   0, 1 }, },
	{ "0A",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  10, 2 },	{ 1,   0, 1 }, },
	{ "0B",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  11, 2 },	{ 1,   0, 1 }, },
	{ "0C",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  12, 2 },	{ 1,   0, 1 }, },
	{ "0D",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  13, 2 },	{ 1,   0, 1 }, },
	{ "0E",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  14, 2 },	{ 1,   0, 1 }, },
	{ "0F",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  15, 2 },	{ 1,   0, 1 }, },
	{ "0X",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 }, },
	{ "0a",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  10, 2 },	{ 1,   0, 1 }, },
	{ "0b",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  11, 2 },	{ 1,   0, 1 }, },
	{ "0c",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  12, 2 },	{ 1,   0, 1 }, },
	{ "0d",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  13, 2 },	{ 1,   0, 1 }, },
	{ "0e",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  14, 2 },	{ 1,   0, 1 }, },
	{ "0f",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  15, 2 },	{ 1,   0, 1 }, },
	{ "0x",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 }, },
	// all digits with leading one
	{ "10",		{ 1,   2, 2 },	{ 1,   8, 2 },	{ 1,  10, 2 },	{ 1,  16, 2 },	{ 1,  10, 2 }, },
	{ "11",		{ 1,   3, 2 },	{ 1,   9, 2 },	{ 1,  11, 2 },	{ 1,  17, 2 },	{ 1,  11, 2 }, },
	{ "12",		{ 1,   1, 1 },	{ 1,  10, 2 },	{ 1,  12, 2 },	{ 1,  18, 2 },	{ 1,  12, 2 }, },
	{ "13",		{ 1,   1, 1 },	{ 1,  11, 2 },	{ 1,  13, 2 },	{ 1,  19, 2 },	{ 1,  13, 2 }, },
	{ "14",		{ 1,   1, 1 },	{ 1,  12, 2 },	{ 1,  14, 2 },	{ 1,  20, 2 },	{ 1,  14, 2 }, },
	{ "15",		{ 1,   1, 1 },	{ 1,  13, 2 },	{ 1,  15, 2 },	{ 1,  21, 2 },	{ 1,  15, 2 }, },
	{ "16",		{ 1,   1, 1 },	{ 1,  14, 2 },	{ 1,  16, 2 },	{ 1,  22, 2 },	{ 1,  16, 2 }, },
	{ "17",		{ 1,   1, 1 },	{ 1,  15, 2 },	{ 1,  17, 2 },	{ 1,  23, 2 },	{ 1,  17, 2 }, },
	{ "18",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  18, 2 },	{ 1,  24, 2 },	{ 1,  18, 2 }, },
	{ "19",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  19, 2 },	{ 1,  25, 2 },	{ 1,  19, 2 }, },
	{ "1A",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  26, 2 },	{ 1,   1, 1 }, },
	{ "1B",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  27, 2 },	{ 1,   1, 1 }, },
	{ "1C",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  28, 2 },	{ 1,   1, 1 }, },
	{ "1D",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  29, 2 },	{ 1,   1, 1 }, },
	{ "1E",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  30, 2 },	{ 1,   1, 1 }, },
	{ "1F",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  31, 2 },	{ 1,   1, 1 }, },
	{ "1X",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 }, },
	{ "1a",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  26, 2 },	{ 1,   1, 1 }, },
	{ "1b",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  27, 2 },	{ 1,   1, 1 }, },
	{ "1c",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  28, 2 },	{ 1,   1, 1 }, },
	{ "1d",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  29, 2 },	{ 1,   1, 1 }, },
	{ "1e",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  30, 2 },	{ 1,   1, 1 }, },
	{ "1f",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  31, 2 },	{ 1,   1, 1 }, },
	{ "1x",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 }, },
	// all digits with leading binary prefix
	{ "0b0",	{ 1,   0, 3 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 176, 3 },	{ 1,   0, 3 }, },
	{ "0b1",	{ 1,   1, 3 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 177, 3 },	{ 1,   1, 3 }, },
	{ "0b2",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 178, 3 },	{ 1,   0, 1 }, },
	{ "0b3",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 179, 3 },	{ 1,   0, 1 }, },
	{ "0b4",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 180, 3 },	{ 1,   0, 1 }, },
	{ "0b5",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 181, 3 },	{ 1,   0, 1 }, },
	{ "0b6",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 182, 3 },	{ 1,   0, 1 }, },
	{ "0b7",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 183, 3 },	{ 1,   0, 1 }, },
	{ "0b8",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 184, 3 },	{ 1,   0, 1 }, },
	{ "0b9",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 185, 3 },	{ 1,   0, 1 }, },
	{ "0bA",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 186, 3 },	{ 1,   0, 1 }, },
	{ "0bB",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 187, 3 },	{ 1,   0, 1 }, },
	{ "0bC",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 188, 3 },	{ 1,   0, 1 }, },
	{ "0bD",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 189, 3 },	{ 1,   0, 1 }, },
	{ "0bE",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 190, 3 },	{ 1,   0, 1 }, },
	{ "0bF",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 191, 3 },	{ 1,   0, 1 }, },
	{ "0bX",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  11, 2 },	{ 1,   0, 1 }, },
	{ "0ba",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 186, 3 },	{ 1,   0, 1 }, },
	{ "0bb",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 187, 3 },	{ 1,   0, 1 }, },
	{ "0bc",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 188, 3 },	{ 1,   0, 1 }, },
	{ "0bd",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 189, 3 },	{ 1,   0, 1 }, },
	{ "0be",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 190, 3 },	{ 1,   0, 1 }, },
	{ "0bf",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 191, 3 },	{ 1,   0, 1 }, },
	{ "0bx",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  11, 2 },	{ 1,   0, 1 }, },
	// all digits with leading hexadecimal prefix
	{ "0x0",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 3 },	{ 1,   0, 3 }, },
	{ "0x1",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   1, 3 },	{ 1,   1, 3 }, },
	{ "0x2",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   2, 3 },	{ 1,   2, 3 }, },
	{ "0x3",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   3, 3 },	{ 1,   3, 3 }, },
	{ "0x4",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   4, 3 },	{ 1,   4, 3 }, },
	{ "0x5",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   5, 3 },	{ 1,   5, 3 }, },
	{ "0x6",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   6, 3 },	{ 1,   6, 3 }, },
	{ "0x7",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   7, 3 },	{ 1,   7, 3 }, },
	{ "0x8",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   8, 3 },	{ 1,   8, 3 }, },
	{ "0x9",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   9, 3 },	{ 1,   9, 3 }, },
	{ "0xA",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  10, 3 },	{ 1,  10, 3 }, },
	{ "0xB",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  11, 3 },	{ 1,  11, 3 }, },
	{ "0xC",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  12, 3 },	{ 1,  12, 3 }, },
	{ "0xD",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  13, 3 },	{ 1,  13, 3 }, },
	{ "0xE",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  14, 3 },	{ 1,  14, 3 }, },
	{ "0xF",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  15, 3 },	{ 1,  15, 3 }, },
	{ "0xX",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 }, },
	{ "0xa",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  10, 3 },	{ 1,  10, 3 }, },
	{ "0xb",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  11, 3 },	{ 1,  11, 3 }, },
	{ "0xc",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  12, 3 },	{ 1,  12, 3 }, },
	{ "0xd",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  13, 3 },	{ 1,  13, 3 }, },
	{ "0xe",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  14, 3 },	{ 1,  14, 3 }, },
	{ "0xf",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  15, 3 },	{ 1,  15, 3 }, },
	{ "0xX",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 }, },
	// terminator
	{ "" }
};

#define SSCANF_TEST(string, format, expret, expval, explen)		\
	do {								\
		int ret = 0, val = 0, len = 0;				\
		ret = sscanf(string, format "%n", &val, &len);		\
		ATF_CHECK_EQ(expret, ret);				\
		if (expret && ret) {					\
			ATF_CHECK_EQ(expval, val);			\
			ATF_CHECK_EQ(explen, len);			\
		}							\
	} while (0)

ATF_TC_WITHOUT_HEAD(sscanf_b);
ATF_TC_BODY(sscanf_b, tc)
{
	const struct sscanf_test_case *stc;
	char input[16];

	for (stc = sscanf_test_cases; *stc->input; stc++) {
		strcpy(input + 1, stc->input);
		SSCANF_TEST(input + 1, "%b", stc->b.ret, stc->b.val, stc->b.len);
		input[0] = '+';
		SSCANF_TEST(input, "%b", stc->b.ret, stc->b.val, stc->b.len ? stc->b.len + 1 : 0);
		input[0] = '-';
		SSCANF_TEST(input, "%b", stc->b.ret, -stc->b.val, stc->b.len ? stc->b.len + 1 : 0);
	}
}

ATF_TC_WITHOUT_HEAD(sscanf_o);
ATF_TC_BODY(sscanf_o, tc)
{
	const struct sscanf_test_case *stc;
	char input[16];

	for (stc = sscanf_test_cases; *stc->input; stc++) {
		strcpy(input + 1, stc->input);
		SSCANF_TEST(input + 1, "%o", stc->o.ret, stc->o.val, stc->o.len);
		input[0] = '+';
		SSCANF_TEST(input, "%o", stc->o.ret, stc->o.val, stc->o.len ? stc->o.len + 1 : 0);
		input[0] = '-';
		SSCANF_TEST(input, "%o", stc->o.ret, -stc->o.val, stc->o.len ? stc->o.len + 1 : 0);
	}
}

ATF_TC_WITHOUT_HEAD(sscanf_d);
ATF_TC_BODY(sscanf_d, tc)
{
	const struct sscanf_test_case *stc;
	char input[16];

	for (stc = sscanf_test_cases; *stc->input; stc++) {
		strcpy(input + 1, stc->input);
		SSCANF_TEST(input + 1, "%d", stc->d.ret, stc->d.val, stc->d.len);
		input[0] = '+';
		SSCANF_TEST(input, "%d", stc->d.ret, stc->d.val, stc->d.len ? stc->d.len + 1 : 0);
		input[0] = '-';
		SSCANF_TEST(input, "%d", stc->d.ret, -stc->d.val, stc->d.len ? stc->d.len + 1 : 0);
	}
}

ATF_TC_WITHOUT_HEAD(sscanf_x);
ATF_TC_BODY(sscanf_x, tc)
{
	const struct sscanf_test_case *stc;
	char input[16];

	for (stc = sscanf_test_cases; *stc->input; stc++) {
		strcpy(input + 1, stc->input);
		SSCANF_TEST(input + 1, "%x", stc->x.ret, stc->x.val, stc->x.len);
		input[0] = '+';
		SSCANF_TEST(input, "%x", stc->x.ret, stc->x.val, stc->x.len ? stc->x.len + 1 : 0);
		input[0] = '-';
		SSCANF_TEST(input, "%x", stc->x.ret, -stc->x.val, stc->x.len ? stc->x.len + 1 : 0);
	}
}

ATF_TC_WITHOUT_HEAD(sscanf_i);
ATF_TC_BODY(sscanf_i, tc)
{
	const struct sscanf_test_case *stc;
	char input[16];

	for (stc = sscanf_test_cases; *stc->input; stc++) {
		strcpy(input + 1, stc->input);
		SSCANF_TEST(input + 1, "%i", stc->i.ret, stc->i.val, stc->i.len);
		input[0] = '+';
		SSCANF_TEST(input, "%i", stc->i.ret, stc->i.val, stc->i.len ? stc->i.len + 1 : 0);
		input[0] = '-';
		SSCANF_TEST(input, "%i", stc->i.ret, -stc->i.val, stc->i.len ? stc->i.len + 1 : 0);
	}
}

ATF_TC_WITHOUT_HEAD(sscanf_wN);
ATF_TC_BODY(sscanf_wN, tc)
{
	const char x00[] = "0x00";
	const char x7f[] = "0x7fffffffffffffff";
	const char xff[] = "0xffffffffffffffff";

#define SSCANF_WN_TEST(N, imin, umax)					\
	do {								\
		int##N##_t i;						\
		uint##N##_t u;						\
		ATF_CHECK_EQ(1, sscanf(x00, "%w" #N "i", &i));		\
		ATF_CHECK_EQ(0, i);					\
		ATF_CHECK_EQ(1, sscanf(x7f, "%w" #N "i", &i));		\
		ATF_CHECK_EQ(imin, i);					\
		ATF_CHECK_EQ(1, sscanf(x00, "%w" #N "x", &u));		\
		ATF_CHECK_EQ(0, u);					\
		ATF_CHECK_EQ(1, sscanf(xff, "%w" #N "x", &u));		\
		ATF_CHECK_EQ(umax, u);					\
	} while (0)
	SSCANF_WN_TEST(8, -1, UCHAR_MAX);
	SSCANF_WN_TEST(16, -1, USHRT_MAX);
	SSCANF_WN_TEST(32, -1, UINT_MAX);
	SSCANF_WN_TEST(64, LLONG_MAX, ULLONG_MAX);
#undef SSCANF_WN_TEST

	ATF_CHECK_EQ(0, sscanf(x00, "%wi", (int *)NULL));
	ATF_CHECK_EQ(0, sscanf(x00, "%w1i", (int *)NULL));
	ATF_CHECK_EQ(0, sscanf(x00, "%w128i", (int *)NULL));
}

ATF_TC_WITHOUT_HEAD(sscanf_wfN);
ATF_TC_BODY(sscanf_wfN, tc)
{
	const char x00[] = "0x00";
	const char x7f[] = "0x7fffffffffffffff";
	const char xff[] = "0xffffffffffffffff";

#define SSCANF_WFN_TEST(N, imin, umax)					\
	do {								\
		int_fast##N##_t i;					\
		uint_fast##N##_t u;					\
		ATF_CHECK_EQ(1, sscanf(x00, "%wf" #N "i", &i));		\
		ATF_CHECK_EQ(0, i);					\
		ATF_CHECK_EQ(1, sscanf(x7f, "%wf" #N "i", &i));		\
		ATF_CHECK_EQ(imin, i);					\
		ATF_CHECK_EQ(1, sscanf(x00, "%wf" #N "x", &u));		\
		ATF_CHECK_EQ(0, u);					\
		ATF_CHECK_EQ(1, sscanf(xff, "%wf" #N "x", &u));		\
		ATF_CHECK_EQ(umax, u);					\
	} while (0)
	SSCANF_WFN_TEST(8, -1, UINT_MAX);
	SSCANF_WFN_TEST(16, -1, UINT_MAX);
	SSCANF_WFN_TEST(32, -1, UINT_MAX);
	SSCANF_WFN_TEST(64, LLONG_MAX, ULLONG_MAX);
#undef SSCANF_WFN_TEST

	ATF_CHECK_EQ(0, sscanf(x00, "%wfi", (int *)NULL));
	ATF_CHECK_EQ(0, sscanf(x00, "%wf1i", (int *)NULL));
	ATF_CHECK_EQ(0, sscanf(x00, "%wf128i", (int *)NULL));
}

/*
 * Test termination cases: non-numeric character, fixed width, EOF
 */
ATF_TC_WITHOUT_HEAD(sscanf_termination);
ATF_TC_BODY(sscanf_termination, tc)
{
	int a = 0, b = 0, c = 0;
	char d = 0;

	ATF_CHECK_EQ(4, sscanf("3.1415", "%d%c%2d%d", &a, &d, &b, &c));
	ATF_CHECK_EQ(3, a);
	ATF_CHECK_EQ(14, b);
	ATF_CHECK_EQ(15, c);
	ATF_CHECK_EQ('.', d);
}

ATF_TP_ADD_TCS(tp)
{
	setlocale(LC_NUMERIC, "en_US.UTF-8");
	ATF_TP_ADD_TC(tp, sscanf_b);
	ATF_TP_ADD_TC(tp, sscanf_o);
	ATF_TP_ADD_TC(tp, sscanf_d);
	ATF_TP_ADD_TC(tp, sscanf_x);
	ATF_TP_ADD_TC(tp, sscanf_i);
	ATF_TP_ADD_TC(tp, sscanf_wN);
	ATF_TP_ADD_TC(tp, sscanf_wfN);
	ATF_TP_ADD_TC(tp, sscanf_termination);
	return (atf_no_error());
}
