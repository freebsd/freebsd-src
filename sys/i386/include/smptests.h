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
 *	$Id: smptests.h,v 1.2 1997/04/27 21:17:56 fsmp Exp $
 */

#ifndef _MACHINE_SMPTESTS_H_
#define _MACHINE_SMPTESTS_H_


/*
 * various 'tests in progress'
 */


/*
 * use 'lowest priority' for sending IRQs to CPUs
 *
 * i386/i386/mplock.s, i386/i386/mpapic.c, kern/init_main.c
 *
 */
#define TEST_LOPRIO


/*
 * count INT hits by CPU
 *
 * i386/isa/vector.s
 *
 */
#define TEST_CPUHITS


/*
 * deal with broken smp_idleloop()
 */
#define IGNORE_IDLEPROCS


/**
 * hack to "fake-out" kernel into thinking it is running on a 'default config'
 *
 * value == default type
#define TEST_DEFAULT_CONFIG	6
 */


#endif /* _MACHINE_SMPTESTS_H_ */
