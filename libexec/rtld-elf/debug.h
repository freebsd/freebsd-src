/*-
 * Copyright 1996-1998 John D. Polstra.
 * All rights reserved.
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
 * $FreeBSD: src/libexec/rtld-elf/debug.h,v 1.4 1999/12/27 04:44:01 jdp Exp $
 */

/*
 * Support for printing debugging messages.
 */

#ifndef DEBUG_H
#define DEBUG_H 1

#ifndef __GNUC__
#error "This file must be compiled with GCC"
#endif

#include <sys/cdefs.h>

#include <string.h>
#include <unistd.h>

extern void debug_printf(const char *, ...) __printflike(1, 2);
extern int debug;

#ifdef DEBUG
#define dbg(format, args...)	debug_printf(format , ## args)
#else
#define dbg(format, args...)	((void) 0)
#endif

#define assert(cond)	((cond) ? (void) 0 :		\
    (msg("ld-elf.so.1: assert failed: " __FILE__ ":"	\
      __XSTRING(__LINE__) "\n"), abort()))
#define msg(s)		write(1, s, strlen(s))
#define trace()		msg("ld-elf.so.1: " __XSTRING(__LINE__) "\n")

#endif /* DEBUG_H */
