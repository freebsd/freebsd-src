/*
 * Copyright 1995 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <sysexits.h>

#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "rsrr.h"

char sunpath[MAXPATHLEN];
int s;

void exitfn(void) {
	close(s);
	unlink(sunpath);
}

int main(void) {
	struct sockaddr_un sun;
	char buf[RSRR_MAX_LEN];
	struct rsrr_header *rh;
	struct rsrr_vif *rvp;
	int i;

	s = socket(PF_LOCAL, SOCK_DGRAM, 0);
	if (s < 0) {
		err(EX_OSERR, "socket(PF_LOCAL, SOCK_DGRAM, 0)");
	}

	sun.sun_family = AF_LOCAL;
	snprintf(sunpath, sizeof sun.sun_path, "/tmp/testrsrr.%lu", 
		 (unsigned long)getpid());
	strcpy(sun.sun_path, sunpath);
	sun.sun_len = (offsetof(struct sockaddr_un, sun_path) 
		       + strlen(sunpath));

	if (bind(s, (struct sockaddr *)&sun, sun.sun_len) < 0) {
		err(EX_OSERR, "bind: %s", sunpath);
	}

	atexit(exitfn);		/* clean up if we exit on error */

	strcpy(sun.sun_path, RSRR_SERV_PATH);
	sun.sun_len = (offsetof(struct sockaddr_un, sun_path) 
		       + strlen(sunpath));

	if (connect(s, (struct sockaddr *)&sun, sun.sun_len) < 0) {
		err(EX_OSERR, "connect: %s", RSRR_SERV_PATH);
	}

	rh = (struct rsrr_header *)buf;
	rh->version = RSRR_MAX_VERSION;
	rh->type = RSRR_INITIAL_QUERY;
	rh->flags = 0;
	rh->num = 0;

	if (write(s, rh, sizeof *rh) == (ssize_t)-1) {
		err(EX_OSERR, "write(initial query)");
	}

	if (read(s, buf, sizeof buf) == (ssize_t)-1) {
		err(EX_OSERR, "read(initial reply)");
	}

	if (rh->version != RSRR_MAX_VERSION) {
		errx(EX_PROTOCOL, "bad remote version %d", rh->version);
	}

	if (rh->type != RSRR_INITIAL_REPLY) {
		errx(EX_PROTOCOL, "remote returned unexpected message type %d",
		     rh->type);
	}

	if (rh->flags) {
		printf("confusing flags: %d\n", rh->flags);
	}

	printf("There are %d vifs configured:\n", rh->num);

	printf(" Vif  Thresh  Status  Local address\n");
	for(i = 0, rvp = (struct rsrr_vif *)(rh + 1); i < rh->num; i++,rvp++) {
		printf(" %3d  %6d  %6d  %s\n", rvp->id, rvp->threshold,
		       rvp->status, inet_ntoa(rvp->local_addr));
	}
	exit(0);
}
