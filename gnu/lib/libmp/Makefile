# Makefile for libmp
# $Id: Makefile,v 1.2 1995/11/12 14:55:44 markm Exp $

LIB=    mp
SRCS=	$(MP_SRCS)
CLEANFILES+=	cre-mparam cre-conv-tab gcd.c gmp-mparam.h mp_bases.c \
		madd.c mcmp.c msqrt.c msub.c mult.c pow.c rpow.c

MP_SRCS=	$(BSDMP_SRCS) $(MPN_SRCS) $(IMPL_SRCS) mp_bases.c mpz_sizeinb.c

CFLAGS+=	-I${.CURDIR} -I${.OBJDIR} -Wall -Wstrict-prototypes -DLOCALE

BSDMP_SRCS=	gcd.c itom.c madd.c mcmp.c mdiv.c move.c msub.c mtox.c mult.c \
		pow.c rpow.c xtom.c msqrt.c sdiv.c mout.c min.c mfree.c

MPN_SRCS=	mpn_add.c mpn_sub.c mpn_cmp.c mpn_mul.c mpn_div.c mpn_dm_1.c \
		mpn_mod_1.c mpn_lshift.c mpn_rshift.c mpn_rshiftci.c mpn_sqrt.c

IMPL_SRCS=	memory.c mp_set_fns.c _mpz_set_str.c _mpz_get_str.c \
		mpz_realloc.c mp_clz_tab.c

SUBDIR+= doc

beforedepend:	gmp-mparam.h mp_bases.c

beforeinstall:
	cmp -s ${.CURDIR}/mp.h ${DESTDIR}/usr/include/mp.h || \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/mp.h ${DESTDIR}/usr/include/mp.h

cre-mparam: cre-mparam.c
	$(CC) $(CFLAGS) ${.OODATE} -o ${.TARGET}

cre-conv-tab: cre-conv-tab.c
	$(CC) $(CFLAGS) -lm ${.OODATE} -o ${.TARGET}

gmp-mparam.h: cre-mparam
	./cre-mparam > tmp-${.TARGET}
	mv tmp-${.TARGET} ${.TARGET}

mp_bases.c: cre-conv-tab
	./cre-conv-tab > tmp-${.TARGET}
	mv tmp-${.TARGET} ${.TARGET}

gcd.c:	mpz_gcd.c mp.h gmp.h gmp-impl.h gmp-mparam.h longlong.h 
	$(CC) $(CFLAGS) -E -DBERKELEY_MP ${.CURDIR}/mpz_gcd.c\
	  | grep -v '^#' > ${.TARGET}

pow.c:	mpz_powm.c mp.h gmp.h gmp-impl.h gmp-mparam.h longlong.h 
	$(CC) $(CFLAGS) -E -DBERKELEY_MP ${.CURDIR}/mpz_powm.c\
	  | grep -v '^#' > ${.TARGET}

rpow.c:	mpz_pow_ui.c mp.h gmp.h gmp-impl.h gmp-mparam.h longlong.h 
	$(CC) $(CFLAGS) -E -DBERKELEY_MP ${.CURDIR}/mpz_pow_ui.c\
	  | grep -v '^#' > ${.TARGET}

madd.c:	mpz_add.c mp.h gmp.h gmp-impl.h gmp-mparam.h
	$(CC) $(CFLAGS) -E -DBERKELEY_MP ${.CURDIR}/mpz_add.c\
	  | grep -v '^#' > ${.TARGET}

msub.c:	mpz_sub.c mp.h gmp.h gmp-impl.h gmp-mparam.h
	$(CC) $(CFLAGS) -E -DBERKELEY_MP ${.CURDIR}/mpz_sub.c\
	  | grep -v '^#' > ${.TARGET}

mult.c:	mpz_mul.c mp.h gmp.h gmp-impl.h gmp-mparam.h
	$(CC) $(CFLAGS) -E -DBERKELEY_MP ${.CURDIR}/mpz_mul.c\
	  | grep -v '^#' > ${.TARGET}

mcmp.c:	mpz_cmp.c mp.h gmp.h gmp-impl.h gmp-mparam.h
	$(CC) $(CFLAGS) -E -DBERKELEY_MP ${.CURDIR}/mpz_cmp.c\
	  | grep -v '^#' > ${.TARGET}

msqrt.c:	mpz_sqrtrem.c mp.h gmp.h gmp-impl.h gmp-mparam.h
	$(CC) $(CFLAGS) -E -DBERKELEY_MP ${.CURDIR}/mpz_sqrtrem.c\
	  | grep -v '^#' > ${.TARGET}

.include <bsd.lib.mk>
