#if !defined(lint) && !defined(SABER)
static const char sccsid[] = "@(#)ns_main.c	4.55 (Berkeley) 7/1/91";
static const char rcsid[] = "$Id: ns_signal.c,v 8.15 2002/05/18 01:39:15 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1986, 1989, 1990
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1996-2000 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* Import. */

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef SVR4	/* XXX */
# include <sys/sockio.h>
#else
#ifndef __hpux
# include <sys/mbuf.h>
#endif
#endif

#include <netinet/in.h>
#include <net/route.h>
#include <net/if.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <netdb.h>
#include <pwd.h>
#include <resolv.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>
#include <isc/list.h>

#include "port_after.h"
#include "named.h"

/* Forward. */

static	SIG_FN	onhup(int);
static	SIG_FN	onintr(int);
static	SIG_FN	setdumpflg(int);
static	SIG_FN	setIncrDbgFlg(int);
static	SIG_FN	setNoDbgFlg(int);
static	SIG_FN	setQrylogFlg(int);
static	SIG_FN	setstatsflg(int);
static	SIG_FN	discard_pipe(int);
static	SIG_FN	setreapflg(int);

/* Data. */

static struct {
	int	sig;
	SIG_FN	(*hand)(int);
} sighandlers[] = {
#ifdef DEBUG
	{ SIGUSR1, setIncrDbgFlg },
	{ SIGUSR2, setNoDbgFlg },
#endif
#if defined(SIGWINCH) && defined(QRYLOG)
	{ SIGWINCH, setQrylogFlg },
#endif
#if defined(SIGXFSZ)
	{ SIGXFSZ, onhup },	/* Wierd DEC Hesiodism, harmless. */
#endif
	{ SIGINT, setdumpflg },
	{ SIGILL, setstatsflg },
	{ SIGHUP, onhup },
	{ SIGCHLD, setreapflg },
	{ SIGPIPE, discard_pipe },
	{ SIGTERM, onintr }
};

static sigset_t mask;
static int blocked = 0;

/* Private. */

static SIG_FN
onhup(int sig) {

	UNUSED(sig);

	ns_need_unsafe(main_need_reload);
}

static SIG_FN
onintr(int sig) {

	UNUSED(sig);

	ns_need_unsafe(main_need_exit);
}

static SIG_FN
setdumpflg(int sig) {

	UNUSED(sig);

	ns_need_unsafe(main_need_dump);
}

#ifdef DEBUG
static SIG_FN
setIncrDbgFlg(int sig) {

	UNUSED(sig);

	desired_debug++;
	ns_need_unsafe(main_need_debug);
}

static SIG_FN
setNoDbgFlg(int sig) {

	UNUSED(sig);

	desired_debug = 0;
	ns_need_unsafe(main_need_debug);
}
#endif /*DEBUG*/

#if defined(QRYLOG) && defined(SIGWINCH)
static SIG_FN
setQrylogFlg(int sig) {

	UNUSED(sig);

	ns_need_unsafe(main_need_qrylog);
}
#endif /*QRYLOG && SIGWINCH*/

static SIG_FN
setstatsflg(int sig) {

	UNUSED(sig);

	ns_need_unsafe(main_need_statsdump);
}

static SIG_FN
discard_pipe(int sig) {
#ifdef SIGPIPE_ONE_SHOT
	int saved_errno = errno;
	struct sigaction sa;

	UNUSED(sig);

	memset(&sa, 0, sizeof sa);
	sa.sa_mask = mask;
	sa.sa_handler = discard_pipe;
	if (sigaction(SIGPIPE, &sa, NULL) < 0)
		ns_error(ns_log_os, "sigaction failed in discard_pipe: %s",
			 strerror(errno));
	errno = saved_errno;
#else
	UNUSED(sig);
#endif
}

static SIG_FN
setreapflg(int sig) {

	UNUSED(sig);

	ns_need_unsafe(main_need_reap);
}

/* Public. */

void
init_signals(void) {
	size_t sh;

	/* The mask of all our handlers will block all our other handlers. */
	(void)sigemptyset(&mask);
	for (sh = 0; sh < sizeof sighandlers / sizeof sighandlers[0]; sh++)
		sigaddset(&mask, sighandlers[sh].sig);

	/* Install our signal handlers with that shared mask. */
	for (sh = 0; sh < sizeof sighandlers / sizeof sighandlers[0]; sh++) {
		struct sigaction sa;

		memset(&sa, 0, sizeof sa);
		sa.sa_mask = mask;
		sa.sa_handler = sighandlers[sh].hand;
		if (sigaction(sighandlers[sh].sig, &sa, NULL) < 0)
			ns_error(ns_log_os,
			      "sigaction failed in set_signal_handler(%d): %s",
				 sighandlers[sh].sig, strerror(errno));
	}
	/* Unblock all signals that we expect to handle. */
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL) < 0)
		ns_panic(ns_log_os, 1, "sigblock failed: %s", strerror(errno));
}

void
block_signals(void) {
	INSIST(!blocked);
	if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0)
		ns_panic(ns_log_os, 1, "sigblock failed: %s", strerror(errno));
	blocked = 1;
}

void
unblock_signals(void) {
	INSIST(blocked);
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL) < 0)
		ns_panic(ns_log_os, 1, "sigblock failed: %s", strerror(errno));
	blocked = 0;
}
