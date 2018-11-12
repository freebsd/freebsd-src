# $FreeBSD$
# Use this to help generate the asm *.S files after an import.  It is not
# perfect by any means, but does what is needed.
# Do a 'make -f Makefile.asm all' and it will generate *.S.  Move them
# to the arch subdir, and correct any exposed paths and $ FreeBSD $ tags.

.include "Makefile.inc"

.if defined(ASM_amd64)

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
SRCS+=	sha1-mb-x86_64.pl sha1-x86_64.pl sha256-mb-x86_64.pl

# whrlpool
SRCS+=	wp-x86_64.pl

# cpuid
SRCS+=	x86_64cpuid.pl

SHA_ASM=	sha256-x86_64 sha512-x86_64
SHA_SRC=	sha512-x86_64.pl
SHA_TMP=	${SHA_ASM:S/$/.s/}

ASM=	${SRCS:R:S/$/.S/} ${SHA_ASM:S/$/.S/}

all:	${ASM}

CLEANFILES=	${ASM} ${SHA_ASM:S/$/.s/}
.SUFFIXES:	.pl

.pl.S:
	( echo '/* $$'FreeBSD'$$ */' ;\
	echo '/* Do not modify. This file is auto-generated from ${.IMPSRC:T}. */' ;\
	env CC=cc perl ${.IMPSRC} elf ) > ${.TARGET}

${SHA_TMP}: ${SHA_SRC}
	env CC=cc perl ${.ALLSRC} elf ${.TARGET}

.for s in ${SHA_ASM}
${s}.S: ${s}.s
	( echo '/* $$'FreeBSD'$$ */' ;\
	echo '/* Do not modify. This file is auto-generated from ${SHA_SRC}. */' ;\
	cat ${s}.s ) > ${.TARGET}
.endfor

.elif defined(ASM_arm)

.PATH:	${LCRYPTO_SRC}/crypto \
	${LCRYPTO_SRC}/crypto/aes/asm \
	${LCRYPTO_SRC}/crypto/bn/asm \
	${LCRYPTO_SRC}/crypto/modes/asm \
	${LCRYPTO_SRC}/crypto/sha/asm

PERLPATH=	-I${LCRYPTO_SRC}/crypto/perlasm

# aes
SRCS=	aesv8-armx.pl bsaes-armv7.pl

# bn
SRCS+=	armv4-mont.pl armv4-gf2m.pl

# modes
SRCS+=	ghash-armv4.pl ghashv8-armx.pl

# sha
SRCS+=	sha1-armv4-large.pl sha256-armv4.pl sha512-armv4.pl

ASM=	aes-armv4.S ${SRCS:R:S/$/.S/}

all:	${ASM}

CLEANFILES=	${ASM} ${SRCS:R:S/$/.s/}
.SUFFIXES:	.pl

aes-armv4.S:	aes-armv4.pl
	( echo '/* $$'FreeBSD'$$ */' ;\
	echo '/* Do not modify. This file is auto-generated from ${.ALLSRC:T}. */' ;\
	env CC=cc perl ${.ALLSRC} elf ) > ${.TARGET}

.pl.S:
	env CC=cc perl ${.IMPSRC} elf ${.TARGET:R:S/$/.s/}
	( echo '/* $$'FreeBSD'$$ */' ;\
	echo '/* Do not modify. This file is auto-generated from ${.IMPSRC:T:R:S/$/.pl/}. */' ;\
	cat ${.TARGET:R:S/$/.s/}) > ${.TARGET}

.elif defined(ASM_i386)

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

ASM=	${SRCS:R:S/$/.S/}

all:	${ASM}

CLEANFILES=	${ASM}
.SUFFIXES:	.pl

.pl.S:
	( echo '/* $$'FreeBSD'$$ */' ;\
	echo '/* Do not modify. This file is auto-generated from ${.IMPSRC:T}. */' ;\
	echo '#ifdef PIC' ;\
	env CC=cc perl ${PERLPATH} ${.IMPSRC} elf ${CFLAGS} -fpic -DPIC ;\
	echo '#else' ;\
	env CC=cc perl ${PERLPATH} ${.IMPSRC} elf ${CFLAGS} ;\
	echo '#endif') |\
	sed -E 's|(\.file[[:blank:]]+)".*"|\1"${.TARGET}"|' > ${.TARGET}
.endif

.include <bsd.prog.mk>
