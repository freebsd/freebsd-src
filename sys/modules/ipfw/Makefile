# $FreeBSD$

.PATH:	${.CURDIR}/../../netinet
KMOD=	ipfw
NOMAN=
CFLAGS+= -DIPFIREWALL
.if defined(IPFW2)
SRCS=	ip_fw2.c
CFLAGS+= -DIPFW2
.else
SRCS=	ip_fw.c
.endif
#
#If you want it verbose
#CFLAGS+= -DIPFIREWALL_VERBOSE
#CFLAGS+= -DIPFIREWALL_VERBOSE_LIMIT=100
#
#If you want it to pass all packets by default
#CFLAGS+= -DIPFIREWALL_DEFAULT_TO_ACCEPT
#

.include <bsd.kmod.mk>
