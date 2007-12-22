/*-
 * Copyright (c) 2005 Christian S.J. Peron
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
 * $FreeBSD$
 */
#include <sys/types.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/user.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/bpf.h>
#include <net/bpfdesc.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "netstat.h"

/* print bpf stats */

static char *
bpf_pidname(pid_t pid)
{
	struct kinfo_proc newkp;
	int error, mib[4];
	size_t size;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = pid;
	size = sizeof(newkp);
	error = sysctl(mib, 4, &newkp, &size, NULL, 0);
	if (error < 0) {
		warn("kern.proc.pid failed");
		return (strdup("??????"));
	}
	return (strdup(newkp.ki_comm));
}

static void
bpf_flags(struct xbpf_d *bd, char *flagbuf)
{

	*flagbuf++ = bd->bd_promisc ? 'p' : '-';
	*flagbuf++ = bd->bd_immediate ? 'i' : '-';
	*flagbuf++ = bd->bd_hdrcmplt ? '-' : 'f';
	*flagbuf++ = (bd->bd_direction == BPF_D_IN) ? '-' :
	    ((bd->bd_direction == BPF_D_OUT) ? 'o' : 's');
	*flagbuf++ = bd->bd_feedback ? 'b' : '-';
	*flagbuf++ = bd->bd_async ? 'a' : '-';
	*flagbuf++ = bd->bd_locked ? 'l' : '-';
	*flagbuf++ = '\0';
}       

void
bpf_stats(char *ifname)
{
	struct xbpf_d *d, *bd;
	char *pname, flagbuf[12];
	size_t size;

	if (sysctlbyname("net.bpf.stats", NULL, &size,
	    NULL, 0) < 0) {
		warn("net.bpf.stats");
		return;
	}
	if (size == 0)
		return;
	bd = malloc(size);
	if (bd == NULL) {
		warn("malloc failed");
		return;
	}
	if (sysctlbyname("net.bpf.stats", bd, &size,
	    NULL, 0) < 0) {
		warn("net.bpf.stats");
		free(bd);
		return;
	}
	printf("%5s %6s %7s %9s %9s %9s %5s %5s %s\n",
	    "Pid", "Netif", "Flags", "Recv", "Drop", "Match", "Sblen",
	    "Hblen", "Command");
	for (d = &bd[0]; d < &bd[size / sizeof(*d)]; d++) {
		if (ifname && strcmp(ifname, d->bd_ifname) != 0)
			continue;
		bpf_flags(d, flagbuf);
		pname = bpf_pidname(d->bd_pid);
		printf("%5d %6s %7s %9lu %9lu %9lu %5d %5d %s\n",
		    d->bd_pid, d->bd_ifname, flagbuf,
		    d->bd_rcount, d->bd_dcount, d->bd_fcount,
		    d->bd_slen, d->bd_hlen, pname);
		free(pname);
	}
	free(bd);
}
