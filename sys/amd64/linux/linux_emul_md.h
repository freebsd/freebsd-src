/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 */

#ifndef _AMD64_LINUX_EMUL_MD_H_
#define	_AMD64_LINUX_EMUL_MD_H_

/*
 * Machine-dependent part of the Linux process emuldata, embedded in
 * struct linux_pemuldata as pem_md.
 */
struct linux_pemuldata_md {
	uint32_t	md_pkey_allocation_map;	/* x86 protection keys */
};

/*
 * Initial protection key allocation map: key 0 is the default key,
 * implicitly allocated on Linux (mm_pkey_allocation_map is initialized
 * to 0x1).  Inherited on fork, reset on exec.
 */
#define	LINUX_PKEY_INITIAL_MAP	0x1

/*
 * Initial PKRU at exec: access disabled for keys 1..15, key 0 open;
 * the Linux init_pkru default.
 */
#define	LINUX_PKRU_INIT		0x55555554

struct thread;

void	linux_pkru_exec_init(struct thread *);

#endif	/* !_AMD64_LINUX_EMUL_MD_H_ */
