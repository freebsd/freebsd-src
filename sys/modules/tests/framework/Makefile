#
# $FreeBSD$
#

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}

.PATH: ${.CURDIR}/../../../tests/framework

KMOD=	kern_testfrwk
SRCS=	kern_testfrwk.c
SRCS+=	bus_if.h device_if.h

#
# Enable full debugging
#
#CFLAGS += -g

.include <bsd.kmod.mk>
