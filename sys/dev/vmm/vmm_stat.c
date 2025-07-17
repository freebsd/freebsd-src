/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/vmm.h>

#include <dev/vmm/vmm_stat.h>

/*
 * 'vst_num_elems' is the total number of addressable statistic elements
 * 'vst_num_types' is the number of unique statistic types
 *
 * It is always true that 'vst_num_elems' is greater than or equal to
 * 'vst_num_types'. This is because a stat type may represent more than
 * one element (for e.g. VMM_STAT_ARRAY).
 */
static int vst_num_elems, vst_num_types;
static struct vmm_stat_type *vsttab[MAX_VMM_STAT_ELEMS];

static MALLOC_DEFINE(M_VMM_STAT, "vmm stat", "vmm stat");

#define	vst_size	((size_t)vst_num_elems * sizeof(uint64_t))

void
vmm_stat_register(void *arg)
{
	struct vmm_stat_type *vst = arg;

	/* We require all stats to identify themselves with a description */
	if (vst->desc == NULL)
		return;

	if (vst->pred != NULL && !vst->pred())
		return;

	if (vst_num_elems + vst->nelems >= MAX_VMM_STAT_ELEMS) {
		printf("Cannot accommodate vmm stat type \"%s\"!\n", vst->desc);
		return;
	}

	vst->index = vst_num_elems;
	vst_num_elems += vst->nelems;

	vsttab[vst_num_types++] = vst;
}

int
vmm_stat_copy(struct vcpu *vcpu, int index, int count, int *num_stats,
    uint64_t *buf)
{
	struct vmm_stat_type *vst;
	uint64_t *stats;
	int i, tocopy;

	if (index < 0 || count < 0)
		return (EINVAL);

	if (index > vst_num_elems)
		return (ENOENT);

	if (index == vst_num_elems) {
		*num_stats = 0;
		return (0);
	}

	tocopy = min(vst_num_elems - index, count);

	/* Let stats functions update their counters */
	for (i = 0; i < vst_num_types; i++) {
		vst = vsttab[i];
		if (vst->func != NULL)
			(*vst->func)(vcpu, vst);
	}

	/* Copy over the stats */
	stats = vcpu_stats(vcpu);
	memcpy(buf, stats + index, tocopy * sizeof(stats[0]));
	*num_stats = tocopy;
	return (0);
}

void *
vmm_stat_alloc(void)
{

	return (malloc(vst_size, M_VMM_STAT, M_WAITOK));
}

void
vmm_stat_init(void *vp)
{

	bzero(vp, vst_size);
}

void
vmm_stat_free(void *vp)
{
	free(vp, M_VMM_STAT);
}

int
vmm_stat_desc_copy(int index, char *buf, int bufsize)
{
	int i;
	struct vmm_stat_type *vst;

	for (i = 0; i < vst_num_types; i++) {
		vst = vsttab[i];
		if (index >= vst->index && index < vst->index + vst->nelems) {
			if (vst->nelems > 1) {
				snprintf(buf, bufsize, "%s[%d]",
					 vst->desc, index - vst->index);
			} else {
				strlcpy(buf, vst->desc, bufsize);
			}
			return (0);	/* found it */
		}
	}

	return (EINVAL);
}
