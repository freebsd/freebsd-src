/*	$NetBSD$	*/
/*
 * [NetBSD for NEC PC98 series]
 *  Copyright (c) 1994, 1995, 1996 NetBSD/pc98 porting staff.
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Common command control queue funcs.
 * Written by N. Honda.
 */

#ifndef	_CCBQUE_H_
#define	_CCBQUE_H_

/* (I)  structure and prototype */
#define GENERIC_CCB_ASSERT(DEV, CCBTYPE)				\
TAILQ_HEAD(CCBTYPE##tab, CCBTYPE);					\
struct CCBTYPE##que {							\
	struct CCBTYPE##tab CCBTYPE##tab;				\
	int count;							\
	int maxccb;							\
};									\
void DEV##_init_ccbque __P((int));					\
struct CCBTYPE *DEV##_get_ccb __P((int));				\
void DEV##_free_ccb __P((struct CCBTYPE *));

/* (II)  static allocated memory */
#define GENERIC_CCB_STATIC_ALLOC(DEV, CCBTYPE)				\
static struct CCBTYPE##que CCBTYPE##que;

/* (III)  functions */
#define GENERIC_CCB(DEV, CCBTYPE, CHAIN)				\
									\
void									\
DEV##_init_ccbque(count)						\
	int count;							\
{									\
	if (CCBTYPE##que.maxccb == 0)					\
		TAILQ_INIT(&CCBTYPE##que.CCBTYPE##tab)			\
	CCBTYPE##que.maxccb += count;					\
}									\
									\
struct CCBTYPE *							\
DEV##_get_ccb(flags)							\
	int flags;							\
{									\
	struct CCBTYPE *cb;						\
	int s = splbio();						\
									\
	do								\
	{								\
		if (CCBTYPE##que.count > CCBTYPE##que.maxccb)		\
		{							\
			if (flags)					\
			{						\
				cb = NULL;				\
				goto done;				\
			}						\
			else						\
			{						\
				tsleep((caddr_t) &CCBTYPE##que.count,	\
					PRIBIO, "ccbwait", 0);		\
				continue;				\
			}						\
		}							\
									\
		if (cb = CCBTYPE##que.CCBTYPE##tab.tqh_first)		\
		{							\
			TAILQ_REMOVE(&CCBTYPE##que.CCBTYPE##tab, cb, CHAIN)\
			break;						\
		}							\
		else							\
		{							\
			if (cb = malloc(sizeof(*cb), M_DEVBUF, M_NOWAIT))\
			{						\
				bzero(cb, sizeof(*cb));			\
				break;					\
			}						\
			else if (flags)					\
				goto done;				\
									\
			tsleep((caddr_t) &CCBTYPE##que.count,		\
				PRIBIO, "ccbwait", 0);			\
		}							\
	}								\
	while (1);							\
	CCBTYPE##que.count ++;						\
									\
done:									\
	splx(s);							\
	return cb;							\
}									\
									\
void									\
DEV##_free_ccb(cb)							\
	struct CCBTYPE *cb;						\
{									\
	int s = splbio();						\
									\
	TAILQ_INSERT_TAIL(&CCBTYPE##que.CCBTYPE##tab, cb, CHAIN)	\
	CCBTYPE##que.count --;						\
									\
	if (CCBTYPE##que.count == CCBTYPE##que.maxccb)			\
		wakeup ((caddr_t) &CCBTYPE##que.count);			\
	splx(s);							\
}
#endif	/* !_CCBQUE_H_ */
