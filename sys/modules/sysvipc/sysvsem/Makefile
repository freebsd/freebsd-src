# $FreeBSD$

.PATH:	${.CURDIR}/../../../kern
KMOD	= sysvsem
SRCS	= sysv_sem.c opt_sysvipc.h

opt_sysvipc.h:
	touch $@

.include <bsd.kmod.mk>
