# $FreeBSD: src/lib/libmagic/Makefile,v 1.16 2010/08/01 12:10:32 rpaulo Exp $
# Copyright (c) David E. O'Brien, 2000-2004

.include <src.opts.mk>

CONTRIBDIR=	${SRCTOP}/contrib/file
.PATH: ${CONTRIBDIR}/src

LIB=	vuln_magic
SHLIB_MAJOR=	4
LIBADD=	z

SRCS=	apprentice.c apptype.c ascmagic.c cdf.c cdf_time.c compress.c \
	der.c encoding.c fsmagic.c funcs.c \
	is_tar.c magic.c print.c readcdf.c readelf.c softmagic.c

.if ${MK_DEMO_VULNERABILITIES} != "no"
SRCS:=	${SRCS:Nfuncs.c} funcs_vuln.c
.endif

MAGICPATH?=	/usr/share/misc

CFLAGS+= -DMAGIC='"${MAGICPATH}/magic"' -DHAVE_CONFIG_H
CFLAGS+= -I${.CURDIR} -I${CONTRIBDIR}/src -DBADMAGIC

WARNS?=		3

.include <bsd.lib.mk>
