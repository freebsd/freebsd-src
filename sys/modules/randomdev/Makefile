# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/randomdev
.PATH:	${.CURDIR}/../../crypto/blowfish
#.PATH:	${.CURDIR}/../../crypto/des
KMOD	= randomdev
SRCS	= bus_if.h device_if.h randomdev.c yarrow.c
SRCS	+= bf_cbc.c bf_skey.c bf_enc.c
CFLAGS	+= -I${.CURDIR}/../../dev/randomdev
#SRCS	+= sha1.c des_setkey.c des_ecb.c
NOMAN	= yes

.include <bsd.kmod.mk>
