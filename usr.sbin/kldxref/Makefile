PACKAGE=	runtime
PROG=	kldxref
MAN=	kldxref.8
SRCS=	kldxref.c ef.c ef_obj.c elf.c
SRCS+=	ef_aarch64.c \
	ef_arm.c \
	ef_amd64.c \
	ef_i386.c \
	ef_mips.c \
	ef_powerpc.c \
	ef_riscv.c

WARNS?=	2

LIBADD=	elf

.include <bsd.prog.mk>
