# $FreeBSD$

PROG=	ttls2
LDADD+= -lpthread
LDADD+= -Wl,--rpath=${.CURDIR}/../../../../lib/libpthread
#LDADD+= -lthr
#LDADD+= -Wl,--rpath=${.CURDIR}/../../../../lib/libthr
LDADD+= -L${.CURDIR}/../../../../lib/libc -lc
LDADD+= -Wl,--rpath=${.CURDIR}/../../../../lib/libc
LDADD+= -Wl,--dynamic-linker=${.CURDIR}/../../../../libexec/rtld-elf/ld-elf.so.1
NOMAN=	t
DEBUG_FLAGS= -g

.include <bsd.prog.mk>
