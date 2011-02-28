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

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
SUBDIR+=	fasttrap fbt
.endif

.include <bsd.subdir.mk>
