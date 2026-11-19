/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

#ifndef TPM_TEST_STAND_H
#define TPM_TEST_STAND_H

#include <sys/param.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *test_malloc(size_t);
void *test_calloc(size_t, size_t);
void test_free(void *);
int test_printf(const char *, ...);

#ifndef TPM_TEST_IMPLEMENTATION
#define malloc test_malloc
#define calloc test_calloc
#define free test_free
#define printf test_printf
#endif

#endif
