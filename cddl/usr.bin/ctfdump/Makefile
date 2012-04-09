# $FreeBSD: src/cddl/usr.bin/ctfdump/Makefile,v 1.1.4.2.4.1 2012/03/03 06:15:13 kensmith Exp $

.include	"../../Makefile.inc"

PROG=		ctfdump
SRCS=		dump.c \
		symbol.c \
		utils.c

WARNS?=		6

CFLAGS+=	-I${OPENSOLARIS_USR_DISTDIR} \
		-I${OPENSOLARIS_SYS_DISTDIR} \
		-I${OPENSOLARIS_USR_DISTDIR}/head \
		-I${OPENSOLARIS_USR_DISTDIR}/cmd/mdb/tools/common \
		-I${.CURDIR}/../../../sys/cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR}/tools/ctf/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common

LDFLAGS+=	-pthread

LDADD+=		-lelf -lz

.PATH:		${OPENSOLARIS_USR_DISTDIR}/tools/ctf/common
.PATH:		${OPENSOLARIS_USR_DISTDIR}/tools/ctf/dump

.include <bsd.prog.mk>
