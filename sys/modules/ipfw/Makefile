#	$Id: Makefile,v 1.8 1997/09/10 04:19:07 peter Exp $

.PATH:	${.CURDIR}/../../netinet
KMOD=	ipfw
SRCS=	ip_fw.c
NOMAN=
CFLAGS+= -DIPFIREWALL -DIPFIREWALL_MODULE
#
#If you want it verbose
#CFLAGS+= -DIPFIREWALL_VERBOSE
#CFLAGS+= -DIPFIREWALL_VERBOSE_LIMIT=100
#
#If you want it to pass all packets by default
#CFLAGS+= -DIPFIREWALL_DEFAULT_TO_ACCEPT
#

.include <bsd.kmod.mk>
