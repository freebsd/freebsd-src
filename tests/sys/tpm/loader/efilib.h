/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

#ifndef TPM_TEST_EFILIB_H
#define TPM_TEST_EFILIB_H

extern EFI_BOOT_SERVICES *BS;
void *efi_get_table(EFI_GUID *);

struct efi_tpm_eventlog;
struct efi_tpm_eventlog *efi_tpm_log_prepare(struct preloaded_file *);
void efi_tpm_log_free(struct efi_tpm_eventlog *);
void efi_tpm_log_finish(struct efi_tpm_eventlog *, EFI_MEMORY_DESCRIPTOR *,
    size_t, size_t);

#endif
