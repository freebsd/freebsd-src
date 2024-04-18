_spath=	${SRCTOP}/contrib/ofed/libirdma
_ipath=	${SRCTOP}/contrib/ofed/include
lin_inc=/usr/src/sys/compat/linuxkpi/common/include
.PATH: ${_spath}

SHLIBDIR?=	/lib
PACKAGE=	lib${LIB}
LIB=		irdma
SHLIB_MAJOR=	1
MK_PROFILE=	no

SRCS= \
irdma_umain.c \
irdma_uverbs.c \
irdma_uk.c \

LIBADD=		ibverbs pthread
CFLAGS+=	-I${_spath} -I${SRCTOP}/contrib/ofed/libibverbs
VERSION_MAP=	${_spath}/libirdma.map

.include <bsd.lib.mk>
