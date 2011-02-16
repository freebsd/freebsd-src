# $FreeBSD: src/sys/modules/dtrace/Makefile,v 1.4.2.1.8.1 2010/12/21 17:10:29 kensmith Exp $

.include <bsd.own.mk>
.include "Makefile.inc"

SUBDIR=		dtmalloc	\
		dtrace		\
		dtraceall	\
		dtrace_test	\
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
