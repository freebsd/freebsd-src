#	Makefile,v 1.1 1994/10/16 20:38:49 sos Exp

.PATH:	${.CURDIR}/../../sys/i386/ibcs2
KMOD=	socksys_mod
SRCS= 	socksys.c ibcs2_socksys.c
NOMAN=
CFLAGS+= -DLKM -I. -DCOMPAT_IBCS2

.include <bsd.kmod.mk>
