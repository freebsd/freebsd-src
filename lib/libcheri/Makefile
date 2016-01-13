# $FreeBSD$

LIB=	cheri

# XXXRW: When compiling for CheriABI, will need to use cheri_invoke_cabi.S
# here.
SRCS=	cheri_enter.c			\
	cheri_fd.c			\
	cheri_stack.c			\
	cheri_system.c			\
	cheri_type.c			\
	libcheri_classes.S		\
	sandbox.c			\
	sandbox_elf.c			\
	sandbox_loader.c		\
	sandbox_methods.c		\

.if defined(LIBCHERI)
SRCS+=	cheri_invoke_cabi.S
.else
SRCS+=	cheri_invoke_hybrid.S
.endif

INCS=	cheri_class.h			\
	cheri_enter.h			\
	cheri_fd.h			\
	cheri_invoke.h			\
	cheri_memcpy.h			\
	cheri_stack.h			\
	cheri_system.h			\
	cheri_type.h			\
	sandbox.h			\
	sandboxasm.h

CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/${MACHINE_ARCH}

INCSDIR=	${INCLUDEDIR}/cheri

CFLAGS+=	-I${.CURDIR}/../libc/include \
		-I${.CURDIR}/../libc/locale

SHLIB_MAJOR=	1

MAN=	cheri_invoke.3			\
	libcheri.3

.if !defined(NEED_CHERI)
NEED_CHERI=	hybrid
.endif

CLEANFILES=	cheri_classes.o cheri_invoke.o

NO_WERROR=

.PATH: ${.CURDIR}/${MACHINE_ARCH}
.PATH: ${.CURDIR}/../libc/string
.PATH: ${.CURDIR}/../libc/${MACHINE}/string

.include <bsd.lib.mk>
