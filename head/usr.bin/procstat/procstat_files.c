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
#include <libprocstat.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
procstat_files(struct procstat *procstat, struct kinfo_proc *kipp)
{ 
	struct sockstat sock;
	struct filestat_list *head;
	struct filestat *fst;
	const char *str;
	struct vnstat vn;
	int error;

	if (!hflag)
		printf("%5s %-16s %4s %1s %1s %-8s %3s %7s %-3s %-12s\n",
		    "PID", "COMM", "FD", "T", "V", "FLAGS", "REF", "OFFSET",
		    "PRO", "NAME");

	head = procstat_getfiles(procstat, kipp, 0);
	if (head == NULL)
		return;
	STAILQ_FOREACH(fst, head, next) {
		printf("%5d ", kipp->ki_pid);
		printf("%-16s ", kipp->ki_comm);
		if (fst->fs_uflags & PS_FST_UFLAG_CTTY)
			printf("ctty ");
		else if (fst->fs_uflags & PS_FST_UFLAG_CDIR)
			printf(" cwd ");
		else if (fst->fs_uflags & PS_FST_UFLAG_JAIL)
			printf("jail ");
		else if (fst->fs_uflags & PS_FST_UFLAG_RDIR)
			printf("root ");
		else if (fst->fs_uflags & PS_FST_UFLAG_TEXT)
			printf("text ");
		else if (fst->fs_uflags & PS_FST_UFLAG_TRACE)
			printf("trace ");
		else
			printf("%4d ", fst->fs_fd);

		switch (fst->fs_type) {
		case PS_FST_TYPE_VNODE:
			str = "v";
			break;

		case PS_FST_TYPE_SOCKET:
			str = "s";
			break;

		case PS_FST_TYPE_PIPE:
			str = "p";
			break;

		case PS_FST_TYPE_FIFO:
			str = "f";
			break;

		case PS_FST_TYPE_KQUEUE:
			str = "k";
			break;

		case PS_FST_TYPE_CRYPTO:
			str = "c";
			break;

		case PS_FST_TYPE_MQUEUE:
			str = "m";
			break;

		case PS_FST_TYPE_SHM:
			str = "h";
			break;

		case PS_FST_TYPE_PTS:
			str = "t";
			break;

		case PS_FST_TYPE_SEM:
			str = "e";
			break;

		case PS_FST_TYPE_NONE:
		case PS_FST_TYPE_UNKNOWN:
		default:
			str = "?";
			break;
		}
		printf("%1s ", str);
		str = "-";
		if (fst->fs_type == PS_FST_TYPE_VNODE) {
			error = procstat_get_vnode_info(procstat, fst, &vn, NULL);
			switch (vn.vn_type) {
			case PS_FST_VTYPE_VREG:
				str = "r";
				break;

			case PS_FST_VTYPE_VDIR:
				str = "d";
				break;

			case PS_FST_VTYPE_VBLK:
				str = "b";
				break;

			case PS_FST_VTYPE_VCHR:
				str = "c";
				break;

			case PS_FST_VTYPE_VLNK:
				str = "l";
				break;

			case PS_FST_VTYPE_VSOCK:
				str = "s";
				break;

			case PS_FST_VTYPE_VFIFO:
				str = "f";
				break;

			case PS_FST_VTYPE_VBAD:
				str = "x";
				break;

			case PS_FST_VTYPE_VNON:
			case PS_FST_VTYPE_UNKNOWN:
			default:
				str = "?";
				break;
			}
		}
		printf("%1s ", str);
		printf("%s", fst->fs_fflags & PS_FST_FFLAG_READ ? "r" : "-");
		printf("%s", fst->fs_fflags & PS_FST_FFLAG_WRITE ? "w" : "-");
		printf("%s", fst->fs_fflags & PS_FST_FFLAG_APPEND ? "a" : "-");
		printf("%s", fst->fs_fflags & PS_FST_FFLAG_ASYNC ? "s" : "-");
		printf("%s", fst->fs_fflags & PS_FST_FFLAG_SYNC ? "f" : "-");
		printf("%s", fst->fs_fflags & PS_FST_FFLAG_NONBLOCK ? "n" : "-");
		printf("%s", fst->fs_fflags & PS_FST_FFLAG_DIRECT ? "d" : "-");
		printf("%s ", fst->fs_fflags & PS_FST_FFLAG_HASLOCK ? "l" : "-");
		if (fst->fs_ref_count > -1)
			printf("%3d ", fst->fs_ref_count);
		else
			printf("%3c ", '-');
		if (fst->fs_offset > -1)
			printf("%7jd ", (intmax_t)fst->fs_offset);
		else
			printf("%7c ", '-');

		switch (fst->fs_type) {
		case PS_FST_TYPE_VNODE:
		case PS_FST_TYPE_FIFO:
		case PS_FST_TYPE_PTS:
			printf("%-3s ", "-");
			printf("%-18s", fst->fs_path != NULL ? fst->fs_path : "-");
			break;

		case PS_FST_TYPE_SOCKET:
			error = procstat_get_socket_info(procstat, fst, &sock, NULL);
			if (error != 0)
				break;
			printf("%-3s ",
			    protocol_to_string(sock.dom_family,
			    sock.type, sock.proto));
			/*
			 * While generally we like to print two addresses,
			 * local and peer, for sockets, it turns out to be
			 * more useful to print the first non-nul address for
			 * local sockets, as typically they aren't bound and
			 *  connected, and the path strings can get long.
			 */
			if (sock.dom_family == AF_LOCAL) {
				struct sockaddr_un *sun =
				    (struct sockaddr_un *)&sock.sa_local;

				if (sun->sun_path[0] != 0)
					print_address(&sock.sa_local);
				else
					print_address(&sock.sa_peer);
			} else {
				print_address(&sock.sa_local);
				printf(" ");
				print_address(&sock.sa_peer);
			}
			break;

		default:
			printf("%-3s ", "-");
			printf("%-18s", "-");
		}

		printf("\n");
	}
}
