/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

extern EFI_BOOT_SERVICES *BS;

void efi_tpm_measure_boot(void);
void efi_tpm_measure_file(struct preloaded_file *, vm_offset_t, size_t);
