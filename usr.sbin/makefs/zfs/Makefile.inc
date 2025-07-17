.PATH:	${SRCDIR}/zfs
.PATH:	${SRCTOP}/stand/libsa/zfs

SRCS+=	dsl.c \
	fs.c \
	objset.c \
	vdev.c \
	zap.c

SRCS+=	nvlist.c

CFLAGS.nvlist.c+= -I${SRCTOP}/stand/libsa -Wno-cast-qual
CWARNFLAGS.zap.c+= -Wno-sign-compare
