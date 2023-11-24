
SHLIBDIR?=	/lib

.include <src.opts.mk>

PACKAGE=	runtime

SHLIB_MAJOR=	2
INCSDIR?=	${INCLUDEDIR}/casper

.if ${MK_CASPER} != "no"
SHLIB=	cap_dns

SRCS=	cap_dns.c
.endif

INCS=	cap_dns.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

MAN+=	cap_dns.3

MLINKS+=cap_dns.3 libcap_dns.3
MLINKS+=cap_dns.3 cap_dns_type_limit.3
MLINKS+=cap_dns.3 cap_dns_family_limit.3

.include <bsd.lib.mk>
