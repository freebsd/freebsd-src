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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/cpu.h>
#include <machine/cpu_feat.h>

SYSCTL_NODE(_hw, OID_AUTO, feat, CTLFLAG_RD, 0, "CPU features/errata");

/* TODO: Make this a list if we ever grow a callback other than smccc_errata */
static cpu_feat_errata_check_fn cpu_feat_check_cb = NULL;

void
enable_cpu_feat(uint32_t stage)
{
	char tunable[32];
	struct cpu_feat **featp, *feat;
	uint32_t midr;
	u_int errata_count, *errata_list;
	cpu_feat_errata errata_status;
	cpu_feat_en check_status;
	bool val;

	MPASS((stage & ~CPU_FEAT_STAGE_MASK) == 0);

	midr = get_midr();
	SET_FOREACH(featp, cpu_feat_set) {
		feat = *featp;

		/* Read any tunable the user may have set */
		if (stage == CPU_FEAT_EARLY_BOOT && PCPU_GET(cpuid) == 0) {
			snprintf(tunable, sizeof(tunable), "hw.feat.%s",
			    feat->feat_name);
			if (TUNABLE_BOOL_FETCH(tunable, &val)) {
				if (val) {
					feat->feat_flags |=
					    CPU_FEAT_USER_ENABLED;
				} else {
					feat->feat_flags |=
					    CPU_FEAT_USER_DISABLED;
				}
			}
		}

		/* Run the enablement code at the correct stage of boot */
		if ((feat->feat_flags & CPU_FEAT_STAGE_MASK) != stage)
			continue;

		/* If the feature is system wide run on a single CPU */
		if ((feat->feat_flags & CPU_FEAT_SCOPE_MASK)==CPU_FEAT_SYSTEM &&
		    PCPU_GET(cpuid) != 0)
			continue;

		if (feat->feat_check != NULL) {
			check_status = feat->feat_check(feat, midr);
		} else {
			check_status = FEAT_DEFAULT_ENABLE;
		}
		/* Ignore features that are not present */
		if (check_status == FEAT_ALWAYS_DISABLE)
			goto next;

		/* The user disabled the feature */
		if ((feat->feat_flags & CPU_FEAT_USER_DISABLED) != 0)
			goto next;

		/*
		 * The feature was disabled by default and the user
		 * didn't enable it then skip.
		 */
		if (check_status == FEAT_DEFAULT_DISABLE &&
		    (feat->feat_flags & CPU_FEAT_USER_ENABLED) == 0)
			goto next;

		/*
		 * Check if the feature has any errata that may need a
		 * workaround applied (or it is to install the workaround for
		 * known errata.
		 */
		errata_status = ERRATA_NONE;
		errata_list = NULL;
		errata_count = 0;
		if (feat->feat_has_errata != NULL) {
			if (feat->feat_has_errata(feat, midr, &errata_list,
			    &errata_count)) {
				/* Assume we are affected */
				errata_status = ERRATA_AFFECTED;
			}
		}

		if (errata_status == ERRATA_AFFECTED &&
		    cpu_feat_check_cb != NULL) {
			for (int i = 0; i < errata_count; i++) {
				cpu_feat_errata new_status;

				/* Check if affected by this erratum */
				new_status = cpu_feat_check_cb(feat,
				    errata_list[i]);
				if (new_status != ERRATA_UNKNOWN) {
					errata_status = new_status;
					errata_list = &errata_list[i];
					errata_count = 1;
					break;
				}
			}
		}

		/* Shouldn't be possible */
		MPASS(errata_status != ERRATA_UNKNOWN);

		if (feat->feat_enable(feat, errata_status, errata_list,
		    errata_count))
			feat->feat_enabled = true;

next:
		if (!feat->feat_enabled && feat->feat_disabled != NULL)
			feat->feat_disabled(feat);
	}
}

static void
enable_cpu_feat_after_dev(void *dummy __unused)
{
	MPASS(PCPU_GET(cpuid) == 0);
	enable_cpu_feat(CPU_FEAT_AFTER_DEV);
}
SYSINIT(enable_cpu_feat_after_dev, SI_SUB_CONFIGURE, SI_ORDER_MIDDLE,
    enable_cpu_feat_after_dev, NULL);

void
cpu_feat_register_errata_check(cpu_feat_errata_check_fn cb)
{
	MPASS(cpu_feat_check_cb == NULL);
	cpu_feat_check_cb = cb;
}
