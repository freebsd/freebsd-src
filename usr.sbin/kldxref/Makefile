PACKAGE=	runtime
PROG=	kldxref
MAN=	kldxref.8
SRCS=	kldxref.c

CFLAGS+=-I${SRCTOP}/lib/libkldelf

WARNS?=	2

LIBADD=	elf

.if defined(BOOTSTRAPPING)
#
# XXX: Fix libprivatelibkldelf.a linker set issue before removing this block
#
.PATH: ${SRCTOP}/lib/libkldelf
KLDELF_SRCS=	ef.c \
		ef_obj.c \
		elf.c \
		ef_aarch64.c \
		ef_arm.c \
		ef_amd64.c \
		ef_i386.c \
		ef_mips.c \
		ef_powerpc.c \
		ef_riscv.c
SRCS+=	${KLDELF_SRCS}
.else
LIBADD+=	kldelf
.endif

.include <bsd.prog.mk>
