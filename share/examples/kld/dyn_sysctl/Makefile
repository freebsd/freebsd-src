#	$FreeBSD$

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
