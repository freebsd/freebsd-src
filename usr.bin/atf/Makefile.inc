# $FreeBSD$

ATF=		${.CURDIR}/../../../contrib/atf

CFLAGS+=	-DGDB=\"gdb\"

BINDIR?=	/usr/bin
WARNS?=		3
