#
#

SHLIBDIR?=	/lib

.include <bsd.own.mk>

PACKAGE=	lib${LIB}
LIB=		pjdlog
SRCS=		pjdlog.c

SHLIB_MAJOR=	0

CFLAGS+=-I${.CURDIR}

LIBADD=	util

.include <bsd.lib.mk>
