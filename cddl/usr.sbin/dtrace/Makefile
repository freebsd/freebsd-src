# $FreeBSD$

.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/cmd/dtrace

PROG=		dtrace
SRCS=		dtrace.c
BINDIR?=	/usr/sbin

WARNS?=		1

CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR}/head \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libdtrace/common \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libproc/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/compat

# Optional debugging stuff...
#CFLAGS+=	-DNEED_ERRLOC
#YFLAGS+=	-d

DPADD=		${LIBPTHREAD} ${LIBDTRACE} ${LIBY} ${LIBL} ${LIBPROC} ${LIBCTF} ${LIBELF} ${LIBZ}
LDADD=		-lpthread -ldtrace -ly -ll -lproc -lctf -lelf -lz

.include <bsd.prog.mk>
