# Makefile to build LCD control software for ppi(4) interface.
#
# $FreeBSD$

PROG=	ppilcd
NOMAN=	YES

# XXX This should not be done in fact. The header files should
# be copied to /usr/include/sys/dev/ppbus during make world
CFLAGS += -I/sys/dev/ppbus

.include <bsd.prog.mk>
