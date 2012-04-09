#
# $FreeBSD: src/tools/tools/ether_reflect/Makefile,v 1.1.2.2.2.1 2012/03/03 06:15:13 kensmith Exp $
#
# A Makefile that builds both the ether_reflect program and its manual page.

PROG=   ether_reflect
CFLAGS+=-DHAVE_SNPRINTF

LDADD+= -lpcap

.include <bsd.prog.mk>
