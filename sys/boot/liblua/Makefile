# $FreeBSD$

LUA_PATH=	${.CURDIR}/../../contrib/lua
.PATH:		${LUA_PATH}/src

LIB=		lua
INTERNALLIB=

# Core Lua.
SRCS=	lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c \
	lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c \
        ltm.c lundump.c lvm.c lzio.c

# Our utilities.
SRCS+=	lstd.c lutils.c

WARNS=	3

CFLAGS+= -I${.CURDIR} -DBOOT_LUA -ffreestanding -nostdlib -fno-stack-protector
CFLAGS+= -I${LUA_PATH}

.include <bsd.lib.mk>
