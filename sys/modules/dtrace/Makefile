# $FreeBSD$

.include <bsd.own.mk>
.include "Makefile.inc"

SUBDIR=		dtmalloc	\
		dtnfsclient	\
		dtrace		\
		dtraceall	\
		dtrace_test	\
		lockstat	\
		profile		\
		prototype	\
		sdt		\
		systrace

.if ${MACHINE_CPUARCH} == "amd64"
SUBDIR+=	fbt
.elif ${MACHINE_CPUARCH} == "i386"
SUBDIR+=	fbt
.endif

.include <bsd.subdir.mk>
