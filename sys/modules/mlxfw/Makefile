# $FreeBSD$
.PATH:  ${SRCTOP}/sys/dev/mlxfw

KMOD=mlxfw
SRCS= \
mlxfw_fsm.c \
mlxfw_mfa2.c \
mlxfw_mfa2_tlv_multi.c
SRCS+=	${LINUXKPI_GENSRCS}

CFLAGS+= \
	-I${SRCTOP}/sys/ofed/include \
	-I${SRCTOP}/sys/compat/linuxkpi/common/include \
	-I${SRCTOP}/sys/contrib/xz-embedded/freebsd \
	-I${SRCTOP}/sys/contrib/xz-embedded/linux/lib/xz

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
