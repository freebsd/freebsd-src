# $FreeBSD: src/sys/modules/dtrace/Makefile,v 1.4.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

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
