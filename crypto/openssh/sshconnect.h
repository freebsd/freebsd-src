/*	$OpenBSD: sshconnect.h,v 1.9 2001/04/12 19:15:25 markus Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
#ifndef SSHCONNECT_H
#define SSHCONNECT_H

int
ssh_connect(const char *host, struct sockaddr_storage * hostaddr,
    u_short port, int connection_attempts,
    int anonymous, struct passwd *pw,
    const char *proxy_command);

void
ssh_login(Key **keys, int nkeys, const char *orighost,
    struct sockaddr *hostaddr, struct passwd *pw);

void
check_host_key(char *host, struct sockaddr *hostaddr, Key *host_key,
    const char *user_hostfile, const char *system_hostfile);

void	ssh_kex(char *host, struct sockaddr *hostaddr);
void	ssh_kex2(char *host, struct sockaddr *hostaddr);

void
ssh_userauth1(const char *local_user, const char *server_user, char *host,
    Key **keys, int nkeys);
void
ssh_userauth2(const char *local_user, const char *server_user, char *host,
    Key **keys, int nkeys);

void	ssh_put_password(char *password);

#endif
