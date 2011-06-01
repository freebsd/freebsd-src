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
SUBDIR+=	fasttrap fbt systrace_linux32
.endif
.if ${MACHINE_ARCH} == "amd64"
SUBDIR+=	systrace_freebsd32
.endif

.include <bsd.subdir.mk>
