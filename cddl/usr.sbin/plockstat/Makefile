# $FreeBSD$

.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/cmd/plockstat

PROG=		plockstat
SRCS=		plockstat.c 
BINDIR?=	/usr/sbin

WARNS?=		1

CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR}/head \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libdtrace/common \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libproc/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/compat \
		-I${.CURDIR}/../../../cddl/lib/libdtrace \
		-I${.CURDIR}/../../../sys

DPADD=		${LIBPTHREAD} ${LIBDTRACE} ${LIBY} ${LIBL} ${LIBPROC} ${LIBCTF}\
		${LIBELF} ${LIBZ} ${LIBRT} ${LIBRTLD_DB} ${LIBUTIL}
LDADD=		-lpthread -ldtrace -ly -ll -lproc -lctf -lelf -lz -lrt \
		-lrtld_db -lutil

.include <bsd.prog.mk>
