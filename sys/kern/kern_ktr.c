/*
 * Copyright (c) 2000
 *	John Baldwin <jhb@FreeBSD.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * This module holds the global variables used by KTR.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/ktr.h>
#include <sys/linker_set.h>
#include <sys/sysctl.h>

#ifdef KTR_EXTEND
/*
 * This variable is used only by gdb to work out what fields are in
 * ktr_entry.
 */
int     ktr_extend = 1;
SYSCTL_INT(_debug, OID_AUTO, ktr_extend, CTLFLAG_RD, &ktr_extend, 1, "");
#else
int     ktr_extend = 0;
SYSCTL_INT(_debug, OID_AUTO, ktr_extend, CTLFLAG_RD, &ktr_extend, 0, "");
#endif

int	ktr_cpumask = KTR_CPUMASK;
SYSCTL_INT(_debug, OID_AUTO, ktr_cpumask, CTLFLAG_RW, &ktr_cpumask, KTR_CPUMASK, "");

int	ktr_mask = KTR_MASK;
SYSCTL_INT(_debug, OID_AUTO, ktr_mask, CTLFLAG_RW, &ktr_mask, KTR_MASK, "");

int	ktr_entries = KTR_ENTRIES;
SYSCTL_INT(_debug, OID_AUTO, ktr_entries, CTLFLAG_RD, &ktr_entries, KTR_ENTRIES, "");

volatile int	ktr_idx = 0;
struct	ktr_entry ktr_buf[KTR_ENTRIES];
