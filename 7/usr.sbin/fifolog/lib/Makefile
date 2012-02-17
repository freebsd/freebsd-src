# $FreeBSD$

LIB =		fifolog
INTERNALLIB =	API not published or supported.

SRCS +=		fifolog_int.c
SRCS +=		fifolog_create.c
SRCS +=		fifolog_write_poll.c
SRCS +=		fifolog_reader.c
SRCS +=		getdate.y

CFLAGS +=	-I${.CURDIR}

.include <bsd.lib.mk>

test:
	echo ${HAS_EXPAND_NUMBER}
