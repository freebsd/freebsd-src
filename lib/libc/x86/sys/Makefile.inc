# $FreeBSD$

.PATH:	${LIBC_SRCTOP}/x86/sys

SRCS+= \
	__vdso_gettc.c

.if ${MK_HYPERV} != "no"
CFLAGS+=	-DWANT_HYPERV
.endif
