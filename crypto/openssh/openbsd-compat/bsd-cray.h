/* 
 * $Id: bsd-cray.h,v 1.5 2002/09/26 00:38:51 tim Exp $
 *
 * bsd-cray.h
 *
 * Copyright (c) 2002, Cray Inc.  (Wendy Palm <wendyp@cray.com>)
 * Significant portions provided by 
 *          Wayne Schroeder, SDSC <schroeder@sdsc.edu>
 *          William Jones, UTexas <jones@tacc.utexas.edu>
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
 * Created: Apr 22 16.34:00 2002 wp
 *
 * This file contains functions required for proper execution
 * on UNICOS systems.
 *
 */
#ifndef _BSD_CRAY_H
#define _BSD_CRAY_H

#ifdef _UNICOS
void cray_init_job(struct passwd *);		/* init cray job */
void cray_job_termination_handler(int);		/* process end of job signal */
void cray_login_failure(char *username, int errcode);
int cray_access_denied(char *username);
extern	char   cray_tmpdir[];			/* cray tmpdir */
#ifndef IA_SSHD
#define IA_SSHD IA_LOGIN
#endif
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN  64
#endif
#endif

#endif /* _BSD_CRAY_H */
