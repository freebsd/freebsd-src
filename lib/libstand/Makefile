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

CFLAGS+= -ffreestanding -Wformat
CFLAGS+= -I${.CURDIR}

.if ${MACHINE_ARCH} == "alpha"
CFLAGS+=	-mno-fp-regs
.endif
.if ${MACHINE_ARCH} == "i386"
CFLAGS+=	-mpreferred-stack-boundary=2
.endif
.if ${MACHINE_ARCH} == "powerpc"
CFLAGS+=	-msoft-float -D_STANDALONE
.endif
.if ${MACHINE_ARCH} == "amd64"
CFLAGS+=	-m32
.endif

# standalone components and stuff we have modified locally
SRCS+=	zutil.h __main.c assert.c bcd.c bswap.c environment.c getopt.c gets.c \
	globals.c pager.c printf.c strdup.c strerror.c strtol.c random.c \
	sbrk.c twiddle.c zalloc.c zalloc_malloc.c

# private (pruned) versions of libc string functions
SRCS+=	strcasecmp.c

# byte order functions from libc
.if ${MACHINE_ARCH} != "amd64"
.PATH: ${.CURDIR}/../libc/${MACHINE_ARCH}/net
.else
.PATH: ${.CURDIR}/../libc/i386/net
.endif
SRCS+= htons.S ntohs.S htonl.S ntohl.S

# string functions from libc
.PATH: ${.CURDIR}/../libc/string
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "powerpc" || \
	${MACHINE_ARCH} == "sparc64" || ${MACHINE_ARCH} == "amd64"
SRCS+=	bcmp.c bcopy.c bzero.c ffs.c index.c memccpy.c memchr.c memcmp.c \
        memcpy.c memmove.c memset.c qdivrem.c rindex.c strcat.c strchr.c \
        strcmp.c strcpy.c strcspn.c strlen.c strncat.c strncmp.c strncpy.c \
	strpbrk.c strrchr.c strsep.c strspn.c strstr.c strtok.c swab.c
.endif
.if ${MACHINE_ARCH} == "alpha"
.PATH: ${.CURDIR}/../libc/alpha/string
SRCS+=	bcmp.c bcopy.S bzero.S ffs.S index.c memccpy.c memchr.c memcmp.c \
	memcpy.S memmove.S memset.c rindex.c strcat.c strchr.c \
	strcmp.c strcpy.c strcspn.c strlen.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strrchr.c strsep.c \
	strspn.c strstr.c strtok.c swab.c

SRCS+= __divqu.S __divq.S __divlu.S __divl.S
SRCS+= __remqu.S __remq.S __remlu.S __reml.S

CLEANFILES+=   __divqu.S __divq.S __divlu.S __divl.S
CLEANFILES+=   __remqu.S __remq.S __remlu.S __reml.S


__divqu.S: ${.CURDIR}/../libc/alpha/gen/divrem.m4
	m4 -DNAME=__divqu -DOP=div -DS=false -DWORDSIZE=64 \
		${.ALLSRC} > ${.TARGET}

__divq.S: ${.CURDIR}/../libc/alpha/gen/divrem.m4
	m4 -DNAME=__divq -DOP=div -DS=true -DWORDSIZE=64 \
		${.ALLSRC} > ${.TARGET}

__divlu.S: ${.CURDIR}/../libc/alpha/gen/divrem.m4
	m4 -DNAME=__divlu -DOP=div -DS=false -DWORDSIZE=32 \
		${.ALLSRC} > ${.TARGET}

__divl.S: ${.CURDIR}/../libc/alpha/gen/divrem.m4
	m4 -DNAME=__divl -DOP=div -DS=true -DWORDSIZE=32 \
		${.ALLSRC} > ${.TARGET}

__remqu.S: ${.CURDIR}/../libc/alpha/gen/divrem.m4
	m4 -DNAME=__remqu -DOP=rem -DS=false -DWORDSIZE=64 \
		${.ALLSRC} > ${.TARGET}

__remq.S: ${.CURDIR}/../libc/alpha/gen/divrem.m4
	m4 -DNAME=__remq -DOP=rem -DS=true -DWORDSIZE=64 \
		${.ALLSRC} > ${.TARGET}

__remlu.S: ${.CURDIR}/../libc/alpha/gen/divrem.m4
	m4 -DNAME=__remlu -DOP=rem -DS=false -DWORDSIZE=32 \
		${.ALLSRC} > ${.TARGET}

__reml.S: ${.CURDIR}/../libc/alpha/gen/divrem.m4
	m4 -DNAME=__reml -DOP=rem -DS=true -DWORDSIZE=32 \
		${.ALLSRC} > ${.TARGET}
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

# _setjmp/_longjmp
.if ${MACHINE_ARCH} == "amd64"
.PATH: ${.CURDIR}/i386
.else
.PATH: ${.CURDIR}/${MACHINE_ARCH}
.endif
SRCS+=	_setjmp.S

# decompression functionality from libz
.PATH: ${.CURDIR}/../../contrib/bzip2
CFLAGS+= -DBZ_NO_STDIO
SRCS+=	bzlib.c crctable.c decompress.c huffman.c randtable.c

# decompression functionality from libz
.PATH: ${.CURDIR}/../libz
CFLAGS+=-DHAVE_MEMCPY -I${.CURDIR}/../libz
SRCS+=	adler32.c crc32.c _infback.c _inffast.c _inflate.c \
	_inftrees.c _zutil.c _zutil.h

# aargh
.for file in zutil.h
CLEANFILES+=	_${file} _${file}.orig

_${file}: ${file} ${file}.diff
	patch -s -b .orig -o ${.TARGET} < ${.ALLSRC:M*.diff} ${.ALLSRC:M*.[ch]}
.endfor

.for file in infback.c inffast.c inflate.c inftrees.c zutil.c
CLEANFILES+=	_${file}

_${file}: ${file}
	sed "s|zutil\.h|_zutil.h|" ${.ALLSRC} > ${.TARGET}
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

.if ${MACHINE_ARCH} == "amd64"
${SRCS:M*.c:R:S/$/.o/g}:	machine

beforedepend:	machine

machine:
	ln -s ${.CURDIR}/../../sys/i386/include machine
.endif
