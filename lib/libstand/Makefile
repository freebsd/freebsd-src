# $FreeBSD$
# Originally from	$NetBSD: Makefile,v 1.21 1997/10/26 22:08:38 lukem Exp $
#
# Notes:
# - We don't use the libc strerror/sys_errlist because the string table is
#   quite large.
#

LIB=		stand
NO_PROFILE=
NO_PIC=
INCS=		stand.h
MAN=		libstand.3

WITHOUT_SSP=
CFLAGS+= -ffreestanding -Wformat
CFLAGS+= -I${.CURDIR}

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
CFLAGS+=	-mpreferred-stack-boundary=2
CFLAGS+=	-mno-mmx -mno-3dnow -mno-sse -mno-sse2
.endif
.if ${MACHINE_ARCH} == "i386"
CFLAGS+=	-mno-sse3
.endif
.if ${MACHINE} == "pc98"
CFLAGS+=	-Os
.endif
.if ${MACHINE_ARCH} == "powerpc"
CFLAGS+=	-msoft-float -D_STANDALONE
.endif
.if ${MACHINE_ARCH} == "amd64"
CFLAGS+=	-m32 -I.
.endif
.if ${MACHINE_ARCH} == "arm"
CFLAGS+=	-msoft-float -DSOFTFLOAT -D_STANDALONE
.endif

# standalone components and stuff we have modified locally
SRCS+=	zutil.h __main.c assert.c bcd.c bswap.c environment.c getopt.c gets.c \
	globals.c pager.c printf.c strdup.c strerror.c strtol.c random.c \
	sbrk.c twiddle.c zalloc.c zalloc_malloc.c

# private (pruned) versions of libc string functions
SRCS+=	strcasecmp.c

.PATH: ${.CURDIR}/../libc/net

SRCS+= ntoh.c

# string functions from libc
.PATH: ${.CURDIR}/../libc/string
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "powerpc" || \
	${MACHINE_ARCH} == "sparc64" || ${MACHINE_ARCH} == "amd64" || \
	${MACHINE_ARCH} == "arm"
SRCS+=	bcmp.c bcopy.c bzero.c ffs.c index.c memccpy.c memchr.c memcmp.c \
        memcpy.c memmove.c memset.c qdivrem.c rindex.c strcat.c strchr.c \
        strcmp.c strcpy.c strcspn.c strlen.c strncat.c strncmp.c strncpy.c \
	strpbrk.c strrchr.c strsep.c strspn.c strstr.c strtok.c swab.c
.endif
.if ${MACHINE_ARCH} == "arm"
.PATH: ${.CURDIR}/../libc/arm/gen
SRCS+= divsi3.S
.endif
.if ${MACHINE_ARCH} == "ia64"
.PATH: ${.CURDIR}/../libc/ia64/string
SRCS+=	bcmp.c bcopy.S bzero.S ffs.S index.c memccpy.c memchr.c memcmp.c \
	memcpy.S memmove.S memset.c rindex.c strcat.c strchr.c \
	strcmp.c strcpy.c strcspn.c strlen.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strrchr.c strsep.c \
	strspn.c strstr.c strtok.c swab.c

.PATH: ${.CURDIR}/../libc/ia64/gen
SRCS+= __divdi3.S __divsi3.S __moddi3.S __modsi3.S
SRCS+= __udivdi3.S __udivsi3.S __umoddi3.S __umodsi3.S
.endif
.if ${MACHINE_ARCH} == "powerpc"
.PATH: ${.CURDIR}/../libc/quad
SRCS+=	ashldi3.c ashrdi3.c
.PATH: ${.CURDIR}/../libc/powerpc/gen
SRCS+=	syncicache.c
.endif

# uuid functions from libc
.PATH: ${.CURDIR}/../libc/uuid
SRCS+= uuid_equal.c uuid_is_nil.c

# _setjmp/_longjmp
.if ${MACHINE_ARCH} == "amd64"
.PATH: ${.CURDIR}/i386
.else
.PATH: ${.CURDIR}/${MACHINE_ARCH}
.endif
SRCS+=	_setjmp.S

# decompression functionality from libbz2
# NOTE: to actually test this functionality after libbz2 upgrade compile
# loader(8) with LOADER_BZIP2_SUPPORT defined
.PATH: ${.CURDIR}/../../contrib/bzip2
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
.PATH: ${.CURDIR}/../libz
CFLAGS+=-DHAVE_MEMCPY -I${.CURDIR}/../libz
SRCS+=	adler32.c crc32.c libstand_zutil.h

.for file in infback.c inffast.c inflate.c inftrees.c zutil.c
SRCS+=	_${file}
CLEANFILES+=	_${file}

_${file}: ${file}
	sed "s|zutil\.h|libstand_zutil.h|" ${.ALLSRC} > ${.TARGET}
.endfor

# depend on stand.h being able to be included multiple times
CLEANFILES+= libstand_zutil.h
libstand_zutil.h: zutil.h
	sed -e 's|<stddef.h>|"stand.h"|' \
	    -e 's|<string.h>|"stand.h"|' \
	    -e 's|<stdlib.h>|"stand.h"|' \
	    ${.ALLSRC} > ${.TARGET}

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

.if ${MACHINE_ARCH} == "amd64"
beforedepend ${OBJS}: machine
cleandepend: cleanmachine
cleanmachine:
	rm -f machine

machine:
	ln -s ${.CURDIR}/../../sys/i386/include machine
.endif
