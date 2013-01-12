# $FreeBSD$

.PATH: ${.CURDIR}/../../include

LIB=		egacy
SRC=
INCSGROUPS=	INCS
INCS=

BOOTSTRAPPING?=	0

_WITH_GETLINE!=	grep -c _WITH_GETLINE /usr/include/stdio.h || true
.if ${_WITH_GETLINE} == 0
.PATH: ${.CURDIR}/../../contrib/file ${.CURDIR}/../../lib/libmagic
SRCS=		getline.c config.h
CFLAGS+=	-DHAVE_CONFIG_H -I.
CLEANFILES+=	config.h

${SRCS:N*.h:R:S/$/.o/}: config.h
config.h: ${.CURDIR}/../../lib/libmagic/config.h
	grep -v HAVE_GETLINE ${.ALLSRC} > ${.TARGET}
.endif

_WITH_PWCACHEDB!= grep -c pwcache_groupdb /usr/include/pwd.h || true
.if ${_WITH_PWCACHEDB} == 0
.PATH: ${.CURDIR}/../../contrib/libc-pwcache
CFLAGS+=	-I${.CURDIR}/../../contrib/libc-pwcache \
		-I${.CURDIR}/../../lib/libc/include
SRCS+=		pwcache.c
.endif

_WITH_STRSVIS!=	grep -c strsvis /usr/include/vis.h || true
.if ${_WITH_STRSVIS} == 0
.PATH: ${.CURDIR}/../../contrib/libc-vis
SRCS+=		vis.c
CFLAGS+=	-I${.CURDIR}/../../contrib/libc-vis \
		-I${.CURDIR}/../../lib/libc/include
.endif

.if empty(SRCS)
SRCS=		dummy.c
.endif

.if defined(CROSS_BUILD_TESTING)
SUBDIR=		cross-build
.endif

.include <bsd.lib.mk>
