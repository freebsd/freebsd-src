#	$Id: Makefile,v 1.9 1998/10/16 04:30:46 peter Exp $

.PATH:	${.CURDIR}/../../netinet
KMOD=	ipfw
SRCS=	ip_fw.c
NOMAN=
CFLAGS+= -DIPFIREWALL
#
#If you want it verbose
#CFLAGS+= -DIPFIREWALL_VERBOSE
#CFLAGS+= -DIPFIREWALL_VERBOSE_LIMIT=100
#
#If you want it to pass all packets by default
#CFLAGS+= -DIPFIREWALL_DEFAULT_TO_ACCEPT
#

.include <bsd.kmod.mk>
