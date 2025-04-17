/*-
 * Copyright (c) 2022, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

/* dfk.c: */
bool data_from_kernel(const char *sym, void *buf, size_t len);

/* util.c: */
bool file2str(const char *fn, char *buffer, size_t buflen);
bool file2u32(const char *fn, uint32_t *val);
bool file2u64(const char *fn, uint64_t *val);
