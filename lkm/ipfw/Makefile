#	$Id: Makefile,v 1.4 1996/02/24 13:41:56 phk Exp $

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
