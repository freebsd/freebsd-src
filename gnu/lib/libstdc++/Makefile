# $FreeBSD$

GCCDIR=	${.CURDIR}/../../../contrib/gcc
SRCDIR=	${.CURDIR}/../../../contrib/libstdc++
SUPDIR=	${SRCDIR}/libsupc++

.PATH: ${SRCDIR}/src ${SRCDIR}/libmath ${SRCDIR}/config/io \
       ${SRCDIR}/config/locale/generic ${SRCDIR} ${SRCDIR}/std \
       ${SUPDIR} ${GCCDIR}

#SUBDIR=	doc

LIB=		stdc++
.if ${MACHINE_ARCH} != "sparc64"
SHLIB_MAJOR=	4
SHLIB_MINOR=	0
.endif

CFLAGS+=	-DIN_GLIBCPP_V3 -DHAVE_CONFIG_H
CFLAGS+=	-I${.CURDIR} -I${SUPDIR} -I${GCCDIR}
CXXFLAGS+=	-fno-implicit-templates -ffunction-sections -fdata-sections -Wno-deprecated
#LDFLAGS+=	-Wl,-version-info,4:0:0 -Wl,--version-script=${SRCDIR}/src/linker.map

LDADD+=		-lm
DPADD+=		${LIBM}

# libstdc++ sources (libstdc++/src/Makefile.in, do not reorder)
SRCS+=	globals.cc limits.cc basic_file_stdio.cc complex_io.cc ios.cc \
	strstream-fixed.cc cmath.cc bitset.cc functexcept.cc stdexcept.cc \
	vterminate.cc c_locale.cc locale.cc localename.cc codecvt.cc \
	collate_members.cc ctype_members.cc messages_members.cc \
	monetary_members.cc numeric_members.cc time_members.cc concept-inst.cc \
	locale-inst.cc misc-inst.cc stl-inst.cc string-inst.cc wstring-inst.cc \
	valarray-inst.cc ext-inst.cc fstream.cc fstream-inst.cc io-inst.cc \
	istream-inst.cc ostream-inst.cc streambuf-inst.cc sstream-inst.cc

# C parts of math
SRCS+=	nan.c signbit.c signbitf.c signbitl.c stubs.c

# Embedded copy of libsupc++
SRCS+=	del_op.cc del_opnt.cc del_opv.cc del_opvnt.cc \
	eh_alloc.cc eh_aux_runtime.cc eh_catch.cc eh_exception.cc \
	eh_globals.cc eh_personality.cc eh_terminate.cc eh_throw.cc eh_type.cc \
	new_handler.cc new_op.cc new_opnt.cc new_opv.cc new_opvnt.cc \
	pure.cc tinfo.cc tinfo2.cc vec.cc

# from libiberty:
SRCS+=	cp-demangle.c dyn-string.c

strstream-fixed.cc:	${SRCDIR}/src/strstream.cc
	sed -e 's,^#include <strstream>,#include <backward/strstream>,g' \
	    < ${SRCDIR}/src/strstream.cc > strstream-fixed.cc

CLEANFILES+=	strstream-fixed.cc

INCSGROUPS=	BITSHDRS BKWHDRS EXTHDRS BASEHDRS BASEXHDRS STDHDRS \
		TARGETHDRS THRHDRS

BITSHDRS=	basic_ios.h basic_ios.tcc basic_string.h basic_string.tcc \
		boost_concept_check.h char_traits.h codecvt.h concept_check.h \
		cpp_type_traits.h fpos.h fstream.tcc functexcept.h \
		generic_shadow.h gslice.h gslice_array.h indirect_array.h \
		ios_base.h istream.tcc locale_facets.h locale_facets.tcc \
		localefwd.h mask_array.h ostream.tcc pthread_allocimpl.h \
		slice.h slice_array.h sstream.tcc stl_algo.h stl_algobase.h \
		stl_alloc.h stl_bvector.h stl_construct.h stl_deque.h \
		stl_function.h stl_heap.h stl_iterator.h \
		stl_iterator_base_funcs.h stl_iterator_base_types.h \
		stl_list.h stl_map.h stl_multimap.h stl_multiset.h \
		stl_numeric.h stl_pair.h stl_pthread_alloc.h stl_queue.h \
		stl_raw_storage_iter.h stl_relops.h stl_set.h stl_stack.h \
		stl_tempbuf.h stl_threads.h stl_tree.h stl_uninitialized.h \
		stl_vector.h stream_iterator.h streambuf.tcc \
		streambuf_iterator.h stringfwd.h type_traits.h \
		valarray_array.h valarray_array.tcc valarray_meta.h
BITSHDRS:=	${BITSHDRS:S;^;${SRCDIR}/include/bits/;}
BITSHDRSDIR=	${INCLUDEDIR}/g++/bits

BKWHDRS=	algo.h algobase.h alloc.h backward_warning.h bvector.h \
		complex.h defalloc.h deque.h fstream.h function.h \
		hash_map.h hash_set.h hashtable.h heap.h iomanip.h \
		iostream.h istream.h iterator.h list.h map.h multimap.h \
		multiset.h new.h ostream.h pair.h queue.h rope.h set.h \
		slist.h stack.h stream.h streambuf.h strstream strstream.h \
		tempbuf.h tree.h vector.h
BKWHDRS:=	${BKWHDRS:S;^;${SRCDIR}/include/backward/;}
BKWHDRSDIR=	${INCLUDEDIR}/g++/backward

EXTHDRS=	algorithm enc_filebuf.h functional hash_map hash_set \
		iterator memory numeric rb_tree rope ropeimpl.h slist \
		stdio_filebuf.h stl_hash_fun.h stl_hashtable.h stl_rope.h
EXTHDRS:=	${EXTHDRS:S;^;${SRCDIR}/include/ext/;}
EXTHDRSDIR=	${INCLUDEDIR}/g++/ext

# This is the common subset of files that all three "C" header models use.
BASEHDRS=	std_cassert.h std_cctype.h std_cerrno.h std_cfloat.h \
		std_ciso646.h std_climits.h std_clocale.h std_cmath.h \
		std_csetjmp.h std_csignal.h std_cstdarg.h std_cstddef.h \
		std_cstdio.h std_cstdlib.h std_cstring.h std_ctime.h \
		std_cwchar.h std_cwctype.h
.for h in ${BASEHDRS}
BASEHDRSNAME_$h=${h:R:S;^std_;;}
.endfor
BASEHDRS:=	${BASEHDRS:S;^;${SRCDIR}/include/c_std/;}
BASEHDRSDIR=	${INCLUDEDIR}/g++

# Some of the different "C" header models need extra files.
BASEXHDRS=	${SRCDIR}/include/c_std/cmath.tcc
BASEXHDRSDIR=	${INCLUDEDIR}/g++/bits

STDHDRS=	std_algorithm.h std_bitset.h std_complex.h std_deque.h \
		std_fstream.h std_functional.h std_iomanip.h std_ios.h \
		std_iosfwd.h std_iostream.h std_istream.h std_iterator.h \
		std_limits.h std_list.h std_locale.h std_map.h std_memory.h \
		std_numeric.h std_ostream.h std_queue.h std_set.h \
		std_sstream.h std_stack.h std_stdexcept.h std_streambuf.h \
		std_string.h std_utility.h std_valarray.h std_vector.h
.for h in ${STDHDRS}
STDHDRSNAME_$h=${h:R:S;^std_;;}
.endfor
STDHDRS:=	${STDHDRS:S;^;${SRCDIR}/include/std/;}
STDHDRSDIR=	${INCLUDEDIR}/g++

TARGETHDRS=	io/basic_file_stdio.h io/c_io_stdio.h \
		locale/generic/c_locale.h \
		locale/generic/codecvt_specializations.h \
		locale/generic/messages_members.h \
		os/bsd/freebsd/bits/ctype_base.h \
		os/bsd/freebsd/bits/ctype_inline.h \
		os/bsd/freebsd/bits/ctype_noninline.h \
		os/bsd/freebsd/bits/os_defines.h
TARGETHDRS:=	${TARGETHDRS:S;^;${SRCDIR}/config/;}
TARGETHDRS+=	${.CURDIR}/c++config.h
TARGETHDRSNAME_basic_file_stdio.h= basic_file.h
TARGETHDRSNAME_c_io_stdio.h= c++io.h
TARGETHDRSNAME_c_locale.h= c++locale.h
TARGETHDRSDIR=	${INCLUDEDIR}/g++/bits

MARCHHDRS=	atomicity.h cpu_limits.h
.if ${MACHINE_ARCH} == "sparc64"
MARCHDIR=	${SRCDIR}/config/cpu/sparc/${MACHINE_ARCH}/bits
.else
MARCHDIR=	${SRCDIR}/config/cpu/${MACHINE_ARCH}/bits
.endif
.for h in ${MARCHHDRS}
.if exists(${MARCHDIR}/${h})
TARGETHDRS+=	${MARCHDIR}/${h}
.else
TARGETHDRS+=	${SRCDIR}/config/cpu/generic/bits/${h}
.endif
.endfor

THRHDRS=	gthr.h gthr-single.h gthr-posix.h gthr-default.h
THRHDRSDIR=	${INCLUDEDIR}/g++/bits

gthr.h: ${GCCDIR}/gthr.h
	sed -e '/^#/s/\([ABCDEFGHIJKLMNOPQRSTUVWXYZ_][ABCDEFGHIJKLMNOPQRSTUVWXYZ_]*\)/_GLIBCPP_\1/g' \
	    -e 's,^#include "\(.*\)",#include <bits/\1>,g' \
	    < ${GCCDIR}/gthr.h > gthr.h

gthr-single.h: ${GCCDIR}/gthr-single.h
	sed -e 's/\(UNUSED\)/_GLIBCPP_\1/g' \
	    -e 's/\(GCC[ABCDEFGHIJKLMNOPQRSTUVWXYZ_]*_H\)/_GLIBCPP_\1/g' \
	    < ${GCCDIR}/gthr-single.h > gthr-single.h

gthr-posix.h: ${GCCDIR}/gthr-posix.h
	sed -e 's/\(UNUSED\)/_GLIBCPP_\1/g' \
	    -e 's/\(GCC[ABCDEFGHIJKLMNOPQRSTUVWXYZ_]*_H\)/_GLIBCPP_\1/g' \
	    -e 's/\([ABCDEFGHIJKLMNOPQRSTUVWXYZ_]*WEAK\)/_GLIBCPP_\1/g' \
	    < ${GCCDIR}/gthr-posix.h > gthr-posix.h

gthr-default.h: ${GCCDIR}/gthr-posix.h
	sed -e 's/\(UNUSED\)/_GLIBCPP_\1/g' \
	    -e 's/\(GCC[ABCDEFGHIJKLMNOPQRSTUVWXYZ_]*_H\)/_GLIBCPP_\1/g' \
	    -e 's/\([ABCDEFGHIJKLMNOPQRSTUVWXYZ_]*WEAK\)/_GLIBCPP_\1/g' \
	    -e 's,^#include "\(.*\)",#include <bits/\1>,g' \
	    < ${GCCDIR}/gthr-posix.h > gthr-default.h

CLEANFILES+=	${THRHDRS}

.include <bsd.lib.mk>
