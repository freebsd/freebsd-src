/*-
 * Copyright (c) 2015 The NetBSD Foundation, Inc.
 * Copyright (c) 2016 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Kurt Lidl
 * under sponsorship from the FreeBSD Foundation.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BLOCKLIST_CLIENT_H
#define BLOCKLIST_CLIENT_H

#ifndef BLOCKLIST_API_ENUM
enum {
	BLOCKLIST_AUTH_OK = 0,
	BLOCKLIST_AUTH_FAIL,
	BLOCKLIST_ABUSIVE_BEHAVIOR,
	BLOCKLIST_BAD_USER
};
#endif

#ifdef USE_BLOCKLIST
void blocklist_init(void);
void blocklist_notify(struct ssh *, int, const char *);

#define BLOCKLIST_INIT() blocklist_init()
#define BLOCKLIST_NOTIFY(ssh,x,msg) blocklist_notify(ssh,x,msg)

#else

#define BLOCKLIST_INIT()
#define BLOCKLIST_NOTIFY(ssh,x,msg)

#endif


#endif /* BLOCKLIST_CLIENT_H */
