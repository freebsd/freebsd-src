/*
 * Copyright (c) 1995
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: yp_extern.h,v 1.9 1995/12/16 04:01:55 wpaul Exp $
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <limits.h>
#include <db.h>
#include <rpc/rpc.h>

#ifndef _PATH_YP
#define _PATH_YP "/var/yp/"
#endif

#ifndef _PATH_LIBEXEC
#define _PATH_LIBEXEC "/usr/libexec/"
#endif

#ifndef MAX_CHILDREN
#define MAX_CHILDREN 20
#endif

/*
 * External functions and variables.
 */

extern int	debug;
extern int	do_dns;
extern int	children;
extern char 	*progname;
extern char	*yp_dir;
extern int	yp_errno;
extern void	yp_error __P((const char *, ...));
extern int	yp_get_record __P(( const char *, const char *, const DBT *, DBT *, int));
extern int	yp_first_record __P((const DB *, DBT *, DBT *));
extern int	yp_next_record __P((const DB *, DBT *, DBT *, int));
extern char	*yp_dnsname __P(( char * ));
extern char	*yp_dnsaddr __P(( const char * ));
extern int	yp_access __P((const char *, const struct svc_req * ));
extern int	yp_validdomain __P((const char * ));
extern DB	*yp_open_db __P(( const char *, const char *));
