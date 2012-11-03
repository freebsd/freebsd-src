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

.if empty(SRCS)
SRCS=		dummy.c
.endif

.if defined(CROSS_BUILD_TESTING)
SUBDIR=		cross-build
.endif

.include <bsd.lib.mk>
