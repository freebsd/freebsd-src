/* cdefs.h

   Standard C definitions... */

/*
 * Copyright (c) 1995 RadioMail Corporation.  All rights reserved.
 * Copyright (c) 1996-1999 Internet Software Consortium.
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
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software was written for RadioMail Corporation by Ted Lemon
 * under a contract with Vixie Enterprises.   Further modifications have
 * been made for the Internet Software Consortium under a contract
 * with Vixie Laboratories.
 */

#if !defined (__ISC_DHCP_CDEFS_H__)
#define __ISC_DHCP_CDEFS_H__
/* Delete attributes if not gcc or not the right version of gcc. */
#if !defined(__GNUC__) || __GNUC__ < 2 || \
        (__GNUC__ == 2 && __GNUC_MINOR__ < 5) || defined (darwin)
#define __attribute__(x)
#endif

#if (defined (__GNUC__) || defined (__STDC__)) && !defined (BROKEN_ANSI)
#define PROTO(x)	x
#define KandR(x)
#define ANSI_DECL(x)	x
#if defined (__GNUC__)
#define INLINE		inline
#else
#define INLINE
#endif /* __GNUC__ */
#else
#define PROTO(x)	()
#define KandR(x)	x
#define ANSI_DECL(x)
#define INLINE
#endif /* __GNUC__ || __STDC__ */
#endif /* __ISC_DHCP_CDEFS_H__ */
