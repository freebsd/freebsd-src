#ifndef _SYS__POSIX_H_
#define _SYS__POSIX_H_

/*-
 * Copyright (c) 1998 HD Associates, Inc.
 * All rights reserved.
 * contact: dufault@hda.com
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
 *
 *	$Id: $
 */

/*
 * This is a stand alone header file to set up for feature specification
 * defined to take place before the inclusion of any standard header.
 * It should only handle pre-processor defines.
 *
 * See section B.2.7 of 1003.1b-1993 
 *
 */

#ifndef _POSIX_VERSION
#define	_POSIX_VERSION		199009L
#endif

/* Test for visibility of pre-existing POSIX.4 features that should really
 * be conditional.  If _POSIX_C_SOURCE and _POSIX_SOURCE are not
 * defined then permit the pre-existing features to show up:
 */
#if !defined(_POSIX_SOURCE) && !defined(_POSIX_C_SOURCE)
#define _POSIX4_VISIBLE_HISTORICALLY
#endif

/* Test for visibility of additional POSIX.4 features:
 */
#if _POSIX_VERSION  >= 199309L && \
    (!defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE >= 199309L)
#define _POSIX4_VISIBLE
#define _POSIX4_VISIBLE_HISTORICALLY
#endif

/* I'm not sure if I'm allowed to do this, but at least initially
 * it may catch some teething problems:
 */

#if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE > _POSIX_VERSION)
#error _POSIX_C_SOURCE > _POSIX_VERSION
#endif

#define POSIX4_VISIBLE You missed the leading _!!
#define POSIX4_VISIBLE_FORCEABLY You left the old define in the code!!

#endif /* _SYS__POSIX_H_ */
