#	$Id$

.PATH:	${.CURDIR}/../../sys/i386/ibcs2
KMOD=	socksys_mod
SRCS= 	socksys.c ibcs2_socksys.c
NOMAN=
CFLAGS+= -DLKM -I. -DCOMPAT_IBCS2

.include <bsd.kmod.mk>
