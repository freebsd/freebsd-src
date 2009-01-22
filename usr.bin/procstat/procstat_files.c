/*-
 * Copyright (c) 2007 Robert N. M. Watson
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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <sys/user.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libutil.h>

#include "procstat.h"

static const char *
protocol_to_string(int domain, int type, int protocol)
{

	switch (domain) {
	case AF_INET:
	case AF_INET6:
		switch (protocol) {
		case IPPROTO_TCP:
			return ("TCP");
		case IPPROTO_UDP:
			return ("UDP");
		case IPPROTO_ICMP:
			return ("ICM");
		case IPPROTO_RAW:
			return ("RAW");
		case IPPROTO_SCTP:
			return ("SCT");
		case IPPROTO_DIVERT:
			return ("IPD");
		default:
			return ("IP?");
		}

	case AF_LOCAL:
		switch (type) {
		case SOCK_STREAM:
			return ("UDS");
		case SOCK_DGRAM:
			return ("UDD");
		default:
			return ("UD?");
		}
	default:
		return ("?");
	}
}

static void
addr_to_string(struct sockaddr_storage *ss, char *buffer, int buflen)
{
	char buffer2[INET6_ADDRSTRLEN];
	struct sockaddr_in6 *sin6;
	struct sockaddr_in *sin;
	struct sockaddr_un *sun;

	switch (ss->ss_family) {
	case AF_LOCAL:
		sun = (struct sockaddr_un *)ss;
		if (strlen(sun->sun_path) == 0)
			strlcpy(buffer, "-", buflen);
		else
			strlcpy(buffer, sun->sun_path, buflen);
		break;

	case AF_INET:
		sin = (struct sockaddr_in *)ss;
		snprintf(buffer, buflen, "%s:%d", inet_ntoa(sin->sin_addr),
		    ntohs(sin->sin_port));
		break;

	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)ss;
		if (inet_ntop(AF_INET6, &sin6->sin6_addr, buffer2,
		    sizeof(buffer2)) != NULL)
			snprintf(buffer, buflen, "%s.%d", buffer2,
			    ntohs(sin6->sin6_port));
		else
			strlcpy(buffer, "-", sizeof(buffer));
		break;

	default:
		strlcpy(buffer, "", buflen);
		break;
	}
}

static void
print_address(struct sockaddr_storage *ss)
{
	char addr[PATH_MAX];

	addr_to_string(ss, addr, sizeof(addr));
	printf("%s", addr);
}

void
procstat_files(pid_t pid, struct kinfo_proc *kipp)
{
	struct kinfo_file *freep, *kif;
	int i, cnt;
	const char *str;

	if (!hflag)
		printf("%5s %-16s %4s %1s %1s %-8s %3s %7s %-3s %-12s\n",
		    "PID", "COMM", "FD", "T", "V", "FLAGS", "REF", "OFFSET",
		    "PRO", "NAME");

	freep = kinfo_getfile(pid, &cnt);
	if (freep == NULL)
		return;
	for (i = 0; i < cnt; i++) {
		kif = &freep[i];
		
		printf("%5d ", pid);
		printf("%-16s ", kipp->ki_comm);
		switch (kif->kf_fd) {
		case KF_FD_TYPE_CWD:
			printf(" cwd ");
			break;

		case KF_FD_TYPE_ROOT:
			printf("root ");
			break;

		case KF_FD_TYPE_JAIL:
			printf("jail ");
			break;

		default:
			printf("%4d ", kif->kf_fd);
			break;
		}
		switch (kif->kf_type) {
		case KF_TYPE_VNODE:
			str = "v";
			break;

		case KF_TYPE_SOCKET:
			str = "s";
			break;

		case KF_TYPE_PIPE:
			str = "p";
			break;

		case KF_TYPE_FIFO:
			str = "f";
			break;

		case KF_TYPE_KQUEUE:
			str = "k";
			break;

		case KF_TYPE_CRYPTO:
			str = "c";
			break;

		case KF_TYPE_MQUEUE:
			str = "m";
			break;

		case KF_TYPE_SHM:
			str = "h";
			break;

		case KF_TYPE_PTS:
			str = "t";
			break;

		case KF_TYPE_SEM:
			str = "e";
			break;

		case KF_TYPE_NONE:
		case KF_TYPE_UNKNOWN:
		default:
			str = "?";
			break;
		}
		printf("%1s ", str);
		str = "-";
		if (kif->kf_type == KF_TYPE_VNODE) {
			switch (kif->kf_vnode_type) {
			case KF_VTYPE_VREG:
				str = "r";
				break;

			case KF_VTYPE_VDIR:
				str = "d";
				break;

			case KF_VTYPE_VBLK:
				str = "b";
				break;

			case KF_VTYPE_VCHR:
				str = "c";
				break;

			case KF_VTYPE_VLNK:
				str = "l";
				break;

			case KF_VTYPE_VSOCK:
				str = "s";
				break;

			case KF_VTYPE_VFIFO:
				str = "f";
				break;

			case KF_VTYPE_VBAD:
				str = "x";
				break;

			case KF_VTYPE_VNON:
			case KF_VTYPE_UNKNOWN:
			default:
				str = "?";
				break;
			}
		}
		printf("%1s ", str);
		printf("%s", kif->kf_flags & KF_FLAG_READ ? "r" : "-");
		printf("%s", kif->kf_flags & KF_FLAG_WRITE ? "w" : "-");
		printf("%s", kif->kf_flags & KF_FLAG_APPEND ? "a" : "-");
		printf("%s", kif->kf_flags & KF_FLAG_ASYNC ? "s" : "-");
		printf("%s", kif->kf_flags & KF_FLAG_FSYNC ? "f" : "-");
		printf("%s", kif->kf_flags & KF_FLAG_NONBLOCK ? "n" : "-");
		printf("%s", kif->kf_flags & KF_FLAG_DIRECT ? "d" : "-");
		printf("%s ", kif->kf_flags & KF_FLAG_HASLOCK ? "l" : "-");
		if (kif->kf_ref_count > -1)
			printf("%3d ", kif->kf_ref_count);
		else
			printf("%3c ", '-');
		if (kif->kf_offset > -1)
			printf("%7jd ", (intmax_t)kif->kf_offset);
		else
			printf("%7c ", '-');

		switch (kif->kf_type) {
		case KF_TYPE_VNODE:
		case KF_TYPE_FIFO:
		case KF_TYPE_PTS:
			printf("%-3s ", "-");
			printf("%-18s", kif->kf_path);
			break;

		case KF_TYPE_SOCKET:
			printf("%-3s ",
			    protocol_to_string(kif->kf_sock_domain,
			    kif->kf_sock_type, kif->kf_sock_protocol));
			/*
			 * While generally we like to print two addresses,
			 * local and peer, for sockets, it turns out to be
			 * more useful to print the first non-nul address for
			 * local sockets, as typically they aren't bound and
			 *  connected, and the path strings can get long.
			 */
			if (kif->kf_sock_domain == AF_LOCAL) {
				struct sockaddr_un *sun =
				    (struct sockaddr_un *)&kif->kf_sa_local;

				if (sun->sun_path[0] != 0)
					print_address(&kif->kf_sa_local);
				else
					print_address(&kif->kf_sa_peer);
			} else {
				print_address(&kif->kf_sa_local);
				printf(" ");
				print_address(&kif->kf_sa_peer);
			}
			break;

		default:
			printf("%-3s ", "-");
			printf("%-18s", "-");
		}

		printf("\n");
	}
	free(freep);
}
