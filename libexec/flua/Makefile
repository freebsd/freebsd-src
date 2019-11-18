#! $FreeBSD$

.include <src.lua.mk>

LUASRC?=	${SRCTOP}/contrib/lua/src
.PATH: ${LUASRC}

PROG=	flua
WARNS?=	2
MAN=	# No manpage; this is internal.

LIBADD=	m

# Core functions
SRCS=	lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c \
	lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c \
	ltm.c lundump.c lvm.c lzio.c

# Library functions; any change to these likely needs an accompanying change
# in our custom linit_flua.c.  We use our custom linit.c to make it easier to
# support bootstrap flua that may not have supporting local libraries.
SRCS+=	lauxlib.c lbaselib.c lbitlib.c lcorolib.c ldblib.c liolib.c \
	lmathlib.c loslib.c lstrlib.c ltablib.c lutf8lib.c loadlib.c

# Entry point
SRCS+=	lua.c

# FreeBSD Extensions
.PATH: ${.CURDIR}/modules
SRCS+=	linit_flua.c
SRCS+=	lfs.c lposix.c

CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/modules -I${LUASRC}
CFLAGS+=	-DLUA_PROGNAME="\"${PROG}\""

.include <bsd.prog.mk>
