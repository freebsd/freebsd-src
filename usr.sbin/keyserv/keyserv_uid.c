/*
 * Copyright (c) 1996
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
 *	$Id: keyserv_uid.c,v 1.13 1997/01/19 20:23:05 wpaul Exp $
 */

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <rpc/key_prot.h>
#include <rpc/des.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <unistd.h>

#include "keyserv.h"

#ifndef lint
static const char rcsid[] = "$Id: keyserv_uid.c,v 1.13 1997/01/19 20:23:05 wpaul Exp $";
#endif

/*
 * XXX should be declared somewhere
 */
struct cmessage {
	struct cmsghdr cmsg;
	struct cmsgcred cmcred;
};

int
__rpc_get_local_uid(uid, transp)
	uid_t *uid;
	SVCXPRT *transp;
{
	struct cmessage *cm;

	if (transp->xp_verf.oa_length < sizeof(struct cmessage) ||
		transp->xp_verf.oa_base == NULL ||
		transp->xp_verf.oa_flavor != AUTH_UNIX)
		return(-1);

	cm = (struct cmessage *)transp->xp_verf.oa_base;
	if (cm->cmsg.cmsg_type != SCM_CREDS)
		return(-1);

	*uid = cm->cmcred.cmcred_euid;
	return(0);
}
