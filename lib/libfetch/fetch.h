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
 *	$Id: fetch.h,v 1.1.1.1 1998/07/09 16:52:41 des Exp $
 */

#ifndef _FETCH_H_INCLUDED
#define _FETCH_H_INCLUDED

#include <sys/param.h>
#include <stdio.h>

#define _LIBFETCH_VER "libfetch/1.0"

#define URL_SCHEMELEN 16
#define URL_USERLEN 256
#define URL_PWDLEN 256

struct url_s {
    char scheme[URL_SCHEMELEN+1];
    char user[URL_USERLEN+1];
    char pwd[URL_PWDLEN+1];
    char host[MAXHOSTNAMELEN+1];
    char *doc;
    int port;
};

typedef struct url_s url_t;

/* FILE-specific functions */
FILE	*fetchGetFile(url_t *, char *);
FILE	*fetchPutFile(url_t *, char *);

/* HTTP-specific functions */
char	*fetchContentType(FILE *);
FILE	*fetchGetHTTP(url_t *, char *);
FILE	*fetchPutHTTP(url_t *, char *);

/* FTP-specific functions */
FILE	*fetchGetFTP(url_t *, char *);
FILE	*fetchPutFTP(url_t *, char *);

/* Generic functions */
int	 fetchConnect(char *, int);
url_t	*fetchParseURL(char *);
void	 fetchFreeURL(url_t *);
FILE	*fetchGetURL(char *, char *);
FILE	*fetchPutURL(char *, char *);

/* Error code and string */
extern int fetchLastErrCode;
extern const char *fetchLastErrText;

#endif
