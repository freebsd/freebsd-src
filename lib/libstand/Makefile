# $FreeBSD: src/lib/libstand/Makefile,v 1.14.2.2 2000/05/04 13:47:49 ps Exp $
#
# Originally from	$NetBSD: Makefile,v 1.21 1997/10/26 22:08:38 lukem Exp $
#
# Notes:
# - We don't use the libc strerror/sys_errlist because the string table is
#   quite large.
#

LIB=		stand
NOPROFILE=	YES
NOPIC=		YES
MAN3=		libstand.3

# Mostly OK, some of the libc imports are a bit noisy
CFLAGS+=	-Wall

.if ${MACHINE_ARCH} == "alpha"
CFLAGS+=	-mno-fp-regs
.endif

# standalone components and stuff we have modified locally
SRCS+=	__main.c assert.c bcd.c bswap.c environment.c getopt.c gets.c \
	globals.c pager.c printf.c strdup.c strerror.c strtol.c random.c \
	sbrk.c twiddle.c zalloc.c zalloc_malloc.c

# private (pruned) versions of libc string functions
SRCS+=	strcasecmp.c

# string functions from libc
.PATH:	${.CURDIR}/../libc/string
.if ${MACHINE_ARCH} == "i386"
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

.PATH: ${.CURDIR}/../libc/alpha/net
SRCS+= htons.S ntohs.S htonl.S ntohl.S

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

# network support from libc
.PATH:	${.CURDIR}/../libc/net
SRCS+=	inet_ntoa.c inet_addr.c

# _setjmp/_longjmp
.PATH:	${.CURDIR}/${MACHINE_ARCH}
SRCS+=	_setjmp.S
# really only required for i386
CFLAGS+=-I${.CURDIR}/../libc/${MACHINE_ARCH}

# decompression functionality from libz
.PATH:	${.CURDIR}/../libz
CFLAGS+=-DHAVE_MEMCPY
SRCS+=	adler32.c crc32.c infblock.c infcodes.c inffast.c inflate.c \
	inftrees.c infutil.c zutil.c 

# io routines
SRCS+=	closeall.c dev.c ioctl.c nullfs.c stat.c \
	fstat.c close.c lseek.c open.c read.c write.c readdir.c

# network routines
SRCS+=	arp.c ether.c in_cksum.c net.c udp.c netif.c rpc.c

# network info services:
SRCS+=	bootp.c rarp.c bootparam.c

# boot filesystems
SRCS+=	ufs.c nfs.c cd9660.c tftp.c zipfs.c
SRCS+=	netif.c nfs.c
SRCS+=	dosfs.c ext2fs.c

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/stand.h \
		${DESTDIR}/usr/include

.include <bsd.lib.mk>
