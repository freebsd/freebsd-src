/*-
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

#ifndef __PACKETBODY_H__
#define __PACKETBODY_H__

#ifdef __FreeBSD__
#include <sys/cdefs.h>
#if __has_feature(capabilities)
#define HAS_CHERI_CAPABILITIES
#include <machine/cheric.h>
#include <machine/cherireg.h>
#endif
#else
#define	__capability
#endif

#ifndef HAS_CHERI_CAPABILITIES
#define	cheri_ptr(var, size)		(void *)(var)
#define	cheri_ptrperm(var, size, perm)	(void *)(var)
#define	__capability
#endif

/*
 * Wrappers for str*() and mem*() functions on packet data.
 */
#ifdef HAS_CHERI_CAPABILITIES
#define	p_memchr		memchr_c
#define	p_memcmp		memcmp_c
#define	p_memcpy_from_packet	memcpy_c_fromcap
#define	p_strchr		strchr_c
#define	p_strcmp_static(p, s) \
	strcmp_c((p), (const char *)(s))
char	*p_strdup(const u_char * data);
#define p_strfree(str)		free(str)
#define	p_strncmp_static(p, s, l) \
	strncmp_c((p), (const char *)(s), (l))
#define	p_strncpy		strncpy_c_fromcap
char	*p_strndup(const u_char * data, size_t n);
#define	p_strnlen		strnlen_c
#define	p_strtol		strtol_c
#else
#define	p_memchr		memchr
#define	p_memcmp		memcmp
#define	p_memcpy_from_packet	memcpy
#define	p_strchr		strchr
#define	p_strcmp_static		strcmp
#define	p_strdup(str)		(char *)(str)
#define	p_strfree(str)		do {} while(0)
#define	p_strncmp_static	strncmp
#define	p_strncpy		strncpy
#define	p_strndup(str, n)	(char *)(str)
#define	p_strnlen		strnlen
#define	p_strtol		strtol
#endif

#endif /* __PACKETBODY_H__ */
