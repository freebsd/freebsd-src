#	$Id: Makefile,v 1.7 1997/02/22 12:48:14 peter Exp $

.PATH:	${.CURDIR}/../../sys/netinet
KMOD=	ipfw_mod
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
