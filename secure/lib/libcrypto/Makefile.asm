# $FreeBSD$
# Use this to help generate the asm *.[Ss] files after an import.  It is not
# perfect by any means, but does what is needed.
# Do a 'make -f Makefile.asm all' and it will generate *.s.  Move them
# to the i386 subdir, and correct any exposed paths and $ FreeBSD $ tags.

.include "Makefile.inc"

.if ${MACHINE_CPUARCH} == "amd64"

.PATH:	${LCRYPTO_SRC}/crypto \
	${LCRYPTO_SRC}/crypto/aes/asm \
	${LCRYPTO_SRC}/crypto/bn/asm \
	${LCRYPTO_SRC}/crypto/camellia/asm \
	${LCRYPTO_SRC}/crypto/ec/asm \
	${LCRYPTO_SRC}/crypto/md5/asm \
	${LCRYPTO_SRC}/crypto/modes/asm \
	${LCRYPTO_SRC}/crypto/rc4/asm \
	${LCRYPTO_SRC}/crypto/sha/asm \
	${LCRYPTO_SRC}/crypto/whrlpool/asm

# aes
SRCS=	aes-x86_64.pl aesni-mb-x86_64.pl aesni-sha1-x86_64.pl \
	aesni-sha256-x86_64.pl aesni-x86_64.pl bsaes-x86_64.pl \
	vpaes-x86_64.pl

# bn
SRCS+=	rsaz-avx2.pl rsaz-x86_64.pl x86_64-gf2m.pl x86_64-mont.pl \
	x86_64-mont5.pl

# camellia
SRCS+=	cmll-x86_64.pl

# ec
SRCS+=	ecp_nistz256-x86_64.pl

# md5
SRCS+=	md5-x86_64.pl

# modes
SRCS+=	aesni-gcm-x86_64.pl ghash-x86_64.pl

# rc4
SRCS+=	rc4-md5-x86_64.pl rc4-x86_64.pl

# sha
SRCS+=	sha1-mb-x86_64.pl sha1-x86_64.pl sha256-mb-x86_64.pl sha512-x86_64.pl

# whrlpool
SRCS+=	wp-x86_64.pl

ASM=	${SRCS:S/.pl/.S/}
ASM+=	sha256-x86_64.S x86_64cpuid.S

all:	${ASM}

CLEANFILES+=	${SRCS:M*.pl:S/.pl$/.cmt/} ${SRCS:M*.pl:S/.pl$/.S/}
CLEANFILES+=	sha256-x86_64.cmt sha256-x86_64.S x86_64cpuid.cmt x86_64cpuid.S
.SUFFIXES:	.pl .cmt

.pl.cmt:
	( cd `dirname ${.IMPSRC}`/.. ; perl ${.IMPSRC} ${.OBJDIR}/${.TARGET} )

.cmt.S:
	( echo '	# $$'FreeBSD'$$'; cat ${.IMPSRC} ) > ${.TARGET}

sha256-x86_64.cmt: sha512-x86_64.pl
	( cd `dirname ${.ALLSRC}`/.. ; perl ${.ALLSRC} ${.OBJDIR}/${.TARGET} )

x86_64cpuid.cmt: x86_64cpuid.pl
	( cd `dirname ${.ALLSRC}` ; perl ${.ALLSRC} ${.OBJDIR}/${.TARGET} )

.elif ${MACHINE_CPUARCH} == "i386"

.PATH:	${LCRYPTO_SRC}/crypto \
	${LCRYPTO_SRC}/crypto/aes/asm \
	${LCRYPTO_SRC}/crypto/bf/asm \
	${LCRYPTO_SRC}/crypto/bn/asm \
	${LCRYPTO_SRC}/crypto/camellia/asm \
	${LCRYPTO_SRC}/crypto/des/asm \
	${LCRYPTO_SRC}/crypto/md5/asm \
	${LCRYPTO_SRC}/crypto/modes/asm \
	${LCRYPTO_SRC}/crypto/rc4/asm \
	${LCRYPTO_SRC}/crypto/rc5/asm \
	${LCRYPTO_SRC}/crypto/ripemd/asm \
	${LCRYPTO_SRC}/crypto/sha/asm \
	${LCRYPTO_SRC}/crypto/whrlpool/asm

PERLPATH=	-I${LCRYPTO_SRC}/crypto/des/asm -I${LCRYPTO_SRC}/crypto/perlasm

# aes
SRCS=	aes-586.pl aesni-x86.pl vpaes-x86.pl

# blowfish
SRCS+=	bf-586.pl bf-686.pl

# bn
SRCS+=	bn-586.pl co-586.pl x86-gf2m.pl x86-mont.pl

# camellia
SRCS+=	cmll-x86.pl

# des
SRCS+=	crypt586.pl des-586.pl

# md5
SRCS+=	md5-586.pl

# modes
SRCS+=	ghash-x86.pl

# rc4
SRCS+=	rc4-586.pl

# rc5
SRCS+=	rc5-586.pl

# ripemd
SRCS+=	rmd-586.pl

# sha
SRCS+=	sha1-586.pl sha256-586.pl sha512-586.pl

# whrlpool
SRCS+=	wp-mmx.pl

# cpuid
SRCS+=	x86cpuid.pl

ASM=	${SRCS:S/.pl/.s/}

all:	${ASM}

CLEANFILES+=	${SRCS:M*.pl:S/.pl$/.s/}
.SUFFIXES:	.pl

.pl.s:
	( echo '	# $$'FreeBSD'$$' ;\
	perl ${PERLPATH} ${.IMPSRC} elf ${CFLAGS} ) > ${.TARGET}
.endif

.include <bsd.prog.mk>
