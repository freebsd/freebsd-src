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

#include "includes.h"
RCSID("$OpenBSD: auth2-none.c,v 1.4 2002/06/27 10:35:47 deraadt Exp $");

#include "auth.h"
#include "xmalloc.h"
#include "packet.h"
#include "log.h"
#include "servconf.h"
#include "atomicio.h"
#include "compat.h"
#include "ssh2.h"
#include "monitor_wrap.h"

/* import */
extern ServerOptions options;

/* "none" is allowed only one time */
static int none_enabled = 1;

char *
auth2_read_banner(void)
{
	struct stat st;
	char *banner = NULL;
	off_t len, n;
	int fd;

	if ((fd = open(options.banner, O_RDONLY)) == -1)
		return (NULL);
	if (fstat(fd, &st) == -1) {
		close(fd);
		return (NULL);
	}
	len = st.st_size;
	banner = xmalloc(len + 1);
	n = atomicio(read, fd, banner, len);
	close(fd);

	if (n != len) {
		xfree(banner);
		return (NULL);
	}
	banner[n] = '\0';

	return (banner);
}

static void
userauth_banner(void)
{
	char *banner = NULL;

	if (options.banner == NULL || (datafellows & SSH_BUG_BANNER))
		return;

	if ((banner = PRIVSEP(auth2_read_banner())) == NULL)
		goto done;

	packet_start(SSH2_MSG_USERAUTH_BANNER);
	packet_put_cstring(banner);
	packet_put_cstring("");		/* language, unused */
	packet_send();
	debug("userauth_banner: sent");
done:
	if (banner)
		xfree(banner);
}

static int
userauth_none(Authctxt *authctxt)
{
	none_enabled = 0;
	packet_check_eom();
	userauth_banner();
#ifdef HAVE_CYGWIN
	if (check_nt_auth(1, authctxt->pw) == 0)
		return(0);
#endif
	return (authctxt->valid ? PRIVSEP(auth_password(authctxt, "")) : 0);
}

Authmethod method_none = {
	"none",
	userauth_none,
	&none_enabled
};
