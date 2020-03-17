#! $FreeBSD$

.include <src.lua.mk>

LUASRC?=	${SRCTOP}/contrib/lua/src
.PATH: ${LUASRC}

PROG=	flua
WARNS?=	2
MAN=	# No manpage; this is internal.

CWARNFLAGS.gcc+=	-Wno-format-nonliteral

LIBADD=	lua

# Entry point
SRCS+=	lua.c

# FreeBSD Extensions
.PATH: ${.CURDIR}/modules
SRCS+=	linit_flua.c
SRCS+=	lfs.c lposix.c

CFLAGS+=	-I${SRCTOP}/lib/liblua -I${.CURDIR}/modules -I${LUASRC}
CFLAGS+=	-DLUA_PROGNAME="\"${PROG}\""

.include <bsd.prog.mk>
