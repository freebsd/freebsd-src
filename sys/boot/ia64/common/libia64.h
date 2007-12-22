/*-
 * Copyright (c) 2006 Marcel Moolenaar
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

#ifndef _LIBIA64_H_
#define	_LIBIA64_H_

#include <bootstrap.h>
#include <ia64/include/bootinfo.h>

/*
 * Portability functions provided by the loader
 * implementation specific to the platform.
 */
extern uint64_t ldr_alloc(vm_offset_t);
extern int ldr_bootinfo(struct bootinfo *, uint64_t *);
extern int ldr_enter(const char *);

/*
 * Functions and variables provided by the ia64 common code
 * and shared by all loader implementations.
 */

extern int ia64_autoload(void);

extern ssize_t ia64_copyin(const void *, vm_offset_t, size_t);
extern ssize_t ia64_copyout(vm_offset_t, void *, size_t);
extern ssize_t ia64_readin(int, vm_offset_t, size_t);

extern char *ia64_fmtdev(struct devdesc *);
extern int ia64_getdev(void **, const char *, const char **);
extern int ia64_setcurrdev(struct env_var *, int, const void *);

#endif /* !_LIBIA64_H_ */
