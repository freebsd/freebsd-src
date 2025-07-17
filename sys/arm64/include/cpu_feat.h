/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Arm Ltd
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
 */

#ifndef _MACHINE_CPU_FEAT_H_
#define	_MACHINE_CPU_FEAT_H_

#include <sys/linker_set.h>

typedef enum {
	ERRATA_UNKNOWN,		/* Unknown erratum */
	ERRATA_NONE,		/* No errata for this feature on this system. */
	ERRATA_AFFECTED,	/* There is errata on this system. */
	ERRATA_FW_MITIGAION,	/* There is errata, and a firmware */
				/* mitigation. The mitigation may need a */
				/* kernel component. */
} cpu_feat_errata;

#define	CPU_FEAT_STAGE_MASK	0x00000001
#define	CPU_FEAT_EARLY_BOOT	0x00000000
#define	CPU_FEAT_AFTER_DEV	0x00000001

#define	CPU_FEAT_SCOPE_MASK	0x00000010
#define	CPU_FEAT_PER_CPU	0x00000000
#define	CPU_FEAT_SYSTEM		0x00000010

struct cpu_feat;

typedef bool (cpu_feat_check)(const struct cpu_feat *, u_int);
typedef bool (cpu_feat_has_errata)(const struct cpu_feat *, u_int,
    u_int **, u_int *);
typedef void (cpu_feat_enable)(const struct cpu_feat *, cpu_feat_errata,
    u_int *, u_int);

struct cpu_feat {
	const char		*feat_name;
	cpu_feat_check		*feat_check;
	cpu_feat_has_errata	*feat_has_errata;
	cpu_feat_enable		*feat_enable;
	uint32_t		 feat_flags;
};
SET_DECLARE(cpu_feat_set, struct cpu_feat);

/*
 * Allow drivers to mark an erratum as worked around, e.g. the Errata
 * Management ABI may know the workaround isn't needed on a given system.
 */
typedef cpu_feat_errata (*cpu_feat_errata_check_fn)(const struct cpu_feat *,
    u_int);
void cpu_feat_register_errata_check(cpu_feat_errata_check_fn);

void	enable_cpu_feat(uint32_t);

/* Check if an erratum is in the list of errata */
static inline bool
cpu_feat_has_erratum(u_int *errata_list, u_int errata_count, u_int erratum)
{
	for (u_int i = 0; i < errata_count; i++)
		if (errata_list[0] == erratum)
			return (true);

	return (false);
}

#endif /* _MACHINE_CPU_FEAT_H_ */
