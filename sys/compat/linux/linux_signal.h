/*-
 * Copyright (c) 2000 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef _LINUX_SIGNAL_H_
#define _LINUX_SIGNAL_H_

#define	LINUX_SI_TKILL		-6;

extern int bsd_to_linux_signal[];
extern int linux_to_bsd_signal[];

void linux_to_bsd_sigset(l_sigset_t *, sigset_t *);
void bsd_to_linux_sigset(sigset_t *, l_sigset_t *);
int linux_do_sigaction(struct thread *, int, l_sigaction_t *, l_sigaction_t *);
void ksiginfo_to_lsiginfo(ksiginfo_t *ksi, l_siginfo_t *lsi, l_int sig);

#define LINUX_SIG_VALID(sig)	((sig) <= LINUX_NSIG && (sig) > 0)

#define BSD_TO_LINUX_SIGNAL(sig)				\
	(((sig) <= LINUX_SIGTBLSZ) ? bsd_to_linux_signal[_SIG_IDX(sig)] : sig)

#endif /* _LINUX_SIGNAL_H_ */
