.include <src.lua.mk>

PACKAGE=	flua

SUBDIR+=	libfreebsd
SUBDIR+=	libhash
SUBDIR+=	libjail
SUBDIR+=	libucl
SUBDIR+=	liblyaml

LUASRC?=	${SRCTOP}/contrib/lua/src
.PATH: ${LUASRC}

PROG=	flua
WARNS?=	3

CWARNFLAGS.gcc+=	-Wno-format-nonliteral

LIBADD=	lua

# Entry point
SRCS+=	lua.c

# FreeBSD Extensions
.PATH: ${.CURDIR}/modules
SRCS+=	linit_flua.c
SRCS+=	lfs.c lposix.c lfbsd.c

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

.include <bsd.prog.mk>
