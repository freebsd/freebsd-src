/*
 * Copyright (c) 2003 Marcel Moolenaar
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

#ifndef	_KSD_H_
#define	_KSD_H_

struct kse;
struct pthread;

register struct kse *_tp __asm("%r13");

static __inline int
_ksd_create(struct ksd *ksd, void *base, int size)
{
	ksd->ksd_base = base;
	ksd->ksd_size = size;
	return (0);
}

static __inline struct kse *
_ksd_curkse()
{
	/* XXX why not simply return _tp? */
	return ((struct kse *)_tp->k_mbx.km_udata);
}

static __inline struct pthread *
_ksd_curthread()
{
	return (_tp->k_curthread);
}

static __inline void
_ksd_destroy(struct ksd *ksd)
{
}

static __inline kse_critical_t
_ksd_get_tmbx()
{
	return (_tp->k_mbx.km_curthread);
}

static __inline kse_critical_t
_ksd_readandclear_tmbx()
{
	kse_critical_t crit;
	__asm("xchg8    %0=[%1],r0" : "=r"(crit)
	    : "r"(&_tp->k_mbx.km_curthread));
	return (crit);
}

static __inline void
_ksd_set_tmbx(kse_critical_t crit)
{
	_tp->k_mbx.km_curthread = crit;
}

static __inline int
_ksd_setprivate(struct ksd *ksd)
{
	_tp = (struct kse *)ksd->ksd_base;
	return (0);
}

#endif /* _KSD_H_ */
