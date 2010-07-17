# $FreeBSD: src/cddl/usr.bin/sgsmsg/Makefile,v 1.1.4.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include	"../../Makefile.inc"

# This program is required as a bootstrap tool for 'make buildworld'
PROG=		sgsmsg

SRCS=		avl.c sgsmsg.c string_table.c findprime.c

CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR}/cmd/sgs/include \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common

.PATH:		${OPENSOLARIS_USR_DISTDIR}/cmd/sgs/tools/common
.PATH:		${OPENSOLARIS_SYS_DISTDIR}/common/avl

NO_MAN=

.include <bsd.prog.mk>
