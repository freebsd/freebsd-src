/*-
 * Copyright (c) 1998 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 *	$Id: fetch.h,v 1.7 1998/12/16 10:24:55 des Exp $
 */

#ifndef _FETCH_H_INCLUDED
#define _FETCH_H_INCLUDED

#include <fetch_err.h>

#define _LIBFETCH_VER "libfetch/1.0"

#define URL_SCHEMELEN 16
#define URL_USERLEN 256
#define URL_PWDLEN 256

struct url {
    char	 scheme[URL_SCHEMELEN+1];
    char	 user[URL_USERLEN+1];
    char	 pwd[URL_PWDLEN+1];
    char	 host[MAXHOSTNAMELEN+1];
    int		 port;
    char	 doc[2];
};

struct url_stat {
    off_t	 size;
    time_t	 atime;
    time_t	 mtime;
};

struct url_ent {
    char	 name[MAXPATHLEN];
    struct url_stat stat;
};

/* FILE-specific functions */
FILE		*fetchGetFile(struct url *, char *);
FILE		*fetchPutFile(struct url *, char *);
int		 fetchStatFile(struct url *, struct url_stat *, char *);
struct url_ent	*fetchListFile(struct url *, char *);

/* HTTP-specific functions */
char		*fetchContentType(FILE *);
FILE		*fetchGetHTTP(struct url *, char *);
FILE		*fetchPutHTTP(struct url *, char *);
int		 fetchStatHTTP(struct url *, struct url_stat *, char *);
struct url_ent	*fetchListHTTP(struct url *, char *);

/* FTP-specific functions */
FILE		*fetchGetFTP(struct url *, char *);
FILE		*fetchPutFTP(struct url *, char *);
int		 fetchStatFTP(struct url *, struct url_stat *, char *);
struct url_ent	*fetchListFTP(struct url *, char *);

/* Generic functions */
struct url	*fetchParseURL(char *);
FILE		*fetchGetURL(char *, char *);
FILE		*fetchPutURL(char *, char *);
int		 fetchStatURL(char *, struct url_stat *, char *);
struct url_ent	*fetchListURL(char *, char *);
FILE		*fetchGet(struct url *, char *);
FILE		*fetchPut(struct url *, char *);
int		 fetchStat(struct url *, struct url_stat *, char *);
struct url_ent	*fetchList(struct url *, char *);

/* Last error code */
extern int	 fetchLastErrCode;

#endif
