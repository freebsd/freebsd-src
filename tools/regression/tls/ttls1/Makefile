# $FreeBSD$

PROG=	ttls1
LDADD+=	-L../libxx -lxx -Wl,--rpath=${.CURDIR}/../libxx
LDADD+=	-L../libyy -lyy -Wl,--rpath=${.CURDIR}/../libyy
LDADD+= -L${.CURDIR}/../../../../lib/libc -lc
LDADD+= -Wl,--rpath=${.CURDIR}/../../../../lib/libc
LDADD+= -Wl,--dynamic-linker=${.CURDIR}/../../../../libexec/rtld-elf/ld-elf.so.1
NOMAN=	t
DEBUG_FLAGS= -g

.include <bsd.prog.mk>
