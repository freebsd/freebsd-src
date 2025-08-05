.include <src.opts.mk>

PACKAGE=runtime
PROG=	newfs_msdos
MAN=	newfs_msdos.8
SRCS=	newfs_msdos.c mkfs_msdos.c

LINKS=  ${BINDIR}/newfs_msdos ${BINDIR}/newfs_exfat
MLINKS=	newfs_msdos.8 newfs_exfat.8

.if ${MK_ICONV} == "yes"
CFLAGS+=-DWITH_ICONV
.endif

# XXX - this is verboten
.if ${MACHINE_CPUARCH} == "arm"
WARNS?= 3
.endif

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
