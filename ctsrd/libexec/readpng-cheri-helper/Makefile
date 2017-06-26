# $FreeBSD$

PROG=	readpng-cheri-helper
SRCS=	readpng-cheri.c				\
	execve.S				\
	sysarch.S

NEED_CHERI=	sandbox
WANT_DUMP=	yes

# don't know how to handle processor specific section `.preinit_array'
CHERI_LLD_BROKEN=yes

.PATH: ${SRCTOP}/ctsrd/lib/libimagebox
SRCS+=	decode_png.c

MAN=

LDFLAGS+=	-Wl,-T,${SRCTOP}/lib/libc_cheri/sandbox.ld \
		-nostdlib
CFLAGS+=	-I${SRCTOP}/ctsrd/lib/libimagebox
CFLAGS+=	-I${SRCTOP}/lib/libc/${MACHINE}
CFLAGS+=	-I${SRCTOP}/lib/libc/include
CFLAGS+=	-DIMAGEBOX_PNG_NO_FD
LIBADD+=	vuln_png c_cheri
# we need the png.h header but don't need to link against it
CFLAGS+=        -I${SRCTOP}/contrib/libpng
CFLAGS+=        -I${SRCTOP}/lib/libpng

NO_SHARED=	yes

WARNS=4

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <bsd.prog.mk>

CFLAGS+=	${CFLAGS.${.IMPSRC:T}}
CFLAGS.zutil.c=         -Wno-incompatible-pointer-types-discards-qualifiers
CFLAGS.inffast.c=	-Wno-cast-qual -O0
CFLAGS.inflate.c=	-Wno-cast-qual
