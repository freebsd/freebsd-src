/*-
 * Copyright 1998 Juniper Networks, Inc.
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
 *
 *	$FreeBSD$
 */

#ifndef PAM_MOD_MISC_H
#define PAM_MOD_MISC_H

#include <sys/cdefs.h>

/* Options */
#define	PAM_OPT_DEBUG			0x0001
#define	PAM_OPT_NO_WARN			0x0002
#define	PAM_OPT_USE_FIRST_PASS		0x0004
#define	PAM_OPT_TRY_FIRST_PASS		0x0008
#define	PAM_OPT_USE_MAPPED_PASS		0x0010
#define	PAM_OPT_ECHO_PASS		0x0020
#define	PAM_OPT_AUTH_AS_SELF		0x0040
#define	PAM_OPT_NULLOK			0x0080
#define	PAM_OPT_NO_ANON			0x0100
#define	PAM_OPT_IGNORE			0x0200

__BEGIN_DECLS
int	 pam_get_pass(pam_handle_t *, const char **, const char *, int);
int	 pam_prompt(pam_handle_t *, int, const char *, char **);
int	 pam_std_option(int *, const char *);
__END_DECLS

#endif
