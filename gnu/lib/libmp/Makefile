#
# $FreeBSD$
#

LIB=	mp

GMPDIR=	${.CURDIR}/../../../contrib/libgmp
.PATH:	${GMPDIR} ${GMPDIR}/mpn ${GMPDIR}/mpn/x86 ${GMPDIR}/mpn/generic \
	${GMPDIR}/mpbsd

CFLAGS+=	-I${GMPDIR} -I${GMPDIR}/mpn/generic -I${GMPDIR}/mpn/x86 \
		-I${GMPDIR}/mpz -DBROKEN_ALIGN -DBERKELEY_MP

MPN_SRC_C=	inlines.c cmp.c divmod_1.c divrem.c divrem_1.c dump.c \
		mod_1.c mul.c mul_n.c random2.c sqrtrem.c get_str.c set_str.c \
		scan0.c scan1.c popcount.c hamdist.c gcd_1.c pre_mod_1.c \
		perfsqr.c bdivmod.c gcd.c gcdext.c mp_bases.c

MPN_SRC_ASM=	add_n.S addmul_1.S lshift.S mul_1.S rshift.S sub_n.S submul_1.S

MPN_SRCS=	${MPN_SRC_C} ${MPN_SRC_ASM}

MPBSD_LINKS=	mpz/add.c mpz/cmp.c mpz/gcd.c mpz/mul.c mpz/pow_ui.c \
		mpz/powm.c mpz/sqrtrem.c mpz/sub.c mpz/realloc.c
MPBSD_SRCS=	itom.c mdiv.c mfree.c min.c mout.c move.c mtox.c sdiv.c xtom.c \
		realloc.c $(MPBSD_LINKS)

SRCS=		memory.c mp_set_fns.c mp_clz_tab.c version.c stack-alloc.c \
		mp_bpl.c extract-double.c insert-double.c \
		${MPN_SRCS} ${MPBSD_SRCS}

beforedepend all: mpz sysdep.h asm-syntax.h

sysdep.h: ${GMPDIR}/mpn/bsd.h
	cp ${GMPDIR}/mpn/bsd.h sysdep.h

asm-syntax.h:
	(echo "#define BSD_SYNTAX" ; \
		echo "#include \"syntax.h\"") > asm-syntax.h

CLEANFILES+=	sysdep.h asm-syntax.h

# Grrr. This package contains modules in separate subdirs that have the
# same name. Nasty hack to keep them from being found by being in
# .PATH:s
#
mpz:
.if exists(${.OBJDIR})
	mkdir ${.OBJDIR}/$@
.else
	mkdir ${.CURDIR}/$@
.endif

CLEANDIRS+=	 ${.OBJDIR}/mpz ${.CURDIR}/mpz

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${GMPDIR}/mp.h ${DESTDIR}/usr/include

.include "../libgmp/Makefile.inc"
.include <bsd.lib.mk>
