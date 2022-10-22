/*-
 * Copyright (c) 2022, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef KBOOT_H
#define KBOOT_H

#define DEVT_HOSTDISK 1234

void do_init(void);
uint64_t kboot_get_phys_load_segment(void);
uint8_t kboot_get_kernel_machine_bits(void);

#endif /* KBOOT_H */
