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

# readline bits; these aren't needed if we're building a bootstrap flua, as we
# don't expect that one to see any REPL usage.
.if !defined(BOOTSTRAPPING)
CFLAGS+=	-DLUA_USE_READLINE
CFLAGS+=	-I${SRCTOP}/lib/libedit -I${SRCTOP}/contrib/libedit
LIBADD+=	edit
LDFLAGS+=	-Wl,-E
.endif

UCLSRC?=	${SRCTOP}/contrib/libucl
.PATH: ${UCLSRC}/lua
SRCS+=	lua_ucl.c
CFLAGS+=	-I${UCLSRC}/include -I${UCLSRC}/src -I${UCLSRC}/uthash
LIBADD+=	ucl

.include <bsd.prog.mk>
