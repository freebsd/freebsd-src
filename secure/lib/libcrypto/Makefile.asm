# $FreeBSD$
# Use this to help generate the asm *.s files after an import.  It is not
# perfect by any means, but does what is needed.
# Do a 'make -f Makefile.asm all' and it will generate *.s.  Move them
# to the i386 subdir, and correct any exposed paths and $FreeBSD$ tags.

.if ${MACHINE_ARCH} == "i386"

.include "Makefile.inc"

.PATH: ${LCRYPTO_SRC}/rc4/asm ${LCRYPTO_SRC}/rc5/asm \
       ${LCRYPTO_SRC}/des/asm ${LCRYPTO_SRC}/cast/asm \
       ${LCRYPTO_SRC}/sha/asm ${LCRYPTO_SRC}/bn/asm \
       ${LCRYPTO_SRC}/bf/asm ${LCRYPTO_SRC}/md5/asm \
       ${LCRYPTO_SRC}/ripemd/asm

PERLPATH=	${LCRYPTO_SRC}/des/asm:${LCRYPTO_SRC}/perlasm

# blowfish
SRCS=	bf-686.pl bf-586.pl

# bn
SRCS+=	bn-586.pl co-586.pl

# cast
SRCS+=	cast-586.pl

# des
SRCS+=	des-586.pl crypt586.pl

# md5
SRCS+=	md5-586.pl

# rc4
SRCS+=	rc4-586.pl

# rc5
SRCS+=	rc5-586.pl

# ripemd
SRCS+=	rmd-586.pl

# sha
SRCS+=	sha1-586.pl

ASM=	${SRCS:S/.pl/.s/}

all:	${ASM}

CLEANFILES+=	${SRCS:M*.pl:S/.pl$/.cmt/} ${SRCS:M*.pl:S/.pl$/.s/}
.SUFFIXES:	.pl .cmt

.pl.cmt:
	perl -I${PERLPATH} ${.IMPSRC} elf ${CPUTYPE:Mi386:S/i//} > ${.TARGET}

.cmt.s:
	tr -d "'" < ${.IMPSRC} > ${.TARGET}

.include <bsd.prog.mk>
.endif
