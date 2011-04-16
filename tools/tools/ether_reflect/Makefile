#
# $FreeBSD$
#
# A Makefile that builds both the ether_reflect program and its manual page.

PROG=   ether_reflect
CFLAGS+=-DHAVE_SNPRINTF

LDADD+= -lpcap

.include <bsd.prog.mk>
