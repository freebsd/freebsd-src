/*
 * Copyright (c) 1996, by Steve Passe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef _MACHINE_SMPTESTS_H_
#define _MACHINE_SMPTESTS_H_


/*
 * Various 'tests in progress' and configuration parameters.
 */

/*
 * Send CPUSTOP IPI for stop/restart of other CPUs on DDB break.
#define VERBOSE_CPUSTOP_ON_DDBBREAK
 */
#define CPUSTOP_ON_DDBBREAK

/*
 * Misc. counters.
 *
#define COUNT_XINVLTLB_HITS
 */

/*
 * Address of POST hardware port.
 * Defining this enables POSTCODE macros.
 *
#define POST_ADDR		0x80
 */


/*
 * POST hardware macros.
 */
#ifdef POST_ADDR
#define ASMPOSTCODE_INC				\
	pushl	%eax ;				\
	movl	_current_postcode, %eax ;	\
	incl	%eax ;				\
	andl	$0xff, %eax ;			\
	movl	%eax, _current_postcode ;	\
	outb	%al, $POST_ADDR ;		\
	popl	%eax

/*
 * Overwrite the current_postcode value.
 */
#define ASMPOSTCODE(X)				\
	pushl	%eax ;				\
	movl	$X, %eax ;			\
	movl	%eax, _current_postcode ;	\
	outb	%al, $POST_ADDR ;		\
	popl	%eax

/*
 * Overwrite the current_postcode low nibble.
 */
#define ASMPOSTCODE_LO(X)			\
	pushl	%eax ;				\
	movl	_current_postcode, %eax ;	\
	andl	$0xf0, %eax ;			\
	orl	$X, %eax ;			\
	movl	%eax, _current_postcode ;	\
	outb	%al, $POST_ADDR ;		\
	popl	%eax

/*
 * Overwrite the current_postcode high nibble.
 */
#define ASMPOSTCODE_HI(X)			\
	pushl	%eax ;				\
	movl	_current_postcode, %eax ;	\
	andl	$0x0f, %eax ;			\
	orl	$(X<<4), %eax ;			\
	movl	%eax, _current_postcode ;	\
	outb	%al, $POST_ADDR ;		\
	popl	%eax
#else
#define ASMPOSTCODE_INC
#define ASMPOSTCODE(X)
#define ASMPOSTCODE_LO(X)
#define ASMPOSTCODE_HI(X)
#endif /* POST_ADDR */


#endif /* _MACHINE_SMPTESTS_H_ */
