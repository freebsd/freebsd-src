# $FreeBSD$

.include	"../../Makefile.inc"

PROG=		lockstat

BINDIR?=	/usr/sbin

SRCS=		lockstat.c sym.c

WARNS=		1

CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR}/head \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libdtrace/common \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libproc/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/compat \
		-I${.CURDIR}/../../../sys

.PATH:		${OPENSOLARIS_USR_DISTDIR}/cmd/lockstat

CFLAGS+=	-DNEED_ERRLOC -g

#YFLAGS+=	-d

LDFLAGS+=	-pthread \
		-L${.OBJDIR}/../../lib/libdtrace \
		-L${.OBJDIR}/../../lib/libproc \
		-L${.OBJDIR}/../../lib/libctf \
		-L${.OBJDIR}/../../../lib/libelf

LDADD+=		-ldtrace -ly -ll -lproc -lctf -lelf -lz -lrt

#DPADD+= ${LIBDTRACE} ${LIBPTHREAD} ${LIBL} ${LIBY} ${LIBZ}

NO_MAN=

.include <bsd.prog.mk>
