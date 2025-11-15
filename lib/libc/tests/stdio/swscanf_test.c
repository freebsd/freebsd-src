/*-
 * Copyright (c) 2023 Dag-Erling Smørgrav
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limits.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

#include <atf-c.h>

#define L(s) L ## s

static const struct swscanf_test_case {
	wchar_t input[8];
	struct {
		int ret, val, len;
	} b, o, d, x, i;
} swscanf_test_cases[] = {
//	input		binary		octal		decimal		hexadecimal	automatic
	// all digits
	{ L"0",		{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 }, },
	{ L"1",		{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 }, },
	{ L"2",		{ 0,   0, 0 },	{ 1,   2, 1 },	{ 1,   2, 1 },	{ 1,   2, 1 },	{ 1,   2, 1 }, },
	{ L"3",		{ 0,   0, 0 },	{ 1,   3, 1 },	{ 1,   3, 1 },	{ 1,   3, 1 },	{ 1,   3, 1 }, },
	{ L"4",		{ 0,   0, 0 },	{ 1,   4, 1 },	{ 1,   4, 1 },	{ 1,   4, 1 },	{ 1,   4, 1 }, },
	{ L"5",		{ 0,   0, 0 },	{ 1,   5, 1 },	{ 1,   5, 1 },	{ 1,   5, 1 },	{ 1,   5, 1 }, },
	{ L"6",		{ 0,   0, 0 },	{ 1,   6, 1 },	{ 1,   6, 1 },	{ 1,   6, 1 },	{ 1,   6, 1 }, },
	{ L"7",		{ 0,   0, 0 },	{ 1,   7, 1 },	{ 1,   7, 1 },	{ 1,   7, 1 },	{ 1,   7, 1 }, },
	{ L"8",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,   8, 1 },	{ 1,   8, 1 },	{ 1,   8, 1 }, },
	{ L"9",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,   9, 1 },	{ 1,   9, 1 },	{ 1,   9, 1 }, },
	{ L"A",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  10, 1 },	{ 0,   0, 0 }, },
	{ L"B",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  11, 1 },	{ 0,   0, 0 }, },
	{ L"C",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  12, 1 },	{ 0,   0, 0 }, },
	{ L"D",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  13, 1 },	{ 0,   0, 0 }, },
	{ L"E",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  14, 1 },	{ 0,   0, 0 }, },
	{ L"F",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  15, 1 },	{ 0,   0, 0 }, },
	{ L"X",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 }, },
	{ L"a",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  10, 1 },	{ 0,   0, 0 }, },
	{ L"b",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  11, 1 },	{ 0,   0, 0 }, },
	{ L"c",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  12, 1 },	{ 0,   0, 0 }, },
	{ L"d",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  13, 1 },	{ 0,   0, 0 }, },
	{ L"e",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  14, 1 },	{ 0,   0, 0 }, },
	{ L"f",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 1,  15, 1 },	{ 0,   0, 0 }, },
	{ L"x",		{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 },	{ 0,   0, 0 }, },
	// all digits with leading zero
	{ L"00",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 }, },
	{ L"01",	{ 1,   1, 2 },	{ 1,   1, 2 },	{ 1,   1, 2 },	{ 1,   1, 2 },	{ 1,   1, 2 }, },
	{ L"02",	{ 1,   0, 1 },	{ 1,   2, 2 },	{ 1,   2, 2 },	{ 1,   2, 2 },	{ 1,   2, 2 }, },
	{ L"03",	{ 1,   0, 1 },	{ 1,   3, 2 },	{ 1,   3, 2 },	{ 1,   3, 2 },	{ 1,   3, 2 }, },
	{ L"04",	{ 1,   0, 1 },	{ 1,   4, 2 },	{ 1,   4, 2 },	{ 1,   4, 2 },	{ 1,   4, 2 }, },
	{ L"05",	{ 1,   0, 1 },	{ 1,   5, 2 },	{ 1,   5, 2 },	{ 1,   5, 2 },	{ 1,   5, 2 }, },
	{ L"06",	{ 1,   0, 1 },	{ 1,   6, 2 },	{ 1,   6, 2 },	{ 1,   6, 2 },	{ 1,   6, 2 }, },
	{ L"07",	{ 1,   0, 1 },	{ 1,   7, 2 },	{ 1,   7, 2 },	{ 1,   7, 2 },	{ 1,   7, 2 }, },
	{ L"08",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   8, 2 },	{ 1,   8, 2 },	{ 1,   0, 1 }, },
	{ L"09",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   9, 2 },	{ 1,   9, 2 },	{ 1,   0, 1 }, },
	{ L"0A",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  10, 2 },	{ 1,   0, 1 }, },
	{ L"0B",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  11, 2 },	{ 1,   0, 1 }, },
	{ L"0C",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  12, 2 },	{ 1,   0, 1 }, },
	{ L"0D",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  13, 2 },	{ 1,   0, 1 }, },
	{ L"0E",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  14, 2 },	{ 1,   0, 1 }, },
	{ L"0F",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  15, 2 },	{ 1,   0, 1 }, },
	{ L"0X",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 }, },
	{ L"0a",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  10, 2 },	{ 1,   0, 1 }, },
	{ L"0b",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  11, 2 },	{ 1,   0, 1 }, },
	{ L"0c",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  12, 2 },	{ 1,   0, 1 }, },
	{ L"0d",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  13, 2 },	{ 1,   0, 1 }, },
	{ L"0e",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  14, 2 },	{ 1,   0, 1 }, },
	{ L"0f",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  15, 2 },	{ 1,   0, 1 }, },
	{ L"0x",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 }, },
	// all digits with two leading zeroes
	{ L"000",	{ 1,   0, 3 },	{ 1,   0, 3 },	{ 1,   0, 3 },	{ 1,   0, 3 },	{ 1,   0, 3 }, },
	{ L"001",	{ 1,   1, 3 },	{ 1,   1, 3 },	{ 1,   1, 3 },	{ 1,   1, 3 },	{ 1,   1, 3 }, },
	{ L"002",	{ 1,   0, 2 },	{ 1,   2, 3 },	{ 1,   2, 3 },	{ 1,   2, 3 },	{ 1,   2, 3 }, },
	{ L"003",	{ 1,   0, 2 },	{ 1,   3, 3 },	{ 1,   3, 3 },	{ 1,   3, 3 },	{ 1,   3, 3 }, },
	{ L"004",	{ 1,   0, 2 },	{ 1,   4, 3 },	{ 1,   4, 3 },	{ 1,   4, 3 },	{ 1,   4, 3 }, },
	{ L"005",	{ 1,   0, 2 },	{ 1,   5, 3 },	{ 1,   5, 3 },	{ 1,   5, 3 },	{ 1,   5, 3 }, },
	{ L"006",	{ 1,   0, 2 },	{ 1,   6, 3 },	{ 1,   6, 3 },	{ 1,   6, 3 },	{ 1,   6, 3 }, },
	{ L"007",	{ 1,   0, 2 },	{ 1,   7, 3 },	{ 1,   7, 3 },	{ 1,   7, 3 },	{ 1,   7, 3 }, },
	{ L"008",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   8, 3 },	{ 1,   8, 3 },	{ 1,   0, 2 }, },
	{ L"009",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   9, 3 },	{ 1,   9, 3 },	{ 1,   0, 2 }, },
	{ L"00A",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,  10, 3 },	{ 1,   0, 2 }, },
	{ L"00B",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,  11, 3 },	{ 1,   0, 2 }, },
	{ L"00C",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,  12, 3 },	{ 1,   0, 2 }, },
	{ L"00D",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,  13, 3 },	{ 1,   0, 2 }, },
	{ L"00E",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,  14, 3 },	{ 1,   0, 2 }, },
	{ L"00F",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,  15, 3 },	{ 1,   0, 2 }, },
	{ L"00X",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 }, },
	{ L"00a",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,  10, 3 },	{ 1,   0, 2 }, },
	{ L"00b",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,  11, 3 },	{ 1,   0, 2 }, },
	{ L"00c",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,  12, 3 },	{ 1,   0, 2 }, },
	{ L"00d",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,  13, 3 },	{ 1,   0, 2 }, },
	{ L"00e",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,  14, 3 },	{ 1,   0, 2 }, },
	{ L"00f",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,  15, 3 },	{ 1,   0, 2 }, },
	{ L"00x",	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 },	{ 1,   0, 2 }, },
	// all digits with leading one
	{ L"10",	{ 1,   2, 2 },	{ 1,   8, 2 },	{ 1,  10, 2 },	{ 1,  16, 2 },	{ 1,  10, 2 }, },
	{ L"11",	{ 1,   3, 2 },	{ 1,   9, 2 },	{ 1,  11, 2 },	{ 1,  17, 2 },	{ 1,  11, 2 }, },
	{ L"12",	{ 1,   1, 1 },	{ 1,  10, 2 },	{ 1,  12, 2 },	{ 1,  18, 2 },	{ 1,  12, 2 }, },
	{ L"13",	{ 1,   1, 1 },	{ 1,  11, 2 },	{ 1,  13, 2 },	{ 1,  19, 2 },	{ 1,  13, 2 }, },
	{ L"14",	{ 1,   1, 1 },	{ 1,  12, 2 },	{ 1,  14, 2 },	{ 1,  20, 2 },	{ 1,  14, 2 }, },
	{ L"15",	{ 1,   1, 1 },	{ 1,  13, 2 },	{ 1,  15, 2 },	{ 1,  21, 2 },	{ 1,  15, 2 }, },
	{ L"16",	{ 1,   1, 1 },	{ 1,  14, 2 },	{ 1,  16, 2 },	{ 1,  22, 2 },	{ 1,  16, 2 }, },
	{ L"17",	{ 1,   1, 1 },	{ 1,  15, 2 },	{ 1,  17, 2 },	{ 1,  23, 2 },	{ 1,  17, 2 }, },
	{ L"18",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  18, 2 },	{ 1,  24, 2 },	{ 1,  18, 2 }, },
	{ L"19",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  19, 2 },	{ 1,  25, 2 },	{ 1,  19, 2 }, },
	{ L"1A",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  26, 2 },	{ 1,   1, 1 }, },
	{ L"1B",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  27, 2 },	{ 1,   1, 1 }, },
	{ L"1C",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  28, 2 },	{ 1,   1, 1 }, },
	{ L"1D",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  29, 2 },	{ 1,   1, 1 }, },
	{ L"1E",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  30, 2 },	{ 1,   1, 1 }, },
	{ L"1F",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  31, 2 },	{ 1,   1, 1 }, },
	{ L"1X",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 }, },
	{ L"1a",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  26, 2 },	{ 1,   1, 1 }, },
	{ L"1b",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  27, 2 },	{ 1,   1, 1 }, },
	{ L"1c",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  28, 2 },	{ 1,   1, 1 }, },
	{ L"1d",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  29, 2 },	{ 1,   1, 1 }, },
	{ L"1e",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  30, 2 },	{ 1,   1, 1 }, },
	{ L"1f",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,  31, 2 },	{ 1,   1, 1 }, },
	{ L"1x",	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 },	{ 1,   1, 1 }, },
	// all digits with leading binary prefix
	{ L"0b0",	{ 1,   0, 3 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 176, 3 },	{ 1,   0, 3 }, },
	{ L"0b1",	{ 1,   1, 3 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 177, 3 },	{ 1,   1, 3 }, },
	{ L"0b2",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 178, 3 },	{ 1,   0, 1 }, },
	{ L"0b3",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 179, 3 },	{ 1,   0, 1 }, },
	{ L"0b4",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 180, 3 },	{ 1,   0, 1 }, },
	{ L"0b5",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 181, 3 },	{ 1,   0, 1 }, },
	{ L"0b6",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 182, 3 },	{ 1,   0, 1 }, },
	{ L"0b7",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 183, 3 },	{ 1,   0, 1 }, },
	{ L"0b8",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 184, 3 },	{ 1,   0, 1 }, },
	{ L"0b9",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 185, 3 },	{ 1,   0, 1 }, },
	{ L"0bA",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 186, 3 },	{ 1,   0, 1 }, },
	{ L"0bB",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 187, 3 },	{ 1,   0, 1 }, },
	{ L"0bC",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 188, 3 },	{ 1,   0, 1 }, },
	{ L"0bD",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 189, 3 },	{ 1,   0, 1 }, },
	{ L"0bE",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 190, 3 },	{ 1,   0, 1 }, },
	{ L"0bF",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 191, 3 },	{ 1,   0, 1 }, },
	{ L"0bX",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  11, 2 },	{ 1,   0, 1 }, },
	{ L"0ba",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 186, 3 },	{ 1,   0, 1 }, },
	{ L"0bb",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 187, 3 },	{ 1,   0, 1 }, },
	{ L"0bc",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 188, 3 },	{ 1,   0, 1 }, },
	{ L"0bd",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 189, 3 },	{ 1,   0, 1 }, },
	{ L"0be",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 190, 3 },	{ 1,   0, 1 }, },
	{ L"0bf",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1, 191, 3 },	{ 1,   0, 1 }, },
	{ L"0bx",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  11, 2 },	{ 1,   0, 1 }, },
	// all digits with leading hexadecimal prefix
	{ L"0x0",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 3 },	{ 1,   0, 3 }, },
	{ L"0x1",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   1, 3 },	{ 1,   1, 3 }, },
	{ L"0x2",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   2, 3 },	{ 1,   2, 3 }, },
	{ L"0x3",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   3, 3 },	{ 1,   3, 3 }, },
	{ L"0x4",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   4, 3 },	{ 1,   4, 3 }, },
	{ L"0x5",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   5, 3 },	{ 1,   5, 3 }, },
	{ L"0x6",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   6, 3 },	{ 1,   6, 3 }, },
	{ L"0x7",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   7, 3 },	{ 1,   7, 3 }, },
	{ L"0x8",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   8, 3 },	{ 1,   8, 3 }, },
	{ L"0x9",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   9, 3 },	{ 1,   9, 3 }, },
	{ L"0xA",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  10, 3 },	{ 1,  10, 3 }, },
	{ L"0xB",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  11, 3 },	{ 1,  11, 3 }, },
	{ L"0xC",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  12, 3 },	{ 1,  12, 3 }, },
	{ L"0xD",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  13, 3 },	{ 1,  13, 3 }, },
	{ L"0xE",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  14, 3 },	{ 1,  14, 3 }, },
	{ L"0xF",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  15, 3 },	{ 1,  15, 3 }, },
	{ L"0xX",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 }, },
	{ L"0xa",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  10, 3 },	{ 1,  10, 3 }, },
	{ L"0xb",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  11, 3 },	{ 1,  11, 3 }, },
	{ L"0xc",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  12, 3 },	{ 1,  12, 3 }, },
	{ L"0xd",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  13, 3 },	{ 1,  13, 3 }, },
	{ L"0xe",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  14, 3 },	{ 1,  14, 3 }, },
	{ L"0xf",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,  15, 3 },	{ 1,  15, 3 }, },
	{ L"0xX",	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 },	{ 1,   0, 1 }, },
	// terminator
	{ L"" }
};

#define SWSCANF_TEST(string, format, expret, expval, explen)		\
	do {								\
		int ret = 0, val = 0, len = 0;				\
		ret = swscanf(string, format L"%n", &val, &len);	\
		ATF_CHECK_EQ(expret, ret);				\
		if (expret && ret) {					\
			ATF_CHECK_EQ(expval, val);			\
			ATF_CHECK_EQ(explen, len);			\
		}							\
	} while (0)

ATF_TC_WITHOUT_HEAD(swscanf_b);
ATF_TC_BODY(swscanf_b, tc)
{
	const struct swscanf_test_case *stc;
	wchar_t input[16];

	for (stc = swscanf_test_cases; *stc->input; stc++) {
		wcscpy(input + 1, stc->input);
		SWSCANF_TEST(input + 1, L"%b", stc->b.ret, stc->b.val, stc->b.len);
		input[0] = L'+';
		SWSCANF_TEST(input, L"%b", stc->b.ret, stc->b.val, stc->b.len ? stc->b.len + 1 : 0);
		input[0] = L'-';
		SWSCANF_TEST(input, L"%b", stc->b.ret, -stc->b.val, stc->b.len ? stc->b.len + 1 : 0);
	}
}

ATF_TC_WITHOUT_HEAD(swscanf_o);
ATF_TC_BODY(swscanf_o, tc)
{
	const struct swscanf_test_case *stc;
	wchar_t input[16];

	for (stc = swscanf_test_cases; *stc->input; stc++) {
		wcscpy(input + 1, stc->input);
		SWSCANF_TEST(input + 1, L"%o", stc->o.ret, stc->o.val, stc->o.len);
		input[0] = L'+';
		SWSCANF_TEST(input, L"%o", stc->o.ret, stc->o.val, stc->o.len ? stc->o.len + 1 : 0);
		input[0] = L'-';
		SWSCANF_TEST(input, L"%o", stc->o.ret, -stc->o.val, stc->o.len ? stc->o.len + 1 : 0);
	}
}

ATF_TC_WITHOUT_HEAD(swscanf_d);
ATF_TC_BODY(swscanf_d, tc)
{
	const struct swscanf_test_case *stc;
	wchar_t input[16];

	for (stc = swscanf_test_cases; *stc->input; stc++) {
		wcscpy(input + 1, stc->input);
		SWSCANF_TEST(input + 1, L"%d", stc->d.ret, stc->d.val, stc->d.len);
		input[0] = L'+';
		SWSCANF_TEST(input, L"%d", stc->d.ret, stc->d.val, stc->d.len ? stc->d.len + 1 : 0);
		input[0] = L'-';
		SWSCANF_TEST(input, L"%d", stc->d.ret, -stc->d.val, stc->d.len ? stc->d.len + 1 : 0);
	}
}

ATF_TC_WITHOUT_HEAD(swscanf_x);
ATF_TC_BODY(swscanf_x, tc)
{
	const struct swscanf_test_case *stc;
	wchar_t input[16];

	for (stc = swscanf_test_cases; *stc->input; stc++) {
		wcscpy(input + 1, stc->input);
		SWSCANF_TEST(input + 1, L"%x", stc->x.ret, stc->x.val, stc->x.len);
		input[0] = L'+';
		SWSCANF_TEST(input, L"%x", stc->x.ret, stc->x.val, stc->x.len ? stc->x.len + 1 : 0);
		input[0] = L'-';
		SWSCANF_TEST(input, L"%x", stc->x.ret, -stc->x.val, stc->x.len ? stc->x.len + 1 : 0);
	}
}

ATF_TC_WITHOUT_HEAD(swscanf_i);
ATF_TC_BODY(swscanf_i, tc)
{
	const struct swscanf_test_case *stc;
	wchar_t input[16];

	for (stc = swscanf_test_cases; *stc->input; stc++) {
		wcscpy(input + 1, stc->input);
		SWSCANF_TEST(input + 1, L"%i", stc->i.ret, stc->i.val, stc->i.len);
		input[0] = L'+';
		SWSCANF_TEST(input, L"%i", stc->i.ret, stc->i.val, stc->i.len ? stc->i.len + 1 : 0);
		input[0] = L'-';
		SWSCANF_TEST(input, L"%i", stc->i.ret, -stc->i.val, stc->i.len ? stc->i.len + 1 : 0);
	}
}

ATF_TC_WITHOUT_HEAD(swscanf_wN);
ATF_TC_BODY(swscanf_wN, tc)
{
	const wchar_t x00[] = L"0x00";
	const wchar_t x7f[] = L"0x7fffffffffffffff";
	const wchar_t xff[] = L"0xffffffffffffffff";

#define SWSCANF_WN_TEST(N, imin, umax)					\
	do {								\
		int##N##_t i;						\
		uint##N##_t u;						\
		ATF_CHECK_EQ(1, swscanf(x00, L"%w" L(#N) L"i", &i));	\
		ATF_CHECK_EQ(0, i);					\
		ATF_CHECK_EQ(1, swscanf(x7f, L"%w" L(#N) L"i", &i));	\
		ATF_CHECK_EQ(imin, i);					\
		ATF_CHECK_EQ(1, swscanf(x00, L"%w" L(#N) L"x", &u));	\
		ATF_CHECK_EQ(0, u);					\
		ATF_CHECK_EQ(1, swscanf(xff, L"%w" L(#N) L"x", &u));	\
		ATF_CHECK_EQ(umax, u);					\
	} while (0)
	SWSCANF_WN_TEST(8, -1, UCHAR_MAX);
	SWSCANF_WN_TEST(16, -1, USHRT_MAX);
	SWSCANF_WN_TEST(32, -1, UINT_MAX);
	SWSCANF_WN_TEST(64, LLONG_MAX, ULLONG_MAX);
#undef SWSCANF_WN_TEST

	ATF_CHECK_EQ(0, swscanf(x00, L"%wi", (int *)NULL));
	ATF_CHECK_EQ(0, swscanf(x00, L"%w1i", (int *)NULL));
	ATF_CHECK_EQ(0, swscanf(x00, L"%w128i", (int *)NULL));
}

ATF_TC_WITHOUT_HEAD(swscanf_wfN);
ATF_TC_BODY(swscanf_wfN, tc)
{
	const wchar_t x00[] = L"0x00";
	const wchar_t x7f[] = L"0x7fffffffffffffff";
	const wchar_t xff[] = L"0xffffffffffffffff";

#define SWSCANF_WFN_TEST(N, imin, umax)					\
	do {								\
		int_fast##N##_t i;					\
		uint_fast##N##_t u;					\
		ATF_CHECK_EQ(1, swscanf(x00, L"%wf" L(#N) L"i", &i));	\
		ATF_CHECK_EQ(0, i);					\
		ATF_CHECK_EQ(1, swscanf(x7f, L"%wf" L(#N) L"i", &i));	\
		ATF_CHECK_EQ(imin, i);					\
		ATF_CHECK_EQ(1, swscanf(x00, L"%wf" L(#N) L"x", &u));	\
		ATF_CHECK_EQ(0, u);					\
		ATF_CHECK_EQ(1, swscanf(xff, L"%wf" L(#N) L"x", &u));	\
		ATF_CHECK_EQ(umax, u);					\
	} while (0)
	SWSCANF_WFN_TEST(8, -1, UINT_MAX);
	SWSCANF_WFN_TEST(16, -1, UINT_MAX);
	SWSCANF_WFN_TEST(32, -1, UINT_MAX);
	SWSCANF_WFN_TEST(64, LLONG_MAX, ULLONG_MAX);
#undef SWSCANF_WFN_TEST

	ATF_CHECK_EQ(0, swscanf(x00, L"%wfi", (int *)NULL));
	ATF_CHECK_EQ(0, swscanf(x00, L"%wf1i", (int *)NULL));
	ATF_CHECK_EQ(0, swscanf(x00, L"%wf128i", (int *)NULL));
}

/*
 * Test termination cases: non-numeric character, fixed width, EOF
 */
ATF_TC_WITHOUT_HEAD(swscanf_termination);
ATF_TC_BODY(swscanf_termination, tc)
{
	int a = 0, b = 0, c = 0;
	wchar_t d = 0;

	ATF_CHECK_EQ(4, swscanf(L"3.1415", L"%d%lc%2d%d", &a, &d, &b, &c));
	ATF_CHECK_EQ(3, a);
	ATF_CHECK_EQ(14, b);
	ATF_CHECK_EQ(15, c);
	ATF_CHECK_EQ(L'.', d);
}

ATF_TP_ADD_TCS(tp)
{
	setlocale(LC_NUMERIC, "en_US.UTF-8");
	ATF_TP_ADD_TC(tp, swscanf_b);
	ATF_TP_ADD_TC(tp, swscanf_o);
	ATF_TP_ADD_TC(tp, swscanf_d);
	ATF_TP_ADD_TC(tp, swscanf_x);
	ATF_TP_ADD_TC(tp, swscanf_i);
	ATF_TP_ADD_TC(tp, swscanf_wN);
	ATF_TP_ADD_TC(tp, swscanf_wfN);
	ATF_TP_ADD_TC(tp, swscanf_termination);
	return (atf_no_error());
}
