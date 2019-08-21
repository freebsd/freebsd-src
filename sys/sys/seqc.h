/*-
 * Copyright (c) 2014 Mateusz Guzik <mjg@FreeBSD.org>
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

#ifndef _SYS_SEQC_H_
#define _SYS_SEQC_H_

#ifdef _KERNEL
#include <sys/systm.h>
#endif
#include <sys/types.h>

/*
 * seqc_t may be included in structs visible to userspace
 */
typedef uint32_t seqc_t;

#ifdef _KERNEL

/* A hack to get MPASS macro */
#include <sys/lock.h>

#include <machine/cpu.h>

static __inline bool
seqc_in_modify(seqc_t seqcp)
{

	return (seqcp & 1);
}

static __inline void
seqc_write_begin(seqc_t *seqcp)
{

	critical_enter();
	MPASS(!seqc_in_modify(*seqcp));
	*seqcp += 1;
	atomic_thread_fence_rel();
}

static __inline void
seqc_write_end(seqc_t *seqcp)
{

	atomic_store_rel_int(seqcp, *seqcp + 1);
	MPASS(!seqc_in_modify(*seqcp));
	critical_exit();
}

static __inline seqc_t
seqc_read(const seqc_t *seqcp)
{
	seqc_t ret;

	for (;;) {
		ret = atomic_load_acq_int(__DECONST(seqc_t *, seqcp));
		if (__predict_false(seqc_in_modify(ret))) {
			cpu_spinwait();
			continue;
		}
		break;
	}

	return (ret);
}

static __inline bool
seqc_consistent_nomb(const seqc_t *seqcp, seqc_t oldseqc)
{

	return (*seqcp == oldseqc);
}

static __inline bool
seqc_consistent(const seqc_t *seqcp, seqc_t oldseqc)
{

	atomic_thread_fence_acq();
	return (seqc_consistent_nomb(seqcp, oldseqc));
}

#endif	/* _KERNEL */
#endif	/* _SYS_SEQC_H_ */
