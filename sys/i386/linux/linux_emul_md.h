/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 */

#ifndef _I386_LINUX_EMUL_MD_H_
#define	_I386_LINUX_EMUL_MD_H_

/*
 * Machine-dependent part of the Linux process emuldata, embedded in
 * struct linux_pemuldata as pem_md.
 */
struct linux_pemuldata_md {
	int	md_dummy;		/* no machine-dependent state yet */
};

#endif	/* !_I386_LINUX_EMUL_MD_H_ */
