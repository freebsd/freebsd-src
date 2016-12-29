# $FreeBSD$

.include <bsd.own.mk>

HV_KVP_DAEMON_DISTDIR?= ${.CURDIR}/../../../../contrib/hyperv/tools
.PATH: ${HV_KVP_DAEMON_DISTDIR} 

PROG= 	hv_kvp_daemon
MAN= 	hv_kvp_daemon.8	

CFLAGS+=	-I${.CURDIR}/../../../../sys/dev/hyperv/utilities

.include <bsd.prog.mk>
