# Makefile for GNU MP (a.k.a. biGNUm)
# Copyright (C) 1991, 1993 Free Software Foundation, Inc.

# This file is part of the GNU MP Library.

# The GNU MP Library is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# The GNU MP Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with the GNU MP Library; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

CC = gcc

# If you cross compile on a machine with the same sizes of the integral
# types ("int", "long int", "short int", and "char") define this as the
# local compiler.  Otherwise, you need look for the uses of LOCAL_CC below,
# and handle those cases manually.
LOCAL_CC = $(CC)
OPT = -O -g
CFLAGS = -I. $(OPT)
AR = ar
RANLIB=`if [ -r /bin/ranlib -o -r /usr/bin/ranlib ]; \
	then echo ranlib; else echo true; fi`
SHELL = /bin/sh

SRCS = $(MPZ_SRCS) $(MPQ_SRCS) $(BSDMP_SRCS) $(MPN_SRCS) $(IMPL_SRCS)
OBJS = $(MPZ_OBJS) $(MPQ_OBJS) $(BSDMP_OBJS) $(MPN_OBJS) $(IMPL_OBJS)

MP_OBJS = $(BSDMP_OBJS) $(MPN_OBJS) $(IMPL_OBJS) mp_bases.o mpz_sizeinb.o
GMP_OBJS = $(MPZ_OBJS) $(MPQ_OBJS) $(MPN_OBJS) $(IMPL_OBJS) mp_bases.o

IMPL_SRCS = memory.c mp_set_fns.c _mpz_set_str.c _mpz_get_str.c \
  mpz_realloc.c mp_clz_tab.c alloca.c
IMPL_OBJS = memory.o mp_set_fns.o _mpz_set_str.o _mpz_get_str.o \
  mpz_realloc.o mp_clz_tab.o alloca.o

MPZ_SRCS = mpz_init.c mpz_set.c mpz_set_ui.c mpz_set_si.c mpz_set_str.c \
  mpz_iset.c mpz_iset_ui.c mpz_iset_si.c mpz_iset_str.c mpz_clear.c \
  mpz_get_ui.c mpz_get_si.c mpz_get_str.c mpz_size.c mpz_sizeinb.c \
  mpz_add.c mpz_add_ui.c mpz_sub.c mpz_sub_ui.c mpz_mul.c mpz_mul_ui.c \
  mpz_div.c mpz_div_ui.c mpz_mod.c mpz_mod_ui.c mpz_dm.c mpz_dm_ui.c \
  mpz_mdiv.c mpz_mmod.c mpz_mdm.c mpz_mdiv_ui.c mpz_mmod_ui.c mpz_mdm_ui.c \
  mpz_gcd.c mpz_gcdext.c mpz_sqrt.c mpz_sqrtrem.c mpz_powm.c mpz_powm_ui.c \
  mpz_cmp.c mpz_cmp_ui.c mpz_cmp_si.c mpz_mul_2exp.c mpz_div_2exp.c \
  mpz_mod_2exp.c mpz_abs.c mpz_neg.c mpz_com.c mpz_and.c mpz_ior.c \
  mpz_inp_raw.c mpz_inp_str.c mpz_out_raw.c mpz_out_str.c \
  mpz_perfsqr.c mpz_random.c mpz_random2.c mpz_pow_ui.c \
  mpz_clrbit.c mpz_fac_ui.c mpz_pprime_p.c
MPZ_OBJS = mpz_init.o mpz_set.o mpz_set_ui.o mpz_set_si.o mpz_set_str.o \
  mpz_iset.o mpz_iset_ui.o mpz_iset_si.o mpz_iset_str.o mpz_clear.o \
  mpz_get_ui.o mpz_get_si.o mpz_get_str.o mpz_size.o mpz_sizeinb.o \
  mpz_add.o mpz_add_ui.o mpz_sub.o mpz_sub_ui.o mpz_mul.o mpz_mul_ui.o \
  mpz_div.o mpz_div_ui.o mpz_mod.o mpz_mod_ui.o mpz_dm.o mpz_dm_ui.o \
  mpz_mdiv.o mpz_mmod.o mpz_mdm.o mpz_mdiv_ui.o mpz_mmod_ui.o mpz_mdm_ui.o \
  mpz_gcd.o mpz_gcdext.o mpz_sqrt.o mpz_sqrtrem.o mpz_powm.o mpz_powm_ui.o \
  mpz_cmp.o mpz_cmp_ui.o mpz_cmp_si.o mpz_mul_2exp.o mpz_div_2exp.o \
  mpz_mod_2exp.o mpz_abs.o mpz_neg.o mpz_com.o mpz_and.o mpz_ior.o \
  mpz_inp_raw.o mpz_inp_str.o mpz_out_raw.o mpz_out_str.o \
  mpz_perfsqr.o mpz_random.o mpz_random2.o mpz_pow_ui.o \
  mpz_clrbit.o mpz_fac_ui.o mpz_pprime_p.o

MPQ_SRCS = mpq_init.c mpq_set.c mpq_set_ui.c mpq_set_si.c \
  mpq_set_num.c mpq_set_den.c mpq_get_num.c mpq_get_den.c \
  mpq_add.c mpq_sub.c mpq_mul.c mpq_div.c \
  mpq_clear.c mpq_cmp.c mpq_inv.c mpq_neg.c
MPQ_OBJS = mpq_init.o mpq_set.o mpq_set_ui.o mpq_set_si.o \
  mpq_set_num.o mpq_set_den.o mpq_get_num.o mpq_get_den.o \
  mpq_add.o mpq_sub.o mpq_mul.o mpq_div.o \
  mpq_clear.o mpq_cmp.o mpq_inv.o mpq_neg.o

MPN_SRCS = mpn_add.c mpn_sub.c mpn_cmp.c mpn_mul.c mpn_div.c mpn_dm_1.c \
  mpn_mod_1.c mpn_lshift.c mpn_rshift.c mpn_rshiftci.c mpn_sqrt.c
MPN_OBJS = mpn_add.o mpn_sub.o mpn_cmp.o mpn_mul.o mpn_div.o mpn_dm_1.o \
  mpn_mod_1.o mpn_lshift.o mpn_rshift.o mpn_rshiftci.o mpn_sqrt.o

# There are fewer members in the BSDMP_SRCS list that in the BSDMP_OBJS
# list because some of the .c files are created by this Makefile.
BSDMP_SRCS = itom.c mdiv.c move.c mtox.c xtom.c sdiv.c mout.c min.c mfree.c
BSDMP_OBJS = gcd.o itom.o madd.o mcmp.o mdiv.o move.o msub.o mtox.o mult.o \
  pow.o rpow.o xtom.o msqrt.o sdiv.o mout.o min.o mfree.o

all: libgmp.a libmp.a

check: libgmp.a
	cd tests; $(MAKE) CC="$(CC)" SHELL="$(SHELL)" OPT="$(OPT)"

libgmp.a: stamp-stddefh $(GMP_OBJS)
	rm -f $@
	$(AR) cr $@ $(GMP_OBJS)
	$(RANLIB) $@

# libmp.a depend on libgmp.a, to get around Unix(tm) ar/ranlib concurrency bug.
libmp.a: stamp-stddefh $(MP_OBJS) libgmp.a
	rm -f $@
	$(AR) cr $@ $(MP_OBJS)
	$(RANLIB) $@

stamp-stddefh:
	rm -f stddef.h
	rm -f test-stddefh.c
	(	echo '#include <stddef.h>'				;\
		echo 'main(){size_t foo=sizeof(size_t);exit(0);}'	;\
	) > test-stddefh.c
	@if $(LOCAL_CC) $(CFLAGS) test-stddefh.c -c 2> /dev/null	;\
	then true							;\
	else								\
	  echo 'This machine has no "stddef.h".  Creating a minimal in ./';\
	  $(LOCAL_CC) $(CFLAGS) cre-stddefh.c -o cre-stddefh		;\
	  ./cre-stddefh > stddef.h					;\
	fi
	rm -f test-stddefh.o
	touch stamp-stddefh

mp_bases.c: cre-conv-tab
	./cre-conv-tab > tmp-$@
	mv tmp-$@ $@
cre-conv-tab: cre-conv-tab.c gmp.h gmp-impl.h gmp-mparam.h longlong.h
	$(LOCAL_CC) $(CFLAGS) `if [ x$(firstword $^) = x ];	\
			       then echo cre-conv-tab.c;	\
			       else echo $(firstword $^); fi` -o $@ -lm

gmp-mparam.h: cre-mparam
	./cre-mparam > tmp-$@
	mv tmp-$@ $@
cre-mparam: cre-mparam.c stamp-stddefh gmp.h
	$(LOCAL_CC) $(CFLAGS) cre-mparam.c -o $@

gcd.c : mpz_gcd.c stamp-stddefh mp.h gmp.h gmp-impl.h gmp-mparam.h longlong.h 
	$(CC) $(CFLAGS) -E -DBERKELEY_MP mpz_gcd.c \
          | grep -v '^#' > $@
pow.c : mpz_powm.c stamp-stddefh mp.h gmp.h gmp-impl.h gmp-mparam.h longlong.h 
	$(CC) $(CFLAGS) -E -DBERKELEY_MP mpz_powm.c\
          | grep -v '^#' > $@
rpow.c: mpz_pow_ui.c stamp-stddefh mp.h gmp.h gmp-impl.h gmp-mparam.h longlong.h 
	$(CC) $(CFLAGS) -E -DBERKELEY_MP mpz_pow_ui.c\
          | grep -v '^#' > $@
madd.c : mpz_add.c stamp-stddefh mp.h gmp.h gmp-impl.h gmp-mparam.h
	$(CC) $(CFLAGS) -E -DBERKELEY_MP mpz_add.c\
          | grep -v '^#' > $@
msub.c : mpz_sub.c stamp-stddefh mp.h gmp.h gmp-impl.h gmp-mparam.h
	$(CC) $(CFLAGS) -E -DBERKELEY_MP mpz_sub.c\
          | grep -v '^#' > $@
mult.c : mpz_mul.c stamp-stddefh mp.h gmp.h gmp-impl.h gmp-mparam.h
	$(CC) $(CFLAGS) -E -DBERKELEY_MP mpz_mul.c\
          | grep -v '^#' > $@
mcmp.c : mpz_cmp.c stamp-stddefh mp.h gmp.h gmp-impl.h gmp-mparam.h
	$(CC) $(CFLAGS) -E -DBERKELEY_MP mpz_cmp.c\
          | grep -v '^#' > $@
msqrt.c : mpz_sqrtrem.c stamp-stddefh mp.h gmp.h gmp-impl.h gmp-mparam.h
	$(CC) $(CFLAGS) -E -DBERKELEY_MP mpz_sqrtrem.c\
          | grep -v '^#' > $@

doc: gmp.dvi gmp.info
LN = ln -s
gmp.dvi: gmp.texi
	rm -f tmp.texi
	$(LN) gmp.texi tmp.texi
	tex tmp.texi < /dev/null
	texindex tmp.cp tmp.fn
	tex tmp.texi < /dev/null 2> /dev/null
	mv tmp.dvi gmp.dvi
gmp.info: gmp.texi
	makeinfo gmp.texi

clean:
	rm -f *.o libgmp.a libmp.a cre-conv-tab cre-mparam cre-stddefh \
 gmp.dvi gmp.info mp_bases.c gmp-mparam.h stamp-stddefh test-stddefh.c \
 stddef.h gcd.c pow.c rpow.c madd.c msub.c mult.c mcmp.c msqrt.c \
 tmp.* tmp-* gmp.ps core
	-cd tests; $(MAKE) clean
realclean: clean

# Automatically generated dependencies

_mpz_get_str.o : _mpz_get_str.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
_mpz_set_str.o : _mpz_set_str.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
cre-conv-tab.o : cre-conv-tab.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
cre-mparam.o : cre-mparam.c gmp.h 
cre-stddefh.o : cre-stddefh.c 
itom.o : itom.c mp.h gmp.h gmp-impl.h gmp-mparam.h 
mdiv.o : mdiv.c mp.h gmp.h gmp-impl.h gmp-mparam.h longlong.h mpz_dmincl.c 
memory.o : memory.c gmp.h gmp-impl.h gmp-mparam.h 
mfree.o : mfree.c mp.h gmp.h gmp-impl.h gmp-mparam.h 
min.o : min.c mp.h gmp.h gmp-impl.h gmp-mparam.h 
mout.o : mout.c mp.h gmp.h gmp-impl.h gmp-mparam.h 
move.o : move.c mp.h gmp.h gmp-impl.h gmp-mparam.h 
mp_bases.o : mp_bases.c gmp.h gmp-impl.h gmp-mparam.h 
mp_clz_tab.o : mp_clz_tab.c gmp.h gmp-impl.h gmp-mparam.h 
mp_set_fns.o : mp_set_fns.c gmp.h gmp-impl.h gmp-mparam.h 
mpn_add.o : mpn_add.c gmp.h gmp-impl.h gmp-mparam.h 
mpn_cmp.o : mpn_cmp.c gmp.h gmp-impl.h gmp-mparam.h 
mpn_div.o : mpn_div.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpn_dm_1.o : mpn_dm_1.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpn_lshift.o : mpn_lshift.c gmp.h gmp-impl.h gmp-mparam.h 
mpn_mod_1.o : mpn_mod_1.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpn_mul.o : mpn_mul.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpn_rshift.o : mpn_rshift.c gmp.h gmp-impl.h gmp-mparam.h 
mpn_rshiftci.o : mpn_rshiftci.c gmp.h gmp-impl.h gmp-mparam.h 
mpn_sqrt.o : mpn_sqrt.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpn_sub.o : mpn_sub.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_add.o : mpq_add.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_clear.o : mpq_clear.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_cmp.o : mpq_cmp.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_div.o : mpq_div.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_get_den.o : mpq_get_den.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_get_num.o : mpq_get_num.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_init.o : mpq_init.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_inv.o : mpq_inv.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_mul.o : mpq_mul.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_neg.o : mpq_neg.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_set.o : mpq_set.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_set_den.o : mpq_set_den.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_set_num.o : mpq_set_num.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_set_si.o : mpq_set_si.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_set_ui.o : mpq_set_ui.c gmp.h gmp-impl.h gmp-mparam.h 
mpq_sub.o : mpq_sub.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_abs.o : mpz_abs.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_add.o : mpz_add.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_add_ui.o : mpz_add_ui.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_and.o : mpz_and.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_clear.o : mpz_clear.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_clrbit.o : mpz_clrbit.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_cmp.o : mpz_cmp.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_cmp_si.o : mpz_cmp_si.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_cmp_ui.o : mpz_cmp_ui.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_com.o : mpz_com.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_div.o : mpz_div.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpz_div_2exp.o : mpz_div_2exp.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_div_ui.o : mpz_div_ui.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpz_dm.o : mpz_dm.c gmp.h gmp-impl.h gmp-mparam.h longlong.h mpz_dmincl.c 
mpz_dm_ui.o : mpz_dm_ui.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpz_fac_ui.o : mpz_fac_ui.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpz_gcd.o : mpz_gcd.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpz_gcdext.o : mpz_gcdext.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_get_si.o : mpz_get_si.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_get_str.o : mpz_get_str.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_get_ui.o : mpz_get_ui.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_init.o : mpz_init.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_inp_raw.o : mpz_inp_raw.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_inp_str.o : mpz_inp_str.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_ior.o : mpz_ior.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_iset.o : mpz_iset.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_iset_si.o : mpz_iset_si.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_iset_str.o : mpz_iset_str.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_iset_ui.o : mpz_iset_ui.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_mdiv.o : mpz_mdiv.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_mdiv_ui.o : mpz_mdiv_ui.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_mdm.o : mpz_mdm.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_mdm_ui.o : mpz_mdm_ui.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_mmod.o : mpz_mmod.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_mmod_ui.o : mpz_mmod_ui.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_mod.o : mpz_mod.c gmp.h gmp-impl.h gmp-mparam.h longlong.h mpz_dmincl.c 
mpz_mod_2exp.o : mpz_mod_2exp.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_mod_ui.o : mpz_mod_ui.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpz_mul.o : mpz_mul.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_mul_2exp.o : mpz_mul_2exp.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_mul_ui.o : mpz_mul_ui.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpz_neg.o : mpz_neg.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_out_raw.o : mpz_out_raw.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_out_str.o : mpz_out_str.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_perfsqr.o : mpz_perfsqr.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpz_pow_ui.o : mpz_pow_ui.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpz_powm.o : mpz_powm.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpz_powm_ui.o : mpz_powm_ui.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpz_pprime_p.o : mpz_pprime_p.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_random.o : mpz_random.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_random2.o : mpz_random2.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_realloc.o : mpz_realloc.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_set.o : mpz_set.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_set_si.o : mpz_set_si.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_set_str.o : mpz_set_str.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpz_set_ui.o : mpz_set_ui.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_size.o : mpz_size.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_sizeinb.o : mpz_sizeinb.c gmp.h gmp-impl.h gmp-mparam.h longlong.h 
mpz_sqrt.o : mpz_sqrt.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_sqrtrem.o : mpz_sqrtrem.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_sub.o : mpz_sub.c gmp.h gmp-impl.h gmp-mparam.h 
mpz_sub_ui.o : mpz_sub_ui.c gmp.h gmp-impl.h gmp-mparam.h 
mtox.o : mtox.c mp.h gmp.h gmp-impl.h gmp-mparam.h 
sdiv.o : sdiv.c mp.h gmp.h gmp-impl.h gmp-mparam.h longlong.h 
xtom.o : xtom.c mp.h gmp.h gmp-impl.h gmp-mparam.h 
