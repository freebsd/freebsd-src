/*-
 * Copyright (c) 2002-2010 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/uuid.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <machine/intr.h>
#include <machine/mca.h>
#include <machine/pal.h>
#include <machine/sal.h>
#include <machine/smp.h>

MALLOC_DEFINE(M_MCA, "MCA", "Machine Check Architecture");

struct mca_info {
	STAILQ_ENTRY(mca_info) mi_link;
	u_long	mi_seqnr;
	u_int	mi_cpuid;
	size_t	mi_recsz;
	char	mi_record[0];
};

STAILQ_HEAD(mca_info_list, mca_info);

static int64_t		mca_info_size[SAL_INFO_TYPES];
static vm_offset_t	mca_info_block;
static struct mtx	mca_info_block_lock;

SYSCTL_NODE(_hw, OID_AUTO, mca, CTLFLAG_RW, NULL, "MCA container");

static int mca_count;		/* Number of records stored. */
static int mca_first;		/* First (lowest) record ID. */
static int mca_last;		/* Last (highest) record ID. */

SYSCTL_INT(_hw_mca, OID_AUTO, count, CTLFLAG_RD, &mca_count, 0,
    "Record count");
SYSCTL_INT(_hw_mca, OID_AUTO, first, CTLFLAG_RD, &mca_first, 0,
    "First record id");
SYSCTL_INT(_hw_mca, OID_AUTO, last, CTLFLAG_RD, &mca_last, 0,
    "Last record id");

static struct mtx mca_sysctl_lock;

static u_int mca_xiv_cmc;

static int
mca_sysctl_inject(SYSCTL_HANDLER_ARGS)
{
	struct ia64_pal_result res;
	u_int val;
	int error;

	val = 0;
	error = sysctl_wire_old_buffer(req, sizeof(u_int));
	if (!error)
		error = sysctl_handle_int(oidp, &val, 0, req);

	if (error != 0 || req->newptr == NULL)
		return (error);

	/* For example: val=137 causes a fatal CPU error. */
	res = ia64_call_pal_stacked(PAL_MC_ERROR_INJECT, val, 0, 0);
	printf("%s: %#lx, %#lx, %#lx, %#lx\n", __func__, res.pal_status,
	    res.pal_result[0], res.pal_result[1], res.pal_result[2]);
	return (0);
}
SYSCTL_PROC(_hw_mca, OID_AUTO, inject, CTLTYPE_INT | CTLFLAG_RW, NULL, 0,
    mca_sysctl_inject, "I", "set to trigger a MCA");

static int
mca_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	int error = 0;

	if (!arg1)
		return (EINVAL);
	error = SYSCTL_OUT(req, arg1, arg2);

	if (error || !req->newptr)
		return (error);

	error = SYSCTL_IN(req, arg1, arg2);
	return (error);
}

static void
ia64_mca_collect_state(int type, struct mca_info_list *reclst)
{
	struct ia64_sal_result result;
	struct mca_record_header *hdr;
	struct mca_info *rec;
	uint64_t seqnr;
	size_t recsz;

	/*
	 * Don't try to get the state if we couldn't get the size of
	 * the state information previously.
	 */
	if (mca_info_size[type] == -1)
		return;

	if (mca_info_block == 0)
		return;

	while (1) {
		mtx_lock_spin(&mca_info_block_lock);
		result = ia64_sal_entry(SAL_GET_STATE_INFO, type, 0,
		    mca_info_block, 0, 0, 0, 0);
		if (result.sal_status < 0) {
			mtx_unlock_spin(&mca_info_block_lock);
			break;
		}

		hdr = (struct mca_record_header *)mca_info_block;
		recsz = hdr->rh_length;
		seqnr = hdr->rh_seqnr;

		mtx_unlock_spin(&mca_info_block_lock);

		rec = malloc(sizeof(struct mca_info) + recsz, M_MCA,
		    M_NOWAIT | M_ZERO);
		if (rec == NULL)
			/* XXX: Not sure what to do. */
			break;

		rec->mi_seqnr = seqnr;
		rec->mi_cpuid = PCPU_GET(cpuid);

		mtx_lock_spin(&mca_info_block_lock);

		/*
		 * If the info block doesn't have our record anymore because
		 * we temporarily unlocked it, get it again from SAL. I assume
		 * that it's possible that we could get a different record.
		 * I expect this to happen in a SMP configuration where the
		 * record has been cleared by a different processor. So, if
		 * we get a different record we simply abort with this record
		 * and start over.
		 */
		if (seqnr != hdr->rh_seqnr) {
			result = ia64_sal_entry(SAL_GET_STATE_INFO, type, 0,
			    mca_info_block, 0, 0, 0, 0);
			if (seqnr != hdr->rh_seqnr) {
				mtx_unlock_spin(&mca_info_block_lock);
				free(rec, M_MCA);
				continue;
			}
		}

		rec->mi_recsz = recsz;
		bcopy((char*)mca_info_block, rec->mi_record, recsz);

		/*
		 * Clear the state so that we get any other records when
		 * they exist.
		 */
		result = ia64_sal_entry(SAL_CLEAR_STATE_INFO, type, 0, 0, 0,
		    0, 0, 0);

		mtx_unlock_spin(&mca_info_block_lock);

		STAILQ_INSERT_TAIL(reclst, rec, mi_link);
	}
}

void
ia64_mca_save_state(int type)
{
	char name[64];
	struct mca_info_list reclst = STAILQ_HEAD_INITIALIZER(reclst);
	struct mca_info *rec;
	struct sysctl_oid *oid;

	ia64_mca_collect_state(type, &reclst);

	STAILQ_FOREACH(rec, &reclst, mi_link) {
		sprintf(name, "%lu", rec->mi_seqnr);
		oid = SYSCTL_ADD_NODE(NULL, SYSCTL_STATIC_CHILDREN(_hw_mca),
		    OID_AUTO, name, CTLFLAG_RW, NULL, name);
		if (oid == NULL)
			continue;

		mtx_lock(&mca_sysctl_lock);
		if (mca_count > 0) {
			if (rec->mi_seqnr < mca_first)
				mca_first = rec->mi_seqnr;
			else if (rec->mi_seqnr > mca_last)
				mca_last = rec->mi_seqnr;
		} else
			mca_first = mca_last = rec->mi_seqnr;
		mca_count++;
		mtx_unlock(&mca_sysctl_lock);

		sprintf(name, "%u", rec->mi_cpuid);
		SYSCTL_ADD_PROC(NULL, SYSCTL_CHILDREN(oid), rec->mi_cpuid,
		    name, CTLTYPE_OPAQUE | CTLFLAG_RD, rec->mi_record,
		    rec->mi_recsz, mca_sysctl_handler, "S,MCA", "MCA record");
	}
}

static u_int
ia64_mca_intr(struct thread *td, u_int xiv, struct trapframe *tf)
{

	if (xiv == mca_xiv_cmc) {
		printf("MCA: corrected machine check (CMC) interrupt\n");
		return (0);
	}

	return (0);
}

void
ia64_mca_init_ap(void)
{

	if (mca_xiv_cmc != 0)
		ia64_set_cmcv(mca_xiv_cmc);
}

void
ia64_mca_init(void)
{
	struct ia64_sal_result result;
	uint64_t max_size;
	char *p;
	int i;

	/*
	 * Get the sizes of the state information we can get from SAL and
	 * allocate a common block (forgive me my Fortran :-) for use by
	 * support functions. We create a region 7 address to make it
	 * easy on the OS_MCA or OS_INIT handlers to get the state info
	 * under unreliable conditions.
	 */
	max_size = 0;
	for (i = 0; i < SAL_INFO_TYPES; i++) {
		result = ia64_sal_entry(SAL_GET_STATE_INFO_SIZE, i, 0, 0, 0,
		    0, 0, 0);
		if (result.sal_status == 0) {
			mca_info_size[i] = result.sal_result[0];
			if (mca_info_size[i] > max_size)
				max_size = mca_info_size[i];
		} else
			mca_info_size[i] = -1;
	}
	max_size = round_page(max_size);

	p = (max_size) ? contigmalloc(max_size, M_TEMP, 0, 0ul,
	    256*1024*1024 - 1, PAGE_SIZE, 256*1024*1024) : NULL;
	if (p != NULL) {
		mca_info_block = IA64_PHYS_TO_RR7(ia64_tpa((u_int64_t)p));

		if (bootverbose)
			printf("MCA: allocated %ld bytes for state info.\n",
			    max_size);
	}

	/*
	 * Initialize the spin lock used to protect the info block. When APs
	 * get launched, there's a short moment of contention, but in all other
	 * cases it's not a hot spot. I think it's possible to have the MCA
	 * handler be called on multiple processors at the same time, but that
	 * should be rare. On top of that, performance is not an issue when
	 * dealing with machine checks...
	 */
	mtx_init(&mca_info_block_lock, "MCA info lock", NULL, MTX_SPIN);

	/*
	 * Serialize sysctl operations with a sleep lock. Note that this
	 * implies that we update the sysctl tree in a context that allows
	 * sleeping.
	 */
	mtx_init(&mca_sysctl_lock, "MCA sysctl lock", NULL, MTX_DEF);

	/*
	 * Get and save any processor and platfom error records. Note that in
	 * a SMP configuration the processor records are for the BSP only. We
	 * let the APs get and save their own records when we wake them up.
	 */
	for (i = 0; i < SAL_INFO_TYPES; i++)
		ia64_mca_save_state(i);

	/*
	 * Allocate a XIV for CMC interrupts, so that we can collect and save
	 * the corrected processor checks.
	 */
	mca_xiv_cmc = ia64_xiv_alloc(PI_SOFT, IA64_XIV_PLAT, ia64_mca_intr);
	if (mca_xiv_cmc != 0)
		ia64_set_cmcv(mca_xiv_cmc);
	else
		printf("MCA: CMC vector could not be allocated\n");
}
