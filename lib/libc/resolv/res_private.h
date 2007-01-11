/* $FreeBSD: src/lib/libc/resolv/res_private.h,v 1.1.1.1.2.1 2006/07/17 10:09:58 ume Exp $ */

#ifndef res_private_h
#define res_private_h

struct __res_state_ext {
	union res_sockaddr_union nsaddrs[MAXNS];
	struct sort_list {
		int     af;
		union {
			struct in_addr  ina;
			struct in6_addr in6a;
		} addr, mask;
	} sort_list[MAXRESOLVSORT];
	char nsuffix[64];
	char nsuffix2[64];
};

extern int
res_ourserver_p(const res_state statp, const struct sockaddr *sa);

#endif
