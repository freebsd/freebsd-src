#       $Id: Makefile,v 1.3 1997/02/22 12:47:52 peter Exp $

.PATH:  ${.CURDIR}/../../sys/gnu/i386/fpemul
KMOD=   gnufpu_mod
SRCS=   div_small.s errors.c fpu_arith.c fpu_aux.c fpu_entry.c fpu_etc.c \
	fpu_trig.c get_address.c load_store.c poly_2xm1.c poly_atan.c \
	poly_div.s poly_l2.c poly_mul64.s poly_sin.c poly_tan.c \
	polynomial.s reg_add_sub.c reg_compare.c reg_constant.c reg_div.s \
	reg_ld_str.c reg_u_mul.s reg_u_sub.s wm_shrx.s wm_sqrt.s
NOMAN=
PSEUDO_LKM=
CFLAGS+= -DLKM -I${.CURDIR}/../../sys/sys

.s.o:
	${CC} -x assembler-with-cpp -DLOCORE ${CFLAGS} -c \
	    ${.IMPSRC} -o ${.TARGET}

.include <bsd.kmod.mk>
