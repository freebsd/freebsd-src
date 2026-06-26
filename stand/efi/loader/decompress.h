/*
 * Copyright (c) 2026 Netflix, Inc. Written by Warner Losh
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

enum compression { none, zlib, bzip2, zstd };
enum step_return { ok, done, err };

typedef struct decomp_state decomp_state;

decomp_state *decomp_init(uint8_t *buf, size_t buflen, size_t size_hint);
enum step_return decomp_step(decomp_state *dctx, uint8_t *buf, size_t len, size_t offset);
void decomp_fini(decomp_state *dctx, bool flush);
EFI_PHYSICAL_ADDRESS decomp_buffer(decomp_state *dctx);
size_t decomp_buffer_length(decomp_state *dctx);

