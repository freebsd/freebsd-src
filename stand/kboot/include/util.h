/*-
 * Copyright (c) 2022, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

bool file2str(const char *fn, char *buffer, size_t buflen);
bool file2u64(const char *fn, uint64_t *val);
