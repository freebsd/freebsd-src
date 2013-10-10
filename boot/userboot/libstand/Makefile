# $FreeBSD$
# Originally from	$NetBSD: Makefile,v 1.21 1997/10/26 22:08:38 lukem Exp $
#
# Notes:
# - We don't use the libc strerror/sys_errlist because the string table is
#   quite large.
#

NO_MAN=

.include <bsd.own.mk>
MK_SSP=		no

S=		${.CURDIR}/../../../../lib/libstand

.PATH:		${S}
LIB=		stand
INTERNALLIB=
NO_PROFILE=
NO_PIC=

WARNS?=		0

CFLAGS+= -ffreestanding -Wformat -fPIC
CFLAGS+= -I${.CURDIR}/../../../../lib/libstand

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=	-mno-mmx -mno-3dnow -mno-sse -mno-sse2
.endif
.if ${MACHINE_CPUARCH} == "i386"
CFLAGS+=	-mpreferred-stack-boundary=2
CFLAGS+=	-mno-sse3
.endif
.if ${MACHINE} == "pc98"
CFLAGS+=	-Os
.endif
.if ${MACHINE_CPUARCH} == "powerpc"
CFLAGS+=	-msoft-float -D_STANDALONE -DNETIF_DEBUG
.endif
.if ${MACHINE_CPUARCH} == "arm"
CFLAGS+=	-msoft-float -D_STANDALONE
.endif

# standalone components and stuff we have modified locally
SRCS+=	gzguts.h zutil.h __main.c assert.c bcd.c bswap.c environment.c getopt.c gets.c \
	globals.c pager.c printf.c strdup.c strerror.c strtol.c random.c \
	sbrk.c twiddle.c zalloc.c zalloc_malloc.c

# private (pruned) versions of libc string functions
SRCS+=	strcasecmp.c

LIBC=	${.CURDIR}/../../../../lib/libc

.PATH: ${LIBC}/net

SRCS+= ntoh.c

# string functions from libc
.PATH: ${LIBC}/string
.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "powerpc" || \
	${MACHINE_CPUARCH} == "sparc64" || ${MACHINE_CPUARCH} == "amd64" || \
	${MACHINE_CPUARCH} == "arm"
SRCS+=	bcmp.c bcopy.c bzero.c ffs.c memccpy.c memchr.c memcmp.c memcpy.c \
	memmove.c memset.c qdivrem.c strcat.c strchr.c strcmp.c strcpy.c \
	strcspn.c strlen.c strncat.c strncmp.c strncpy.c strpbrk.c \
	strrchr.c strsep.c strspn.c strstr.c strtok.c swab.c
.endif
.if ${MACHINE_CPUARCH} == "arm"
.PATH: ${LIBC}/arm/gen
SRCS+= divsi3.S
.endif
.if ${MACHINE_CPUARCH} == "ia64"
.PATH: ${LIBC}/ia64/string
SRCS+=	bcmp.c bcopy.S bzero.S ffs.S index.c memccpy.c memchr.c memcmp.c \
	memcpy.S memmove.S memset.c rindex.c strcat.c strchr.c \
	strcmp.c strcpy.c strcspn.c strlen.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strrchr.c strsep.c \
	strspn.c strstr.c strtok.c swab.c

.PATH: ${LIBC}/ia64/gen
SRCS+= __divdi3.S __divsi3.S __moddi3.S __modsi3.S
SRCS+= __udivdi3.S __udivsi3.S __umoddi3.S __umodsi3.S
.endif
.if ${MACHINE_CPUARCH} == "powerpc"
.PATH: ${LIBC}/libc/quad
SRCS+=	ashldi3.c ashrdi3.c
.PATH: ${LIBC}/powerpc/gen
SRCS+=	syncicache.c
.endif

# uuid functions from libc
.PATH: ${LIBC}/uuid
SRCS+= uuid_equal.c uuid_is_nil.c

# _setjmp/_longjmp
.if ${MACHINE_CPUARCH} == "amd64"
.PATH: ${S}/amd64
.elif ${MACHINE_ARCH} == "powerpc64"
.PATH: ${S}/powerpc
.else
.PATH: ${S}/${MACHINE_CPUARCH}
.endif
SRCS+=	_setjmp.S

# decompression functionality from libbz2
# NOTE: to actually test this functionality after libbz2 upgrade compile
# loader(8) with LOADER_BZIP2_SUPPORT defined
.PATH: ${.CURDIR}/../../../../contrib/bzip2
CFLAGS+= -DBZ_NO_STDIO -DBZ_NO_COMPRESS
SRCS+=	libstand_bzlib_private.h

.for file in bzlib.c crctable.c decompress.c huffman.c randtable.c
SRCS+=	_${file}
CLEANFILES+=	_${file}

_${file}: ${file}
	sed "s|bzlib_private\.h|libstand_bzlib_private.h|" ${.ALLSRC} > ${.TARGET}
.endfor

CLEANFILES+= libstand_bzlib_private.h
libstand_bzlib_private.h: bzlib_private.h
	sed -e 's|<stdlib.h>|"stand.h"|' \
		${.ALLSRC} > ${.TARGET}

# decompression functionality from libz
.PATH: ${.CURDIR}/../../../../lib/libz
CFLAGS+=-DHAVE_MEMCPY -I${.CURDIR}/../../../../lib/libz
SRCS+=	adler32.c crc32.c libstand_zutil.h libstand_gzguts.h

.for file in infback.c inffast.c inflate.c inftrees.c zutil.c
SRCS+=	_${file}
CLEANFILES+=	_${file}

_${file}: ${file}
	sed -e "s|zutil\.h|libstand_zutil.h|" \
	    -e "s|gzguts\.h|libstand_gzguts.h|" ${.ALLSRC} > ${.TARGET}
.endfor

# depend on stand.h being able to be included multiple times
.for file in zutil.h gzguts.h
CLEANFILES+= libstand_${file}
libstand_${file}: ${file}
	sed -e 's|<fcntl.h>|"stand.h"|' \
	    -e 's|<stddef.h>|"stand.h"|' \
	    -e 's|<string.h>|"stand.h"|' \
	    -e 's|<stdio.h>|"stand.h"|' \
	    -e 's|<stdlib.h>|"stand.h"|' \
	    ${.ALLSRC} > ${.TARGET}
.endfor

# io routines
SRCS+=	closeall.c dev.c ioctl.c nullfs.c stat.c \
	fstat.c close.c lseek.c open.c read.c write.c readdir.c

# network routines
SRCS+=	arp.c ether.c inet_ntoa.c in_cksum.c net.c udp.c netif.c rpc.c

# network info services:
SRCS+=	bootp.c rarp.c bootparam.c

# boot filesystems
SRCS+=	ufs.c nfs.c cd9660.c tftp.c gzipfs.c bzipfs.c
SRCS+=	dosfs.c ext2fs.c
SRCS+=	splitfs.c

.include <bsd.lib.mk>
