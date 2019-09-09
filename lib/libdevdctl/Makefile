# $FreeBSD$

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

INCSDIR= ${INCLUDEDIR}/devdctl

WARNS?=	3
PRIVATELIB=	true
SHLIB_MAJOR=	0

# Silence gcc warnings about deprecated std::auto_ptr, and various function
# objects from <functional>.
CWARNFLAGS+=	-Wno-deprecated-declarations

.include <bsd.lib.mk>
