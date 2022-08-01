# $FreeBSD$

KMOD=	wtap

.PATH:	${SRCTOP}/sys/dev/wtap
SRCS=	if_wtap_module.c if_wtap.c if_medium.c

.PATH:	${SRCTOP}/sys/dev/wtap/wtap_hal
SRCS+=	hal.c

.PATH:	${SRCTOP}/sys/dev/wtap/plugins
SRCS+=	visibility.c

SRCS+=	opt_wlan.h opt_global.h

.if !defined(KERNBUILDDIR)
.if defined(VIMAGE)
opt_global.h:
	echo "#define VIMAGE 1" >> ${.TARGET}
CFLAGS+=	-include opt_global.h
.endif
.endif

.include <bsd.kmod.mk>
