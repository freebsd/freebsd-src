#	$FreeBSD: src/share/examples/kld/dyn_sysctl/Makefile,v 1.1.2.1 2000/09/25 12:09:19 ps Exp $

SRCS	= dyn_sysctl.c
CFLAGS	= -I/sys
KMOD	= dyn_sysctl
KO	= ${KMOD}.ko
KLDMOD	= t

KLDLOAD		= /sbin/kldload
KLDUNLOAD	= /sbin/kldunload

load: ${KO}
	${KLDLOAD} -v ./${KO}

unload: ${KO}
	${KLDUNLOAD} -v -n ${KO}

.include <bsd.kmod.mk>
