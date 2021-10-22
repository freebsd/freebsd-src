# $FreeBSD$

.PATH:	${LIBC_SRCTOP}/x86/sys

SRCS+= \
	__vdso_gettc.c \
	pkru.c \
	sched_getcpu_gen.c

MAN+=	\
	pkru.3

.if ${MACHINE_CPUARCH} == "amd64" && ${MK_HYPERV} != "no"
CFLAGS+=	-DWANT_HYPERV
.endif
