#	$Id: Makefile,v 1.5 1996/06/23 14:27:52 bde Exp $

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

.include <bsd.kmod.mk>
