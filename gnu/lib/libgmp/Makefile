#
# $Id: Makefile,v 1.10 1997/02/22 15:42:57 peter Exp $
#

LIB=	gmp

SUBDIR=	doc

GMPDIR=	${.CURDIR}/../../../contrib/libgmp
.PATH:	${GMPDIR} ${GMPDIR}/mpn ${GMPDIR}/mpn/x86 ${GMPDIR}/mpn/generic

CFLAGS+=	-I${GMPDIR} -I${GMPDIR}/mpn/generic -I${GMPDIR}/mpn/x86 \
		-DBROKEN_ALIGN

MPN_SRC_C=	inlines.c cmp.c divmod_1.c divrem.c divrem_1.c dump.c \
		mod_1.c mul.c mul_n.c random2.c sqrtrem.c get_str.c set_str.c \
		scan0.c scan1.c popcount.c hamdist.c gcd_1.c pre_mod_1.c \
		perfsqr.c bdivmod.c gcd.c gcdext.c mp_bases.c

MPN_SRC_ASM=	add_n.S addmul_1.S lshift.S mul_1.S rshift.S sub_n.S submul_1.S

MPN_SRCS=	${MPN_SRC_C} ${MPN_SRC_ASM}

MPZ_SRCS=	mpz/init.c mpz/set.c mpz/set_ui.c mpz/set_si.c mpz/set_str.c \
		mpz/set_d.c mpz/set_f.c mpz/set_q.c mpz/iset.c mpz/iset_ui.c \
		mpz/iset_si.c mpz/iset_str.c mpz/iset_d.c mpz/clear.c \
		mpz/get_ui.c mpz/get_si.c mpz/get_str.c mpz/get_d.c \
		mpz/size.c mpz/sizeinbase.c mpz/add.c mpz/add_ui.c \
		mpz/sub.c mpz/sub_ui.c mpz/mul.c mpz/mul_ui.c mpz/gcd.c \
		mpz/gcd_ui.c mpz/gcdext.c mpz/sqrt.c mpz/sqrtrem.c \
		mpz/powm.c mpz/powm_ui.c mpz/cmp.c mpz/cmp_ui.c mpz/cmp_si.c \
		mpz/mul_2exp.c mpz/fdiv_q_2exp.c mpz/fdiv_r_2exp.c \
		mpz/tdiv_q_2exp.c mpz/tdiv_r_2exp.c mpz/abs.c mpz/neg.c \
		mpz/com.c mpz/and.c mpz/ior.c mpz/inp_raw.c mpz/inp_str.c \
		mpz/out_raw.c mpz/out_str.c mpz/perfsqr.c mpz/random.c \
		mpz/random2.c mpz/pow_ui.c mpz/ui_pow_ui.c mpz/setbit.c \
		mpz/clrbit.c mpz/fac_ui.c mpz/pprime_p.c mpz/realloc.c \
		mpz/getlimbn.c mpz/popcount.c mpz/hamdist.c mpz/cdiv_qr.c \
		mpz/cdiv_q.c mpz/cdiv_r.c mpz/cdiv_qr_ui.c mpz/cdiv_q_ui.c \
		mpz/cdiv_r_ui.c mpz/cdiv_ui.c mpz/fdiv_qr.c mpz/fdiv_q.c \
		mpz/fdiv_r.c mpz/fdiv_qr_ui.c mpz/fdiv_q_ui.c mpz/fdiv_r_ui.c \
		mpz/fdiv_ui.c mpz/tdiv_qr.c mpz/tdiv_q.c mpz/tdiv_r.c \
		mpz/tdiv_qr_ui.c mpz/tdiv_q_ui.c mpz/tdiv_r_ui.c mpz/mod.c \
		mpz/divexact.c mpz/array_init.c mpz/scan0.c mpz/scan1.c \
		mpz/jacobi.c mpz/legendre.c mpz/invert.c

MPF_SRCS=	mpf/init.c mpf/init2.c mpf/set.c mpf/set_ui.c mpf/set_si.c \
		mpf/set_str.c mpf/set_d.c mpf/set_z.c mpf/iset.c \
		mpf/iset_ui.c mpf/iset_si.c mpf/iset_str.c mpf/iset_d.c \
		mpf/clear.c mpf/get_str.c mpf/dump.c mpf/size.c mpf/eq.c \
		mpf/reldiff.c mpf/sqrt.c mpf/random2.c mpf/inp_str.c \
		mpf/out_str.c mpf/add.c mpf/add_ui.c mpf/sub.c mpf/sub_ui.c \
		mpf/ui_sub.c mpf/mul.c mpf/mul_ui.c mpf/div.c mpf/div_ui.c \
		mpf/cmp.c mpf/cmp_ui.c mpf/cmp_si.c mpf/mul_2exp.c \
		mpf/div_2exp.c mpf/abs.c mpf/neg.c mpf/set_q.c mpf/get_d.c \
		mpf/set_dfl_prec.c mpf/set_prc.c mpf/set_prc_raw.c \
		mpf/get_prc.c mpf/ui_div.c mpf/sqrt_ui.c

MPQ_SRCS=	mpq/add.c mpq/canonicalize.c mpq/clear.c mpq/cmp.c \
		mpq/cmp_ui.c mpq/div.c mpq/get_d.c mpq/get_den.c mpq/get_num.c \
		mpq/init.c mpq/inv.c mpq/mul.c mpq/neg.c mpq/set.c \
		mpq/set_den.c mpq/set_num.c mpq/set_si.c mpq/set_ui.c \
		mpq/sub.c mpq/equal.c mpq/set_z.c

SRCS=	memory.c mp_set_fns.c mp_clz_tab.c version.c stack-alloc.c mp_bpl.c \
	extract-double.c insert-double.c \
	${MPN_SRCS} ${MPZ_SRCS} ${MPF_SRCS} ${MPQ_SRCS}

sysdep.h: ${GMPDIR}/mpn/bsd.h
	cp ${GMPDIR}/mpn/bsd.h sysdep.h

asm-syntax.h:
	(echo "#define BSD_SYNTAX" ; \
		echo "#include \"syntax.h\"") > asm-syntax.h

beforedepend all: mpz mpf mpq sysdep.h asm-syntax.h

CLEANFILES+=	sysdep.h asm-syntax.h

# Grrr. This package contains modules in separate subdirs that have the
# same name. Nasty hack to keep them from being found by being in
# .PATH:s
#
mpz mpf mpq:
	mkdir ${.TARGET}

CLEANDIRS+=	${.OBJDIR}/mpz ${.CURDIR}/mpz \
		${.OBJDIR}/mpf ${.CURDIR}/mpf \
		${.OBJDIR}/mpq ${.CURDIR}/mpq

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${GMPDIR}/gmp.h ${DESTDIR}/usr/include

.S.o:
	${CC} ${SFLAGS} ${CFLAGS:M-[ID]*} ${AINC} -c ${.IMPSRC} -o ${.TARGET}

.S.po:
	${CC} -DPROF ${SFLAGS} ${CFLAGS:M-[ID]*} ${AINC} -c ${.IMPSRC} -o ${.TARGET}

.S.so:
	${CC} -fpic -DPIC ${SFLAGS} ${CFLAGS:M-[ID]*} ${AINC} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

.include "Makefile.inc"
.include <bsd.lib.mk>
