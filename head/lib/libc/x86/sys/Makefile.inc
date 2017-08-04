# $FreeBSD$

.PATH:	${LIBC_SRCTOP}/x86/sys

SRCS+= \
	__vdso_gettc.c

.if ${MACHINE_CPUARCH} == "amd64" && ${MK_HYPERV} != "no"
CFLAGS+=	-DWANT_HYPERV
.endif
