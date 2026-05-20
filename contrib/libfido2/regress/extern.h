/*
 * Copyright (c) 2024 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef REGRESS_EXTERN_H
#define REGRESS_EXTERN_H

#include <fido.h>

void setup_dummy_io(fido_dev_t *);
void set_read_interval(long);
uint8_t *wiredata_setup(const uint8_t *, size_t);
void wiredata_clear(uint8_t **);

#endif
