# $FreeBSD$

.PATH:	${.CURDIR}/../../../kern
KMOD	= sysvshm
OPTS	= opt_sysvipc.h opt_compat.h opt_rlimit.h
SRCS	= sysv_shm.c $(OPTS)

$(OPTS):
	touch $@

.include <bsd.kmod.mk>
