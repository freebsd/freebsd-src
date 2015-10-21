# $FreeBSD: stable/9/lib/libdevinfo/Makefile 201381 2010-01-02 09:58:07Z ed $

LIB_CXX=	devdctl
INCS=	consumer.h		\
	event.h			\
	event_factory.h		\
	exception.h		\
	guid.h
SRCS=	consumer.cc		\
	event.cc		\
	event_factory.cc	\
	exception.cc		\
	guid.cc

INCSDIR= ${INCLUDEDIR}/devctl

WARNS?=	3
SHLIB_MAJOR=	0

.include <bsd.lib.mk>
