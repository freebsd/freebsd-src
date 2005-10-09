# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/rc

KMOD=	rc
SRCS=	rc.c device_if.h bus_if.h isa_if.h opt_tty.h

opt_tty.h:
	echo "#define TTYHOG 8192" >opt_tty.h

.include <bsd.kmod.mk>
