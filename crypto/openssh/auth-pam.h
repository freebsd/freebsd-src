/* $Id: auth-pam.h,v 1.16 2002/07/23 00:44:07 stevesk Exp $ */

/*
 * Copyright (c) 2000 Damien Miller.  All rights reserved.
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
 */

#include "includes.h"
#ifdef USE_PAM

#if !defined(SSHD_PAM_SERVICE)
# define SSHD_PAM_SERVICE		__progname
#endif

void start_pam(const char *user);
void finish_pam(void);
int auth_pam_password(Authctxt *authctxt, const char *password);
char **fetch_pam_environment(void);
void free_pam_environment(char **env);
int do_pam_authenticate(int flags);
int do_pam_account(char *username, char *remote_user);
void do_pam_session(char *username, const char *ttyname);
void do_pam_setcred(int init);
void print_pam_messages(void);
int is_pam_password_change_required(void);
void do_pam_chauthtok(void);
void do_pam_set_conv(struct pam_conv *);
void message_cat(char **p, const char *a);

#endif /* USE_PAM */
