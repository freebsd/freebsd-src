# Makefile for libmp
# $Id: Makefile,v 1.3 1995/11/13 18:39:23 markm Exp $

LIB=    mp
SRCS=	$(MP_SRCS)
.PATH:	${.CURDIR}/../libgmp
CLEANFILES+=	cre-mparam cre-conv-tab gmp-mparam.h mp_bases.c

MP_SRCS=	mp_bases.c $(BSDMP_SRCS) $(MPN_SRCS) $(IMPL_SRCS) 

CFLAGS+=	-I${.CURDIR}/../libgmp -I${.OBJDIR} -Wall -Wstrict-prototypes \
		-DLOCALE -DBERKELEY_MP

BSDMP_SRCS=	mpz_gcd.c mpz_add.c mpz_cmp.c mpz_sub.c mpz_mul.c \
		mpz_powm.c mpz_pow_ui.c mpz_sqrtrem.c mpz_sizeinb.c \
		itom.c mdiv.c move.c mtox.c xtom.c sdiv.c mout.c min.c mfree.c

MPN_SRCS=	mpn_add.c mpn_sub.c mpn_cmp.c mpn_mul.c mpn_div.c mpn_dm_1.c \
		mpn_mod_1.c mpn_lshift.c mpn_rshift.c mpn_rshiftci.c mpn_sqrt.c

IMPL_SRCS=	memory.c mp_set_fns.c _mpz_set_str.c _mpz_get_str.c \
		mpz_realloc.c mp_clz_tab.c

beforedepend:	gmp-mparam.h mp_bases.c

beforeinstall:
	cmp -s ${.CURDIR}/../libgmp/mp.h ${DESTDIR}/usr/include/mp.h || \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/../libgmp/mp.h ${DESTDIR}/usr/include/mp.h

cre-mparam: cre-mparam.c
	$(CC) $(CFLAGS) ${.CURDIR}/../libgmp/cre-mparam.c -o ${.TARGET}

cre-conv-tab: cre-conv-tab.c gmp-mparam.h
	$(CC) $(CFLAGS) -lm ${.CURDIR}/../libgmp/cre-conv-tab.c -o ${.TARGET}

gmp-mparam.h: cre-mparam
	./cre-mparam > ${.TARGET}

mp_bases.c: cre-conv-tab
	./cre-conv-tab > ${.TARGET}

.include <bsd.lib.mk>
