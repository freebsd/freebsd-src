/*-
 * Copyright (c) 1989, 1990 William F. Jolitz.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)icu.s	7.2 (Berkeley) 5/21/91
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         5       00167
 * --------------------         -----   ----------------------
 * 
 * 28 Nov 92	Frank MacLachlan	Aligned addresses and data
 *					on 32bit boundaries.
 * 24 Mar 93	Rodney W. Grimes	Added interrupt counters for vmstat
 *					also stray and false intr counters added
 * 20 Apr 93	Bruce Evans		New npx-0.5 code
 * 25 Apr 93	Bruce Evans		Support new interrupt code (intr-0.1)
 *		Rodney W. Grimes	Reimplement above patches..
 * 17 May 93	Rodney W. Grimes	Redid the interrupt counter stuff
 *					moved the counters to vectors.s so
 *					they are next to the name tables.
 * 04 Jun 93	Bruce Evans		Fixed irq_num vs id_num for multiple
 *					devices configed on the same irq with
 *					respect to ipending.  Restructured
 *					not to use BUILD_VECTORS.
 *		Rodney W. Grimes	softsio1 only works if you have sio
 *					serial driver, added #include sio.h
 *					and #ifdef NSIO > 0 to protect it.
 */

/*
 * AT/386
 * Vector interrupt control section
 */

/*
 * XXX - this file is now misnamed.  All spls are now soft and the only thing
 * related to the hardware icu is that the bit numbering is the same in the
 * soft priority masks as in the hard ones.
 */

#include "sio.h"
#define	HIGHMASK	0xffff
#define	SOFTCLOCKMASK	0x8000

	.data
	.globl	_cpl
_cpl:	.long	0xffff			# current priority (all off)
	.globl	_imen
_imen:	.long	0xffff			# interrupt mask enable (all off)
#	.globl	_highmask
_highmask:	.long	HIGHMASK
	.globl	_ttymask
_ttymask:	.long	0
	.globl	_biomask
_biomask:	.long	0
	.globl	_netmask
_netmask:	.long	0
	.globl	_ipending
_ipending:	.long	0
vec:
	.long	vec0, vec1, vec2, vec3, vec4, vec5, vec6, vec7
	.long	vec8, vec9, vec10, vec11, vec12, vec13, vec14, vec15

#define	GENSPL(name, mask, event) \
	.globl	_spl/**/name ; \
	ALIGN_TEXT ; \
_spl/**/name: ; \
	COUNT_EVENT(_intrcnt_spl, event) ; \
	movl	_cpl,%eax ; \
	movl	%eax,%edx ; \
	orl	mask,%edx ; \
	movl	%edx,_cpl ; \
	SHOW_CPL ; \
	ret

#define	FASTSPL(mask) \
	movl	mask,_cpl ; \
	SHOW_CPL

#define	FASTSPL_VARMASK(varmask) \
	movl	varmask,%eax ; \
	movl	%eax,_cpl ; \
	SHOW_CPL

	.text

	ALIGN_TEXT
unpend_v:
	COUNT_EVENT(_intrcnt_spl, 0)
	bsfl	%eax,%eax		# slow, but not worth optimizing
	btrl	%eax,_ipending
	jnc	unpend_v_next		# some intr cleared the in-memory bit
	SHOW_IPENDING
	movl	Vresume(,%eax,4),%eax
	testl	%eax,%eax
	je	noresume
	jmp	%eax

	ALIGN_TEXT
/*
 * XXX - must be some fastintr, need to register those too.
 */
noresume:
#if NSIO > 0
	call	_softsio1
#endif
unpend_v_next:
	movl	_cpl,%eax
	movl	%eax,%edx
	notl	%eax
	andl	_ipending,%eax
	je	none_to_unpend
	jmp	unpend_v

/*
 * Handle return from interrupt after device handler finishes
 */
	ALIGN_TEXT
doreti:
	COUNT_EVENT(_intrcnt_spl, 1)
	addl	$4,%esp			# discard unit arg
	popl	%eax			# get previous priority
/*
 * Now interrupt frame is a trap frame!
 *
 * XXX - setting up the interrupt frame to be almost a stack frame is mostly
 * a waste of time.
 */
	movl	%eax,_cpl
	SHOW_CPL
	movl	%eax,%edx
	notl	%eax
	andl	_ipending,%eax
	jne	unpend_v
none_to_unpend:
	testl	%edx,%edx		# returning to zero priority?
	jne	1f			# nope, going to non-zero priority
	movl	_netisr,%eax
	testl	%eax,%eax		# check for softint s/traps
	jne	2f			# there are some
	jmp	test_resched		# XXX - schedule jumps better
	COUNT_EVENT(_intrcnt_spl, 2)	# XXX

	ALIGN_TEXT			# XXX
1:					# XXX
	COUNT_EVENT(_intrcnt_spl, 3)
	popl	%es
	popl	%ds
	popal
	addl	$8,%esp
	iret

#include "../net/netisr.h"

#define DONET(s, c, event) ; \
	.globl	c ; \
	btrl	$s,_netisr ; \
	jnc	1f ; \
	COUNT_EVENT(_intrcnt_spl, event) ; \
	call	c ; \
1:

	ALIGN_TEXT
2:
	COUNT_EVENT(_intrcnt_spl, 4)
/*
 * XXX - might need extra locking while testing reg copy of netisr, but
 * interrupt routines setting it would not cause any new problems (since we
 * don't loop, fresh bits will not be processed until the next doreti or spl0).
 */
	testl	$~((1 << NETISR_SCLK) | (1 << NETISR_AST)),%eax
	je	test_ASTs		# no net stuff, just temporary AST's
	FASTSPL_VARMASK(_netmask)
	DONET(NETISR_RAW, _rawintr, 5)
#ifdef INET
	DONET(NETISR_IP, _ipintr, 6)
#endif
#ifdef IMP
	DONET(NETISR_IMP, _impintr, 7)
#endif
#ifdef NS
	DONET(NETISR_NS, _nsintr, 8)
#endif
	FASTSPL($0)
test_ASTs:
	btrl	$NETISR_SCLK,_netisr
	jnc	test_resched
	COUNT_EVENT(_intrcnt_spl, 9)
	FASTSPL($SOFTCLOCKMASK)
/*
 * Back to an interrupt frame for a moment.
 */
	pushl	$0			# previous cpl (probably not used)
	pushl	$0x7f			# dummy unit number
	call	_softclock
	addl	$8,%esp			# discard dummies
	FASTSPL($0)
test_resched:
#ifdef notused1
	btrl	$NETISR_AST,_netisr
	jnc	2f
#endif
#ifdef notused2
	cmpl	$0,_want_resched
	je	2f
#endif
	cmpl	$0,_astpending		# XXX - put it back in netisr to
	je	2f			# reduce the number of tests
	testb	$SEL_RPL_MASK,TRAPF_CS_OFF(%esp)
					# to non-kernel (i.e., user)?
	je	2f			# nope, leave
	COUNT_EVENT(_intrcnt_spl, 10)
	movl	$0,_astpending
	call	_trap
2:
	COUNT_EVENT(_intrcnt_spl, 11)
	popl	%es
	popl	%ds
	popal
	addl	$8,%esp
	iret

/*
 * Interrupt priority mechanism
 *	-- soft splXX masks with group mechanism (cpl)
 *	-- h/w masks for currently active or unused interrupts (imen)
 *	-- ipending = active interrupts currently masked by cpl
 */

	GENSPL(bio, _biomask, 12)
	GENSPL(clock, $HIGHMASK, 13)	/* splclock == splhigh ex for count */
	GENSPL(high, $HIGHMASK, 14)
	GENSPL(imp, _netmask, 15)	/* splimp == splnet except for count */
	GENSPL(net, _netmask, 16)
	GENSPL(softclock, $SOFTCLOCKMASK, 17)
	GENSPL(tty, _ttymask, 18)

	.globl _splnone
	.globl _spl0
	ALIGN_TEXT
_splnone:
_spl0:
	COUNT_EVENT(_intrcnt_spl, 19)
in_spl0:
	movl	_cpl,%eax
	pushl	%eax			# save old priority
	testl	$(1 << NETISR_RAW) | (1 << NETISR_IP),_netisr
	je	over_net_stuff_for_spl0
	movl	_netmask,%eax		# mask off those network devices
	movl	%eax,_cpl		# set new priority
	SHOW_CPL
/*
 * XXX - what about other net intrs?
 */
	DONET(NETISR_RAW, _rawintr, 20)
#ifdef INET
	DONET(NETISR_IP, _ipintr, 21)
#endif
over_net_stuff_for_spl0:
	movl	$0,_cpl			# set new priority
	SHOW_CPL
	movl	_ipending,%eax
	testl	%eax,%eax
	jne	unpend_V
	popl	%eax			# return old priority
	ret

	.globl _splx
	ALIGN_TEXT
_splx:
	COUNT_EVENT(_intrcnt_spl, 22)
	movl	4(%esp),%eax		# new priority
	testl	%eax,%eax
	je	in_spl0			# going to "zero level" is special
	COUNT_EVENT(_intrcnt_spl, 23)
	movl	_cpl,%edx		# save old priority
	movl	%eax,_cpl		# set new priority
	SHOW_CPL
	notl	%eax
	andl	_ipending,%eax
	jne	unpend_V_result_edx
	movl	%edx,%eax		# return old priority
	ret

	ALIGN_TEXT
unpend_V_result_edx:
	pushl	%edx
unpend_V:
	COUNT_EVENT(_intrcnt_spl, 24)
	bsfl	%eax,%eax
	btrl	%eax,_ipending
	jnc	unpend_V_next
	SHOW_IPENDING
	movl	Vresume(,%eax,4),%edx
	testl	%edx,%edx
	je	noresumeV
/*
 * We would prefer to call the intr handler directly here but that doesn't
 * work for badly behaved handlers that want the interrupt frame.  Also,
 * there's a problem determining the unit number.  We should change the
 * interface so that the unit number is not determined at config time.
 */
	jmp	*vec(,%eax,4)

	ALIGN_TEXT
/*
 * XXX - must be some fastintr, need to register those too.
 */
noresumeV:
#if NSIO > 0
	call	_softsio1
#endif
unpend_V_next:
	movl	_cpl,%eax
	notl	%eax
	andl	_ipending,%eax
	jne	unpend_V
	popl	%eax
	ret

#define BUILD_VEC(irq_num) \
	ALIGN_TEXT ; \
vec/**/irq_num: ; \
	int	$ICU_OFFSET + (irq_num) ; \
	popl	%eax ; \
	ret

	BUILD_VEC(0)
	BUILD_VEC(1)
	BUILD_VEC(2)
	BUILD_VEC(3)
	BUILD_VEC(4)
	BUILD_VEC(5)
	BUILD_VEC(6)
	BUILD_VEC(7)
	BUILD_VEC(8)
	BUILD_VEC(9)
	BUILD_VEC(10)
	BUILD_VEC(11)
	BUILD_VEC(12)
	BUILD_VEC(13)
	BUILD_VEC(14)
	BUILD_VEC(15)
