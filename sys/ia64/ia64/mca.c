/*
 * Copyright (c) 2002 Marcel Moolenaar
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
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/uuid.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <machine/mca.h>
#include <machine/sal.h>
#include <machine/smp.h>

MALLOC_DEFINE(M_MCA, "MCA", "Machine Check Architecture");

int64_t		mca_info_size[SAL_INFO_TYPES];
vm_offset_t	mca_info_block;
struct mtx	mca_info_block_lock;

SYSCTL_NODE(_hw, OID_AUTO, mca, CTLFLAG_RW, 0, "MCA container");

static int mca_count;		/* Number of records stored. */
static int mca_first;		/* First (lowest) record ID. */
static int mca_last;		/* Last (highest) record ID. */

SYSCTL_INT(_hw_mca, OID_AUTO, count, CTLFLAG_RD, &mca_count, 0,
    "Record count");
SYSCTL_INT(_hw_mca, OID_AUTO, first, CTLFLAG_RD, &mca_first, 0,
    "First record id");
SYSCTL_INT(_hw_mca, OID_AUTO, last, CTLFLAG_RD, &mca_last, 0,
    "Last record id");

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

void
ia64_mca_save_state(int type)
{
	struct ia64_sal_result result;
	struct mca_record_header *hdr;
	struct sysctl_oid *oidp;
	char *name, *state;
	uint64_t seqnr;
	size_t recsz, totsz;

	/*
	 * Don't try to get the state if we couldn't get the size of
	 * the state information previously.
	 */
	if (mca_info_size[type] == -1)
		return;

	while (1) {
		mtx_lock_spin(&mca_info_block_lock);

		result = ia64_sal_entry(SAL_GET_STATE_INFO, type, 0,
		    mca_info_block, 0, 0, 0, 0);
		if (result.sal_status < 0) {
			mtx_unlock_spin(&mca_info_block_lock);
			return;
		}

		hdr = (struct mca_record_header *)mca_info_block;
		recsz = hdr->rh_length;
		seqnr = hdr->rh_seqnr;

		mtx_unlock_spin(&mca_info_block_lock);

		totsz = sizeof(struct sysctl_oid) + recsz + 32;
		oidp = malloc(totsz, M_MCA, M_WAITOK|M_ZERO);
		state = (char*)(oidp + 1);
		name = state + recsz;
		sprintf(name, "%lld", (long long)seqnr);

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
				free(oidp, M_MCA);
				continue;
			}
		}

		bcopy((char*)mca_info_block, state, recsz);

		oidp->oid_parent = &sysctl__hw_mca_children;
		oidp->oid_number = OID_AUTO;
		oidp->oid_kind = CTLTYPE_OPAQUE|CTLFLAG_RD|CTLFLAG_DYN;
		oidp->oid_arg1 = state;
		oidp->oid_arg2 = recsz;
		oidp->oid_name = name;
		oidp->oid_handler = mca_sysctl_handler;
		oidp->oid_fmt = "S,MCA";
		oidp->descr = "Error record";

		sysctl_register_oid(oidp);

		if (mca_count > 0) {
			if (seqnr < mca_first)
				mca_first = seqnr;
			else if (seqnr > mca_last)
				mca_last = seqnr;
		} else
			mca_first = mca_last = seqnr;

		mca_count++;

		/*
		 * Clear the state so that we get any other records when
		 * they exist.
		 */
		result = ia64_sal_entry(SAL_CLEAR_STATE_INFO, type, 0, 0, 0,
		    0, 0, 0);

		mtx_unlock_spin(&mca_info_block_lock);
	}
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
	for (i = 0; i <= SAL_INFO_TYPES; i++) {
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

	p = contigmalloc(max_size, M_TEMP, M_WAITOK, 0ul, 256*1024*1024 - 1,
	    PAGE_SIZE, 256*1024*1024);

	mca_info_block = IA64_PHYS_TO_RR7(ia64_tpa((u_int64_t)p));

	if (bootverbose)
		printf("MCA: allocated %ld bytes for state information\n",
		    max_size);

	/*
	 * Initialize the spin lock used to protect the info block. When APs
	 * get launched, there's a short moment of contention, but in all other
	 * cases it's not a hot spot. I think it's possible to have the MCA
	 * handler be called on multiple processors at the same time, but that
	 * should be rare. On top of that, performance is not an issue when
	 * dealing with machine checks...
	 */
	mtx_init(&mca_info_block_lock, "MCA spin lock", NULL, MTX_SPIN);

	/*
	 * Get and save any processor and platfom error records. Note that in
	 * a SMP configuration the processor records are for the BSP only. We
	 * let the APs get and save their own records when we wake them up.
	 */
	for (i = 0; i < SAL_INFO_TYPES; i++)
		ia64_mca_save_state(i);
}
