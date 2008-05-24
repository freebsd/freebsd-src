# $FreeBSD$

.include <bsd.own.mk>
.include "Makefile.inc"

SUBDIR=		dtmalloc	\
		dtrace		\
		dtraceall	\
		profile		\
		prototype	\
		sdt		\
		systrace

.if ${MACHINE_ARCH} == "amd64"
SUBDIR+=	fbt
.elif ${MACHINE_ARCH} == "i386"
SUBDIR+=	fbt
.endif

.include <bsd.subdir.mk>
