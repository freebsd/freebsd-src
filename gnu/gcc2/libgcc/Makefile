LIB=	gcc
CC=	gcc

SRCS= _mulsi3.c _udivsi3.c _divsi3.c _umodsi3.c _modsi3.c \
   _lshrsi3.c _lshlsi3.c _ashrsi3.c _ashlsi3.c \
   _divdf3.c _muldf3.c _negdf2.c _adddf3.c _subdf3.c \
   _fixdfsi.c _fixsfsi.c _floatsidf.c _floatsisf.c \
   _truncdfsf2.c _extendsfdf2.c \
   _addsf3.c _negsf2.c _subsf3.c _mulsf3.c _divsf3.c \
   _eqdf2.c _nedf2.c _gtdf2.c _gedf2.c _ltdf2.c _ledf2.c \
   _eqsf2.c _nesf2.c _gtsf2.c _gesf2.c _ltsf2.c _lesf2.c \
   _muldi3.c _divdi3.c _moddi3.c _udivdi3.c _umoddi3.c _negdi2.c \
   _lshrdi3.c _lshldi3.c _ashldi3.c _ashrdi3.c  _ffsdi2.c \
   _udiv_w_sdiv.c _udivmoddi4.c _cmpdi2.c _ucmpdi2.c \
   _floatdidf.c _floatdisf.c \
   _fixunsdfsi.c _fixunssfsi.c _fixunsdfdi.c _fixdfdi.c \
   _fixunssfdi.c _fixsfdi.c \
   _fixxfdi.c _fixunsxfdi.c _floatdixf.c _fixunsxfsi.c \
   __gcc_bcmp.c _varargs.c _eprintf.c _op_new.c _new_handler.c \
   _op_delete.c  _bb.c _shtab.c _clear_cache.c _trampoline.c \
   __main.c _exit.c _ctors.c

.include <bsd.lib.mk>


LIB1FUNCS= _mulsi3 _udivsi3 _divsi3 _umodsi3 _modsi3 \
   _lshrsi3 _lshlsi3 _ashrsi3 _ashlsi3 \
   _divdf3 _muldf3 _negdf2 _adddf3 _subdf3 \
   _fixdfsi _fixsfsi _floatsidf _floatsisf \
   _truncdfsf2 _extendsfdf2 \
   _addsf3 _negsf2 _subsf3 _mulsf3 _divsf3 \
   _eqdf2 _nedf2 _gtdf2 _gedf2 _ltdf2 _ledf2 \
   _eqsf2 _nesf2 _gtsf2 _gesf2 _ltsf2 _lesf2 \

LIB2FUNCS=    _muldi3 _divdi3 _moddi3 _udivdi3 _umoddi3 _negdi2 \
   _lshrdi3 _lshldi3 _ashldi3 _ashrdi3  _ffsdi2 \
   _udiv_w_sdiv _udivmoddi4 _cmpdi2 _ucmpdi2 \
   _floatdidf _floatdisf \
   _fixunsdfsi _fixunssfsi _fixunsdfdi _fixdfdi \
   _fixunssfdi _fixsfdi \
   _fixxfdi _fixunsxfdi _floatdixf _fixunsxfsi \
   __gcc_bcmp _varargs _eprintf _op_new _new_handler \
   _op_delete  _bb _shtab _clear_cache _trampoline \
   __main _exit _ctors

sources: libgcc1 libgcc2
libgcc1:
	for name in $(LIB1FUNCS); \
        do \
	echo $${name}; \
	gcc -E -I../lib -DL$${name} libgcc1.c | \
	sed -f clean.sed >$${name}.c; \
	done


libgcc2:
	for name in $(LIB2FUNCS); \
        do \
	echo $${name}; \
	gcc -E -I../lib -DL$${name} libgcc2.c | \
	sed -f clean.sed >$${name}.c; \
	done
