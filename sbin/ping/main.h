/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2019 Jan Sucan <jansucan@FreeBSD.org>
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

#ifndef MAIN_H
#define MAIN_H 1

#ifdef IPSEC
#include <netipsec/ipsec.h>
#endif /*IPSEC*/

#if defined(INET) && defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
 #define PING4ADDOPTS "P:"
#else
 #define PING4ADDOPTS
#endif
#define PING4OPTS ".::4AaC:c:DdfG:g:Hh:I:i:Ll:M:m:nop:QqRrS:s:T:t:vW:z:" PING4ADDOPTS

#if defined(INET6) && defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
 #define PING6ADDOPTS "P:"
#elif defined(INET6) && defined(IPSEC) && !defined(IPSEC_POLICY_IPSEC)
 #define PING6ADDOPTS "ZE"
#else
 #define PING6ADDOPTS
#endif
#define PING6OPTS ".::6Aab:C:c:Dde:fHI:i:k:l:m:nNoOp:qS:s:t:uvyYW:z:" PING6ADDOPTS

void usage(void) __dead2;

#endif
