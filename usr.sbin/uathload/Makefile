# $FreeBSD$

PROG=	uathload
MAN=	uathload.8

SRCS=	uathload.c ar5523.bin

CLEANFILES=	ar5523.bin

# It's hard to tag ar5523.o with the proper gnu note saying that it has a
# non-executable stack, so ld doesn't properly mark his executable as
# not having an executable stack. Mark it explicitly, but only for those
# platforms that support his feature (otherwise signals don't work).
# Note: Newer versions of ld than is in the tree ignore -z.
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "powerpc64"
LDFLAGS+=	-Wl,-z,noexecstack
.endif

# The conversion from .bin to .o doesn't always produce a pedantically correct
# .o's. And it doesn't matter, so turn off the mismatch warnings since it is
# pure data. On mips64 here's no easy way to produce a proper .o.
LDFLAGS+=	-Wl,--no-warn-mismatch

ar5523.bin: ${.CURDIR}/../../sys/contrib/dev/uath/ar5523.bin.uu
	uudecode -p ${.CURDIR}/../../sys/contrib/dev/uath/ar5523.bin.uu > ${.TARGET}

ar5523.o: ar5523.bin
	${LD} -b binary -d -warn-common -r -d -o ${.TARGET} ar5523.bin

.include <bsd.prog.mk>
