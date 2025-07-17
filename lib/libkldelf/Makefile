.include <bsd.own.mk>

PACKAGE=	runtime
LIB=		kldelf
PRIVATELIB=	yes

SRCS=	ef.c \
	ef_obj.c \
	elf.c \
	ef_aarch64.c \
	ef_arm.c \
	ef_amd64.c \
	ef_i386.c \
	ef_mips.c \
	ef_powerpc.c \
	ef_riscv.c
WARNS?=	2

LIBADD=	elf

.include <bsd.lib.mk>
