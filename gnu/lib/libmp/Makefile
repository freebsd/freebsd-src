# Makefile for libmp
# $Id$

LIB=    mp
SRCS=	$(MP_SRCS)
CLEANFILES+=	cre-mparam cre-conv-tab gcd.c gmp-mparam.h mp_bases.c \
		madd.c mcmp.c msqrt.c msub.c mult.c pow.c rpow.c

MP_SRCS=	$(BSDMP_SRCS) $(MPN_SRCS) $(IMPL_SRCS) mp_bases.c mpz_sizeinb.c
GMP_SRCS=	$(MPZ_SRCS) $(MPQ_SRCS) $(MPN_SRCS) $(IMPL_SRCS) mp_bases.c

CFLAGS+=	-I${.CURDIR} -I${.OBJDIR} -Wall -Wstrict-prototypes -DLOCALE

BSDMP_SRCS=	gcd.c itom.c madd.c mcmp.c mdiv.c move.c msub.c mtox.c mult.c \
		pow.c rpow.c xtom.c msqrt.c sdiv.c mout.c min.c mfree.c

MPN_SRCS=	mpn_add.c mpn_sub.c mpn_cmp.c mpn_mul.c mpn_div.c mpn_dm_1.c \
		mpn_mod_1.c mpn_lshift.c mpn_rshift.c mpn_rshiftci.c mpn_sqrt.c

IMPL_SRCS=	memory.c mp_set_fns.c _mpz_set_str.c _mpz_get_str.c \
		mpz_realloc.c mp_clz_tab.c

MPZ_SRCS=	mpz_init.c mpz_set.c mpz_set_ui.c mpz_set_si.c mpz_set_str.c \
		mpz_iset.c mpz_iset_ui.c mpz_iset_si.c mpz_iset_str.c \
		mpz_clear.c mpz_get_ui.c mpz_get_si.c mpz_get_str.c \
		mpz_size.c mpz_sizeinb.c mpz_add.c mpz_add_ui.c mpz_sub.c \
		mpz_sub_ui.c mpz_mul.c mpz_mul_ui.c mpz_div.c mpz_div_ui.c \
		mpz_mod.c mpz_mod_ui.c mpz_dm.c mpz_dm_ui.c mpz_mdiv.c \
		mpz_mmod.c mpz_mdm.c mpz_mdiv_ui.c mpz_mmod_ui.c mpz_mdm_ui.c \
		mpz_gcd.c mpz_gcdext.c mpz_sqrt.c mpz_sqrtrem.c mpz_powm.c \
		mpz_powm_ui.c mpz_cmp.c mpz_cmp_ui.c mpz_cmp_si.c \
		mpz_mul_2exp.c mpz_div_2exp.c mpz_mod_2exp.c mpz_abs.c \
		mpz_neg.c mpz_com.c mpz_and.c mpz_ior.c mpz_inp_raw.c \
		mpz_inp_str.c mpz_out_raw.c mpz_out_str.c mpz_perfsqr.c \
		mpz_random.c mpz_random2.c mpz_pow_ui.c mpz_clrbit.c \
		mpz_fac_ui.c mpz_pprime_p.c

MPQ_SRCS=	mpq_init.c mpq_set.c mpq_set_ui.c mpq_set_si.c \
		mpq_set_num.c mpq_set_den.c mpq_get_num.c mpq_get_den.c \
		mpq_add.c mpq_sub.c mpq_mul.c mpq_div.c mpq_clear.c \
		mpq_cmp.c mpq_inv.c mpq_neg.c

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
