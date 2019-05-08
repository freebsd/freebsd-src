# $FreeBSD$
.PATH:  ${SRCTOP}/sys/dev/mlxfw

KMOD=mlxfw
SRCS= \
mlxfw_fsm.c \
mlxfw_mfa2.c \
mlxfw_mfa2_tlv_multi.c \
device_if.h bus_if.h vnode_if.h pci_if.h

CFLAGS+= \
	-I${SRCTOP}/sys/ofed/include \
	-I${SRCTOP}/sys/compat/linuxkpi/common/include \
	-I${SRCTOP}/sys/contrib/xz-embedded/freebsd \
	-I${SRCTOP}/sys/contrib/xz-embedded/linux/lib/xz

.include <bsd.kmod.mk>
