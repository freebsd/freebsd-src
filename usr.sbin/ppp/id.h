/*-
 * Copyright (c) 1997 Brian Somers <brian@Awfulhak.org>
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
 *	$Id: id.h,v 1.6 1998/05/28 23:15:37 brian Exp $
 */

struct utmp;
struct sockaddr_un;

extern void ID0init(void);
extern uid_t ID0realuid(void);
extern int ID0ioctl(int, unsigned long, void *);
extern int ID0unlink(const char *);
extern int ID0socket(int, int, int);
extern FILE *ID0fopen(const char *, const char *);
extern int ID0open(const char *, int, ...);
extern int ID0write(int, const void *, size_t);
extern int ID0uu_lock(const char *);
extern int ID0uu_lock_txfr(const char *, pid_t);
extern int ID0uu_unlock(const char *);
extern void ID0login(struct utmp *);
extern void ID0logout(const char *);
extern int ID0bind_un(int, const struct sockaddr_un *);
extern int ID0connect_un(int, const struct sockaddr_un *);
extern int ID0kill(pid_t, int);
