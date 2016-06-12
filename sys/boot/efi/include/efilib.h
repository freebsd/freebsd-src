/*-
 * Copyright (c) 2000 Doug Rabson
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <stand.h>

extern EFI_HANDLE		IH;
extern EFI_SYSTEM_TABLE		*ST;
extern EFI_BOOT_SERVICES	*BS;
extern EFI_RUNTIME_SERVICES	*RS;

extern struct devsw efipart_dev;
extern struct devsw efinet_dev;
extern struct netif_driver efinetif;

void *efi_get_table(EFI_GUID *tbl);

int efi_register_handles(struct devsw *, EFI_HANDLE *, EFI_HANDLE *, int);
EFI_HANDLE efi_find_handle(struct devsw *, int);
int efi_handle_lookup(EFI_HANDLE, struct devsw **, int *,  uint64_t *);
int efi_handle_update_dev(EFI_HANDLE, struct devsw *, int, uint64_t);

EFI_DEVICE_PATH *efi_lookup_image_devpath(EFI_HANDLE);
EFI_DEVICE_PATH *efi_lookup_devpath(EFI_HANDLE);
EFI_HANDLE efi_devpath_handle(EFI_DEVICE_PATH *);
EFI_DEVICE_PATH *efi_devpath_last_node(EFI_DEVICE_PATH *);
EFI_DEVICE_PATH *efi_devpath_trim(EFI_DEVICE_PATH *);
CHAR16 *efi_devpath_name(EFI_DEVICE_PATH *);
void efi_free_devpath_name(CHAR16 *);

int efi_status_to_errno(EFI_STATUS);

void efi_time_init(void);
void efi_time_fini(void);

EFI_STATUS main(int argc, CHAR16 *argv[]);
void exit(EFI_STATUS status);
void delay(int usecs);
