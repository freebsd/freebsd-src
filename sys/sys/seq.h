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

#ifndef _SYS_SEQ_H_
#define _SYS_SEQ_H_

#ifdef _KERNEL
#include <sys/systm.h>
#endif
#include <sys/types.h>

/*
 * seq_t may be included in structs visible to userspace
 */
typedef uint32_t seq_t;

#ifdef _KERNEL

/*
 * Typical usage:
 *
 * writers:
 * 	lock_exclusive(&obj->lock);
 * 	seq_write_begin(&obj->seq);
 * 	.....
 * 	seq_write_end(&obj->seq);
 * 	unlock_exclusive(&obj->unlock);
 *
 * readers:
 * 	obj_t lobj;
 * 	seq_t seq;
 *
 * 	for (;;) {
 * 		seq = seq_read(&gobj->seq);
 * 		lobj = gobj;
 * 		if (seq_consistent(&gobj->seq, seq))
 * 			break;
 * 		cpu_spinwait();
 * 	}
 * 	foo(lobj);
 */		

/* A hack to get MPASS macro */
#include <sys/lock.h>

#include <machine/cpu.h>

static __inline bool
seq_in_modify(seq_t seqp)
{

	return (seqp & 1);
}

static __inline void
seq_write_begin(seq_t *seqp)
{

	MPASS(!seq_in_modify(*seqp));
	*seqp += 1;
	atomic_thread_fence_rel();
}

static __inline void
seq_write_end(seq_t *seqp)
{

	atomic_store_rel_int(seqp, *seqp + 1);
	MPASS(!seq_in_modify(*seqp));
}

static __inline seq_t
seq_read(const seq_t *seqp)
{
	seq_t ret;

	for (;;) {
		ret = atomic_load_acq_int(__DECONST(seq_t *, seqp));
		if (seq_in_modify(ret)) {
			cpu_spinwait();
			continue;
		}
		break;
	}

	return (ret);
}

static __inline seq_t
seq_consistent(const seq_t *seqp, seq_t oldseq)
{

	atomic_thread_fence_acq();
	return (*seqp == oldseq);
}

static __inline seq_t
seq_consistent_nomb(seq_t *seqp, seq_t oldseq)
{

	return (*seqp == oldseq);
}

#endif	/* _KERNEL */
#endif	/* _SYS_SEQ_H_ */
