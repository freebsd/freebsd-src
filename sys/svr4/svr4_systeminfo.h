/*
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#ifndef	_SVR4_SYSTEMINFO_H_
#define	_SVR4_SYSTEMINFO_H_

#define	SVR4_SI_SYSNAME		  1
#define	SVR4_SI_HOSTNAME	  2
#define	SVR4_SI_RELEASE 	  3
#define	SVR4_SI_VERSION		  4
#define	SVR4_SI_MACHINE		  5
#define	SVR4_SI_ARCHITECTURE	  6
#define	SVR4_SI_HW_SERIAL	  7
#define	SVR4_SI_HW_PROVIDER	  8
#define	SVR4_SI_SRPC_DOMAIN	  9
#define	SVR4_SI_SET_HOSTNAME	258
#define	SVR4_SI_SET_SRPC_DOMAIN	265
#define	SVR4_SI_SET_KERB_REALM	266
#define SVR4_SI_KERB_REALM	267
#define	SVR4_SI_PLATFORM	513
#define	SVR4_SI_ISALIST		514

#endif /* !_SVR4_SYSTEMINFO_H_ */
