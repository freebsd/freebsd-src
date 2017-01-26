 /*-
 * Copyright (c) 2016 Alex Richardson
 *
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */
#include <sys/types.h>
#include <setjmp.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>


/*
 * XXXAR: These wrapper only exists so that the CheriABI assembly code
 * can reference a local symbol. The current PIC_CALL() implementation does
 * not deal with preemptible symbols but we still want LD_PRELOAD to work for
 * all callsites of longjmperror(), abort() and sigprocmask()
 *
 * XXXAR: The better solution to this problem would be to add assembly for
 * PIC_(TAIL)CALL_PREEMPTIBLE() that does the right thing.
 */
#ifdef __CHERI_PURE_CAPABILITY__

void __hidden
__cheriabi_longjmperror(void)
{
	longjmperror();
}

int __hidden
__cheriabi_sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
	return sigprocmask(how, set, oset);
}

void __dead2 __hidden
__cheriabi_abort(void)
{
	abort();
}
#endif
