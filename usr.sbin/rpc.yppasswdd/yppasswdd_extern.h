/*
 * Copyright (c) 1995, 1996
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
 *	$FreeBSD$
 */

#include <sys/types.h>
#include <limits.h>
#include <db.h>
#include <paths.h>
#include <rpc/rpc.h>
#include <pwd.h>
#include <err.h>
#include <rpcsvc/yp.h>
#include "yp_extern.h"
#include "ypxfr_extern.h"

#ifndef YPLIBDIR
#define YPLIBDIR "/usr/libexec/"
#endif

#ifndef _PATH_YP
#define _PATH_YP "/var/yp/"
#endif

#define MAP_UPDATE "yppwupdate"
#define MAP_UPDATE_PATH YPLIBDIR "yppwupdate"

extern char	*yp_dir;
extern char	*progname;
extern void	do_master __P(( void ));
extern void	yppasswdprog_1 __P(( struct svc_req *, register SVCXPRT * ));
extern void	reaper __P(( int ));
extern void	install_reaper __P(( int ));
extern int	pw_copy __P(( int, int, struct passwd * ));
extern int	pw_lock __P(( void ));
extern int	pw_mkdb __P(( char * ));
extern int	pw_tmp __P(( void ));
extern void	pw_init __P(( void ));
extern char	*ok_shell __P (( char * ));
extern char	*passfile;
extern char	*passfile_default;
extern char	*tempname;
extern char	*yppasswd_domain;
extern int	no_chsh;
extern int	no_chfn;
extern int	allow_additions;
extern int	multidomain;
extern int	resvport;
extern int	inplace;
extern int	verbose;
extern int	_rpc_dtablesize __P((void));
