# Makefile for building the sample syscall module

SRCS	= syscall.c
KMOD	= syscall
KO	= ${KMOD}.ko
KLDMOD	= t

KLDLOAD		= /sbin/kldload
KLDUNLOAD	= /sbin/kldunload

load: ${KO}
	${KLDLOAD} -v ./${KO}

unload: ${KO}
	${KLDUNLOAD} -v -n ${KO}

.include <bsd.kmod.mk>
