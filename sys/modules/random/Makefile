# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/random
.PATH:	${.CURDIR}/../../crypto/blowfish
KMOD	= random
SRCS	= bus_if.h device_if.h randomdev.c yarrow.c hash.c
SRCS	+= bf_cbc.c bf_skey.c bf_enc.c
SRCS	+= vnode_if.h opt_noblockrandom.h
CFLAGS	+= -I${.CURDIR}/../..
NOMAN	= yes

NOBLOCKRANDOM=	yes
.if defined(NOBLOCKRANDOM)
RANDOM_OPTS=     "\#define NOBLOCKRANDOM"
.endif

opt_noblockrandom.h:
	touch opt_noblockrandom.h
	echo $(RANDOM_OPTS) >> opt_noblockrandom.h

.include <bsd.kmod.mk>
