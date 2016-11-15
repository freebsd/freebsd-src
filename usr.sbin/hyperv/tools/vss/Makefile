# $FreeBSD$
DIRDEPS = lib/libc

.include <bsd.own.mk>

HV_VSS_DAEMON_DISTDIR?= ${.CURDIR}/../../../../contrib/hyperv/tools
.PATH: ${HV_VSS_DAEMON_DISTDIR} 

PROG= 	hv_vss_daemon
MAN= 	hv_vss_daemon.8	

CFLAGS+=	-I${.CURDIR}/../../../../sys/dev/hyperv/utilities

.include <bsd.prog.mk>
