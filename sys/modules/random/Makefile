# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/randomdev
.PATH:	${.CURDIR}/../../crypto/blowfish
KMOD	= random
SRCS	= bus_if.h device_if.h randomdev.c yarrow.c hash.c
SRCS	+= bf_cbc.c bf_skey.c bf_enc.c
CFLAGS	+= -I${.CURDIR}/../..
NOMAN	= yes

.include <bsd.kmod.mk>
