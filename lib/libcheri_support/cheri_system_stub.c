/*-
 * Copyright (c) 2014-2016 Robert N. M. Watson
 * Copyright (c) 2014 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include <sys/types.h>
#include <sys/errno.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>
#include <cheri/cheri_system.h>

#include <assert.h>
#include <errno.h>

struct cheri_object _cheri_system_object;

/* XXX-BD: should be in crt. */
extern char **environ;
char **environ;
extern const char *__progname;
const char *__progname = "";
extern register_t _sb_heapbase;
extern size_t             _sb_heaplen;
register_t        _sb_heapbase;
size_t            _sb_heaplen;

static char *heapcap;

void *sbrk(int incr);
void *
sbrk(int incr)
{

	if (heapcap == NULL) {
		if (_sb_heapbase == 0 || _sb_heaplen == 0)
			return (NULL);
		heapcap = cheri_csetbounds(cheri_setoffset(cheri_getdefault(),
		    _sb_heapbase), _sb_heaplen);
		assert(heapcap != NULL);
	}

	if (incr < 0) {
		if (-incr > (ssize_t)cheri_getoffset(heapcap)) {
			errno = EINVAL;
			return (void *)-1;
		}
	} else if (incr > (ssize_t)(cheri_getlen(heapcap) - cheri_getoffset(heapcap))) {
		errno = ENOMEM;
		return (void *)-1;
	}
	return (heapcap += incr);
}
