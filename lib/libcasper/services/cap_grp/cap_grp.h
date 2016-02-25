/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_CAP_GRP_H_
#define	_CAP_GRP_H_

struct group *cap_getgrent(cap_channel_t *chan);
struct group *cap_getgrnam(cap_channel_t *chan, const char *name);
struct group *cap_getgrgid(cap_channel_t *chan, gid_t gid);

int cap_getgrent_r(cap_channel_t *chan, struct group *grp, char *buffer,
    size_t bufsize, struct group **result);
int cap_getgrnam_r(cap_channel_t *chan, const char *name, struct group *grp,
    char *buffer, size_t bufsize, struct group **result);
int cap_getgrgid_r(cap_channel_t *chan, gid_t gid, struct group *grp,
    char *buffer, size_t bufsize, struct group **result);

int cap_setgroupent(cap_channel_t *chan, int stayopen);
int cap_setgrent(cap_channel_t *chan);
void cap_endgrent(cap_channel_t *chan);

int cap_grp_limit_cmds(cap_channel_t *chan, const char * const *cmds,
    size_t ncmds);
int cap_grp_limit_fields(cap_channel_t *chan, const char * const *fields,
    size_t nfields);
int cap_grp_limit_groups(cap_channel_t *chan, const char * const *names,
    size_t nnames, gid_t *gids, size_t ngids);

#endif	/* !_CAP_GRP_H_ */
