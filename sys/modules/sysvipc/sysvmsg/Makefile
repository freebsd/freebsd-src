# $FreeBSD$

.PATH:	${.CURDIR}/../../../kern
KMOD	= sysvmsg
SRCS	= sysv_msg.c opt_sysvipc.h

opt_sysvipc.h:
	touch $@

.include <bsd.kmod.mk>
