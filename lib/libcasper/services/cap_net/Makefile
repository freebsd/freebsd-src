# $FreeBSD$

SHLIBDIR?=	/lib/casper

.include <src.opts.mk>

PACKAGE=libcasper

SHLIB_MAJOR=	1
INCSDIR?=	${INCLUDEDIR}/casper

.if ${MK_CASPER} != "no"
SHLIB=	cap_net

SRCS=	cap_net.c
.endif

INCS=	cap_net.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}
CFLAGS+=-DWITH_CASPER

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

MAN+=	cap_net.3

MLINKS+=cap_net.3 libcap_net.3
MLINKS+=cap_net.3 cap_bind.3
MLINKS+=cap_net.3 cap_connect.3
MLINKS+=cap_net.3 cap_net_free.3
MLINKS+=cap_net.3 cap_net_limit.3
MLINKS+=cap_net.3 cap_net_limit_addr2name.3
MLINKS+=cap_net.3 cap_net_limit_addr2name_family.3
MLINKS+=cap_net.3 cap_net_limit_bind.3
MLINKS+=cap_net.3 cap_net_limit_connect.3
MLINKS+=cap_net.3 cap_net_limit_init.3
MLINKS+=cap_net.3 cap_net_limit_name2addr.3
MLINKS+=cap_net.3 cap_net_limit_name2addr_family.3
MLINKS+=cap_net.3 cap_getaddrinfo.3
MLINKS+=cap_net.3 cap_gethostbyaddr.3
MLINKS+=cap_net.3 cap_gethostbyname.3
MLINKS+=cap_net.3 cap_gethostbyname2.3
MLINKS+=cap_net.3 cap_getnameinfo.3

.include <bsd.lib.mk>
