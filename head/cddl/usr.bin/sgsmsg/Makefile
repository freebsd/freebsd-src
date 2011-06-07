# $FreeBSD$

.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/cmd/sgs/tools/common
.PATH: ${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/avl

# This program is required as a bootstrap tool for 'make buildworld'
PROG=		sgsmsg
NO_MAN=
SRCS=		avl.c sgsmsg.c string_table.c findprime.c

WARNS?=		0
CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR}/cmd/sgs/include \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common

.include <bsd.prog.mk>
