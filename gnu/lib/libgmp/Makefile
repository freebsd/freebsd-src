# Makefile for libgmp
# $Id: Makefile,v 1.4 1996/05/07 23:14:38 wosch Exp $

LIB=    gmp
SRCS=	$(GMP_SRCS)
CLEANFILES+=	cre-mparam cre-conv-tab gmp-mparam.h mp_bases.c

GMP_SRCS=	mp_bases.c $(MPZ_SRCS) $(MPQ_SRCS) $(MPN_SRCS) $(IMPL_SRCS)

CFLAGS+=	-I${.CURDIR} -I${.OBJDIR} -Wall -Wstrict-prototypes -DLOCALE

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

SUBDIR+=	doc

beforedepend:	gmp-mparam.h mp_bases.c

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/gmp.h \
		${DESTDIR}/usr/include

cre-mparam: cre-mparam.c
	$(CC) $(CFLAGS) ${.CURDIR}/cre-mparam.c -o ${.TARGET}

cre-conv-tab: cre-conv-tab.c gmp-mparam.h
	$(CC) $(CFLAGS) -lm ${.CURDIR}/cre-conv-tab.c -o ${.TARGET}

cre-conv-tab: gmp-mparam.h

gmp-mparam.h: cre-mparam
	./cre-mparam > tmp-${.TARGET}
	mv -f tmp-${.TARGET} ${.TARGET}

mp_bases.c: cre-conv-tab
	./cre-conv-tab > tmp-${.TARGET}
	mv -f tmp-${.TARGET} ${.TARGET}

.include <bsd.lib.mk>
