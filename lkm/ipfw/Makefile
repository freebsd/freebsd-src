#	$Id: Makefile,v 1.5.2.1 1998/03/06 05:26:59 jkh Exp $

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
