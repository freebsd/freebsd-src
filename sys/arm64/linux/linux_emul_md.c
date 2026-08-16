/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/imgact.h>
#include <sys/proc.h>

#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_mmap.h>

/* No machine-dependent emuldata state yet. */

void
linux_pemuldata_init_md(struct thread *td, struct linux_pemuldata *pem)
{
}

void
linux_pemuldata_exec_md(struct linux_pemuldata *pem)
{
}

/*
 * Protection key back ends: behave as Linux does on hardware without
 * protection keys.  pkey_alloc() reports no free keys and only the
 * default key semantics remain.
 */

int
linux_pkey_alloc_machdep(struct thread *td, uint64_t init_val)
{

	return (ENOSPC);
}

int
linux_pkey_free_machdep(struct thread *td, int pkey)
{

	return (EINVAL);
}

int
linux_pkey_mprotect_machdep(struct thread *td, uintptr_t addr, size_t len,
    int prot, int pkey)
{

	return (EINVAL);
}
