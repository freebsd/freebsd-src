#	$Id: Makefile,v 1.2 1995/01/12 13:57:51 ugen Exp $

.PATH:	${.CURDIR}/../../sys/netinet
KMOD=	ipfw_mod
SRCS=	ipfw_lkm.c ip_fw.c
NOMAN=
CFLAGS+= -DIPFIREWALL  -DIPACCT
#
#If you want it verbose
#CFLAGS+= -DIPFIREWALL_VERBOSE
#

.include <bsd.kmod.mk>
