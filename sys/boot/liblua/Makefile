# $FreeBSD$

LUA_PATH?=	${.CURDIR}/../../contrib/lua
.PATH:		${LUA_PATH}/src

LIBLUA_PATH?=	${.CURDIR}/../liblua
.PATH:		${LIBLUA_PATH}

COMMON_PATH?=	${.CURDIR}/../common

LIB=		lua
INTERNALLIB=

# Core Lua.
SRCS=	lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c \
	lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c \
        ltm.c lundump.c lvm.c lzio.c
SRCS+=	lauxlib.c lbaselib.c lbitlib.c ldblib.c lstrlib.c ltablib.c loadlib.c \
	linit.c

# Our utilities.
SRCS+=	lstd.c lutils.c

WARNS=	3

CFLAGS+= -ffreestanding -nostdlib
CFLAGS+= -fno-stack-protector -D__BSD_VISIBLE
CFLAGS+= -I${LIBLUA_PATH} -I${LUA_PATH} -I${COMMON_PATH}

.if ${MACHINE_CPUARCH} == "i386" || \
    (${MACHINE_CPUARCH} == "amd64" && defined(LUA32))
CFLAGS+=        -march=i386
CFLAGS.gcc+=    -mpreferred-stack-boundary=2
.endif
.if ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=        -mno-red-zone
.endif
.if ${MACHINE_CPUARCH} == "i386"
CFLAGS+=        -mno-mmx -mno-3dnow -mno-sse -mno-sse2 -mno-sse3 -msoft-float
.endif

.if ${MACHINE_CPUARCH} == "amd64"
.if defined(LUA32)
CFLAGS+=        -m32
.else
CFLAGS+=        -fPIC
.endif
.endif

.include <bsd.lib.mk>
