#
# $Id: Makefile,v 1.12 1997/08/29 13:58:08 peter Exp $
#

LIB=	mp

GMPDIR=	${.CURDIR}/../../../contrib/libgmp

CFLAGS+=	-I${GMPDIR} -I${GMPDIR}/mpn/generic

.if ${MACHINE_ARCH} == "alpha"
.PATH:		${GMPDIR} ${GMPDIR}/mpn ${GMPDIR}/mpn/alpha ${GMPDIR}/mpn/generic \
		${GMPDIR}/mpbsd

CFLAGS+=	-I${GMPDIR}/mpn/alpha -I${GMPDIR}/mpz -DBERKELEY_MP

MPN_SRC_ASM=	add_n.s addmul_1.s lshift.s mul_1.s rshift.s sub_n.s submul_1.s udiv_qrnnd.S

.elif ${MACHINE_ARCH} == "i386"
.PATH:		${GMPDIR} ${GMPDIR}/mpn ${GMPDIR}/mpn/x86 ${GMPDIR}/mpn/generic \
		${GMPDIR}/mpbsd

CFLAGS+=	-I${GMPDIR}/mpn/x86 -I${GMPDIR}/mpz -DBERKELEY_MP

MPN_SRC_ASM=	add_n.S addmul_1.S lshift.S mul_1.S rshift.S sub_n.S submul_1.S

.elif ${MACHINE_ARCH} == "m68k"
.PATH:		${GMPDIR} ${GMPDIR}/mpn ${GMPDIR}/mpn/m68k ${GMPDIR}/mpn/generic \
		${GMPDIR}/mpbsd

CFLAGS+=	-I${GMPDIR}/mpn/m68k -I${GMPDIR}/mpz -DBERKELEY_MP

MPN_SRC_ASM=	add_n.S lshift.S rshift.S sub_n.S

.endif

MPN_SRC_C=	add_n.c addmul_1.c bdivmod.c cmp.c divmod_1.c divrem.c \
		divrem_1.c dump.c gcd.c gcd_1.c gcdext.c get_str.c hamdist.c \
		inlines.c lshift.c mod_1.c mul.c mul_1.c mul_n.c perfsqr.c \
		popcount.c pre_mod_1.c random2.c rshift.c scan0.c scan1.c \
		set_str.c sqrtrem.c mp_bases.c

MPBSD_LINKS=	mpz/add.c mpz/cmp.c mpz/gcd.c mpz/mul.c mpz/pow_ui.c \
		mpz/powm.c mpz/sqrtrem.c mpz/sub.c mpz/realloc.c
MPBSD_SRCS=	itom.c mdiv.c mfree.c min.c mout.c move.c mtox.c sdiv.c xtom.c \
		realloc.c $(MPBSD_LINKS)

SRCS=		memory.c mp_set_fns.c mp_clz_tab.c version.c stack-alloc.c \
		mp_bpl.c extract-double.c insert-double.c \
		${MPN_SRC_ASM}

# Look though the generic C source for names that don't have assembly
# source version.
.for _src in ${MPN_SRC_C}
.if (${MPN_SRC_ASM:R:M${_src:R}} == "")
SRCS+=$(_src)
.endif
.endfor

SRCS+=		${MPBSD_SRCS}

beforedepend all: mpz sysdep.h asm-syntax.h

.if defined(BINFORMAT) && ${BINFORMAT} == elf

sysdep.h: ${GMPDIR}/mpn/sysv.h
	cp ${GMPDIR}/mpn/sysv.h sysdep.h

asm-syntax.h:
	(echo "#define ELF_SYNTAX" ; \
		echo "#include \"syntax.h\"") > asm-syntax.h
.else

sysdep.h: ${GMPDIR}/mpn/bsd.h
	cp ${GMPDIR}/mpn/bsd.h sysdep.h

asm-syntax.h:
	(echo "#define BSD_SYNTAX" ; \
		echo "#include \"syntax.h\"") > asm-syntax.h

CFLAGS+=	-DBROKEN_ALIGN

.endif

CLEANFILES+=	sysdep.h asm-syntax.h

# Grrr. This package contains modules in separate subdirs that have the
# same name. Nasty hack to keep them from being found by being in
# .PATH:s
#
mpz:
	mkdir ${.TARGET}

CLEANDIRS+=	 ${.OBJDIR}/mpz ${.CURDIR}/mpz

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${GMPDIR}/mp.h ${DESTDIR}/usr/include

.include "../libgmp/Makefile.inc"
.include <bsd.lib.mk>
