/*
 * pam_tokens.h
 *
 * $Id$
 * $FreeBSD$
 *
 * This is a Linux-PAM Library Private Header file. It contains tokens
 * that are used when we parse the configuration file(s).
 *
 * Please see end of file for copyright.
 *
 * Creator: Andrew Morgan.
 * 
 * $Log$
 */

#ifndef _PAM_TOKENS_H
#define _PAM_TOKENS_H

/* an array of actions */

const char * const _pam_token_actions[-_PAM_ACTION_UNDEF] = {
    "ignore",     /*  0 */
    "ok",         /* -1 */
    "done",       /* -2 */
    "bad",        /* -3 */
    "die",        /* -4 */
    "reset",      /* -5 */
};

/* an array of possible return values */

const char * const _pam_token_returns[_PAM_RETURN_VALUES+1] = {
    "success",           /* 0 */
    "open_err",          /* 1 */
    "symbol_err",        /* 2 */
    "service_err",       /* 3 */
    "system_err",        /* 4 */
    "buf_err",           /* 5 */
    "perm_denied",       /* 6 */
    "auth_err",          /* 7 */
    "cred_insufficient", /* 8 */
    "authinfo_unavail",  /* 9 */
    "user_unknown",      /* 10 */
    "maxtries",          /* 11 */
    "new_authtok_reqd",		/* 12 */
    "acct_expired",      /* 13 */
    "session_err",       /* 14 */
    "cred_unavail",      /* 15 */
    "cred_expired",      /* 16 */
    "cred_err",          /* 17 */
    "no_module_data",    /* 18 */
    "conv_err",          /* 19 */
    "authtok_err",       /* 20 */
    "authtok_recover_err", /* 21 */
    "authtok_lock_busy", /* 22 */
    "authtok_disable_aging", /* 23 */
    "try_again",         /* 24 */
    "ignore",            /* 25 */
    "abort",             /* 26 */
    "authtok_expired",   /* 27 */
    "module_unknown",    /* 28 */
    "bad_item",          /* 29 */
/* add new return codes here */
    "default"            /* this is _PAM_RETURN_VALUES and indicates
			    the default return action */
};

/*
 * Copyright (C) 1998, Andrew G. Morgan <morgan@linux.kernel.org>
 *
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 * 
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#endif /* _PAM_PRIVATE_H_ */
