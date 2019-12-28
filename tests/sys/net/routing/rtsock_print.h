/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Alexander V. Chernikov
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef _NET_ROUTING_RTSOCK_PRINT_H_
#define _NET_ROUTING_RTSOCK_PRINT_H_


#define	RLOG(_fmt, ...)	printf("%s: " _fmt "\n", __func__, ##__VA_ARGS__)
#define	RLOG_ERRNO(_fmt, ...)	do {			\
	printf("%s: " _fmt, __func__, ##__VA_ARGS__);	\
	printf(": %s\n", strerror(errno));		\
} while(0)

#define	RTSOCK_ATF_REQUIRE_MSG(_rtm, _cond, _fmt, ...)	 do {	\
	if (!(_cond)) {						\
		printf("-- CONDITION FAILED, rtm dump  --\n\n");\
		rtsock_print_rtm(_rtm);				\
	}							\
	ATF_REQUIRE_MSG(_cond, _fmt, ##__VA_ARGS__);		\
} while (0);


/* from route.c */
static const char *const msgtypes[] = {
	"",
	"RTM_ADD",
	"RTM_DELETE",
	"RTM_CHANGE",
	"RTM_GET",
	"RTM_LOSING",
	"RTM_REDIRECT",
	"RTM_MISS",
	"RTM_LOCK",
	"RTM_OLDADD",
	"RTM_OLDDEL",
	"RTM_RESOLVE",
	"RTM_NEWADDR",
	"RTM_DELADDR",
	"RTM_IFINFO",
	"RTM_NEWMADDR",
	"RTM_DELMADDR",
	"RTM_IFANNOUNCE",
	"RTM_IEEE80211",
};

static const char metricnames[] =
    "\011weight\010rttvar\7rtt\6ssthresh\5sendpipe\4recvpipe\3expire"
    "\1mtu";
static const char routeflags[] =
    "\1UP\2GATEWAY\3HOST\4REJECT\5DYNAMIC\6MODIFIED\7DONE"
    "\012XRESOLVE\013LLINFO\014STATIC\015BLACKHOLE"
    "\017PROTO2\020PROTO1\021PRCLONING\022WASCLONED\023PROTO3"
    "\024FIXEDMTU\025PINNED\026LOCAL\027BROADCAST\030MULTICAST\035STICKY";
static const char ifnetflags[] =
    "\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5PTP\6b6\7RUNNING\010NOARP"
    "\011PPROMISC\012ALLMULTI\013OACTIVE\014SIMPLEX\015LINK0\016LINK1"
    "\017LINK2\020MULTICAST";
static const char addrnames[] =
    "\1DST\2GATEWAY\3NETMASK\4GENMASK\5IFP\6IFA\7AUTHOR\010BRD";

static int
_printb(char *buf, size_t bufsize, int b, const char *str)
{
	int i;
	int gotsome = 0;

	char *pbuf = buf;

	if (b == 0) {
		*pbuf = '\0';
		return (0);
	}
	while ((i = *str++) != 0) {
		if (b & (1 << (i-1))) {
			if (gotsome == 0)
				i = '<';
			else
				i = ',';
			*pbuf++ = i;
			gotsome = 1;
			for (; (i = *str) > 32; str++)
				*pbuf++ = i;
		} else
			while (*str > 32)
				str++;
	}
	if (gotsome)
		*pbuf++ = '>';
	*pbuf = '\0';

	return (int)(pbuf - buf);
}

const char *
rtsock_print_cmdtype(int cmd)
{

	return (msgtypes[cmd]);
}


#define	_PRINTX(fmt, ...)	do {				\
	one_len = snprintf(ptr, rem_len, fmt, __VA_ARGS__);	\
	ptr += one_len;						\
	rem_len -= one_len;					\
} while(0)


void
sa_print_hd(char *buf, int buflen, const char *data, int len)
{
	char *ptr;
	int one_len, rem_len;

	ptr = buf;
	rem_len = buflen;
	
	const char *last_char = NULL;
	unsigned char v;
	int repeat_count = 0;
	for (int i = 0; i < len; i++) {
		if (last_char && *last_char == data[i]) {
			repeat_count++;
			continue;
		}

		if (repeat_count > 1) {
			_PRINTX("{%d}", repeat_count);
			repeat_count = 0;
		}

		v = ((const unsigned char *)data)[i];
		if (last_char == NULL)
			_PRINTX("%02X", v);
		else
			_PRINTX(", %02X", v);

		last_char = &data[i];
		repeat_count = 1;
	}

	if (repeat_count > 1)
		snprintf(ptr, rem_len, "{%d}", repeat_count);
}

#undef _PRINTX

void
sa_print(const struct sockaddr *sa, int include_hexdump)
{
	char hdbuf[512], abuf[64];
	char ifbuf[128];
	const struct sockaddr_dl *sdl;
	const struct sockaddr_in6 *sin6;
	const struct sockaddr_in *sin;
	int i;

	switch (sa->sa_family) {
		case AF_INET:
			sin = (struct sockaddr_in *)sa;
			inet_ntop(AF_INET, &sin->sin_addr, abuf, sizeof(abuf));
			printf(" af=inet len=%d addr=%s", sa->sa_len, abuf);
			break;
		case AF_INET6:
			sin6 = (struct sockaddr_in6 *)sa;
			inet_ntop(AF_INET6, &sin6->sin6_addr, abuf, sizeof(abuf));
			int scope_id = sin6->sin6_scope_id;
			printf(" af=inet6 len=%d addr=%s", sa->sa_len, abuf);
			if (scope_id != 0) {
				memset(ifbuf, 0, sizeof(ifbuf));
				if_indextoname(scope_id, ifbuf);
				printf(" scope_id=%d if_name=%s", scope_id, ifbuf);
			}
			break;
		case AF_LINK:
			sdl = (const struct sockaddr_dl *)sa;
			int sdl_index = sdl->sdl_index;
			if (sdl_index != 0) {
				memset(ifbuf, 0, sizeof(ifbuf));
				if_indextoname(sdl_index, ifbuf);
				printf(" af=link len=%d sdl_index=%d if_name=%s", sdl->sdl_len, sdl_index, ifbuf);
			}
			if (sdl->sdl_nlen) {
				char _ifname[IFNAMSIZ];
				memcpy(_ifname, sdl->sdl_data, sdl->sdl_nlen);
				_ifname[sdl->sdl_nlen] = '\0';
				printf(" name=%s", _ifname);
			}
			if (sdl->sdl_alen) {
				printf(" addr=");
				const char *lladdr = LLADDR(sdl);
				for (int i = 0; i < sdl->sdl_alen; i++) {
					if (i + 1 < sdl->sdl_alen)
						printf("%02X:", ((const unsigned char *)lladdr)[i]);
					else
						printf("%02X", ((const unsigned char *)lladdr)[i]);
				}
			}
			break;
		default:
			printf(" af=%d len=%d", sa->sa_family, sa->sa_len);
	}

	if (include_hexdump) {
		sa_print_hd(hdbuf, sizeof(hdbuf), ((char *)sa), sa->sa_len);
		printf(" hd={%s}", hdbuf);
	}
	printf("\n");
}

/*
got message of size 240 on Mon Dec 16 09:23:31 2019
RTM_ADD: Add Route: len 240, pid: 25534, seq 2, errno 0, flags:<HOST,DONE,LLINFO,STATIC>
locks:  inits:
sockaddrs: <DST,GATEWAY>
*/

void
rtsock_print_rtm(struct rt_msghdr *rtm)
{
	struct timeval tv;
	struct tm tm_res;
	char buf[64];

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm_res); 
	strftime(buf, sizeof(buf), "%F %T", &tm_res);
	printf("Got message of size %hu on %s\n", rtm->rtm_msglen, buf);

	char flags_buf[256];
	_printb(flags_buf, sizeof(flags_buf), rtm->rtm_flags, routeflags);

	printf("%s: len %hu, pid: %d, seq %d, errno %d, flags: %s\n", msgtypes[rtm->rtm_type],
		rtm->rtm_msglen, rtm->rtm_pid, rtm->rtm_seq, rtm->rtm_errno, flags_buf);

	_printb(flags_buf, sizeof(flags_buf), rtm->rtm_addrs, addrnames);
	printf("sockaddrs: 0x%X %s\n", rtm->rtm_addrs, flags_buf);

	char *ptr = (char *)(rtm + 1);
	for (int i = 0; i < RTAX_MAX; i++) {
		if (rtm->rtm_addrs & (1 << i)) {
			struct sockaddr *sa = (struct sockaddr *)ptr;
			sa_print(sa, 1);

			/* add */
			ptr += ALIGN(((struct sockaddr *)ptr)->sa_len);
		}
	}

	printf("\n");

}

#endif
