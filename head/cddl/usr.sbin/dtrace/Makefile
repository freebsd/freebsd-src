# $FreeBSD$

.include	"../../Makefile.inc"

PROG=		dtrace

BINDIR?=	/usr/sbin

SRCS=		dtrace.c

WARNS=		1

CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR}/head \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libdtrace/common \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libproc/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/compat

.PATH:		${OPENSOLARIS_USR_DISTDIR}/cmd/dtrace

# Optional debugging stuff...
#CFLAGS+=	-DNEED_ERRLOC
#YFLAGS+=	-d

LDFLAGS+=	-pthread \
		-L${.OBJDIR}/../../lib/libdtrace \
		-L${.OBJDIR}/../../lib/libproc \
		-L${.OBJDIR}/../../lib/libctf \
		-L${.OBJDIR}/../../../lib/libelf

LDADD+=		-ldtrace -ly -ll -lproc -lctf -lelf -lz

DPADD+= ${LIBDTRACE} ${LIBCTF} ${LIBELF} ${LIBPTHREAD} ${LIBL} ${LIBY} ${LIBZ}

.include <bsd.prog.mk>
