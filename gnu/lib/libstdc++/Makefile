# $FreeBSD$

SRCDIR=	${.CURDIR}/../../../contrib/libstdc++-v3/src
.PATH: ${SRCDIR}/src ${SRCDIR}/libmath ${SRCDIR}/config/io \
       ${SRCDIR}/config/locale/generic ${SRCDIR} ${SRCDIR}/std

SUBDIR=	doc

LIB=		stdc++
SHLIB_MAJOR=	4
SHLIB_MINOR=	0

CFLAGS+=	-DHAVE_CONFIG_H
CFLAGS+=	-I${.CURDIR} -I${SRCDIR}/include -I.
CXXFLAGS+=	-nostdinc++ \
		-fno-implicit-templates -ffunction-sections -fdata-sections
LDFLAGS+=	-version-info 4:0:0 -Wl,--version-script=linker.map

LDADD+=		-lm -lsupc++
DPADD+=		${LIBM} ${LIBSUPCXX}

# libstdc++ sources (libstdc++-v3/src/Makefile.in, do not reorder)
SRCS+=	globals.cc limits.cc basic_file_stdio.cc complex_io.cc ios.cc \
	strstream.cc cmath.cc bitset.cc functexcept.cc stdexcept.cc \
	vterminate.cc c_locale.cc locale.cc localename.cc codecvt.cc \
	collate_members.cc ctype_members.cc messages_members.cc \
	monetary_members.cc numeric_members.cc time_members.cc concept-inst.cc \
	locale-inst.cc misc-inst.cc stl-inst.cc string-inst.cc wstring-inst.cc \
	valarray-inst.cc ext-inst.cc

# C parts of math
SRCS+=	nan.c signbit.c signbitf.c signbitl.c # stubs.c

#------------------------------------------------------------------------------
#	From libstdc++-v3/include/Makefile.am

BITHDRS=basic_ios.h basic_ios.tcc basic_string.h basic_string.tcc \
	boost_concept_check.h char_traits.h codecvt.h concept_check.h \
	cpp_type_traits.h fpos.h fstream.tcc functexcept.h generic_shadow.h \
	gslice.h gslice_array.h indirect_array.h ios_base.h istream.tcc \
	locale_facets.h locale_facets.tcc localefwd.h mask_array.h ostream.tcc \
	pthread_allocimpl.h stream_iterator.h streambuf_iterator.h slice.h \
	slice_array.h sstream.tcc \
	stl_algo.h stl_algobase.h stl_alloc.h stl_bvector.h stl_construct.h \
	stl_deque.h stl_function.h stl_heap.h stl_iterator.h \
	stl_iterator_base_funcs.h stl_iterator_base_types.h stl_list.h \
	stl_map.h stl_multimap.h stl_multiset.h stl_numeric.h stl_pair.h \
	stl_pthread_alloc.h stl_queue.h stl_raw_storage_iter.h stl_relops.h \
	stl_set.h stl_stack.h stl_tempbuf.h stl_threads.h stl_tree.h \
	stl_uninitialized.h stl_vector.h \
	streambuf.tcc stringfwd.h type_traits.h valarray_array.h \
	valarray_array.tcc valarray_meta.h

BKWHDRS=complex.h iomanip.h istream.h ostream.h stream.h streambuf.h algo.h \
	algobase.h alloc.h bvector.h defalloc.h deque.h function.h hash_map.h \
	hash_set.h hashtable.h heap.h iostream.h iterator.h list.h map.h \
	multimap.h new.h multiset.h pair.h queue.h rope.h set.h slist.h \
	stack.h tempbuf.h tree.h vector.h fstream.h strstream.h strstream \
	backward_warning.h

EXTHDRS=algorithm functional hash_map hash_set iterator memory numeric \
	rb_tree rope ropeimpl.h slist stl_hash_fun.h stl_hashtable.h stl_rope.h

# This is the common subset of files that all three "C" header models use.
BASEDHDRS=std_cassert.h std_cctype.h std_cerrno.h std_cfloat.h std_ciso646.h \
	std_climits.h std_clocale.h std_cmath.h std_csetjmp.h std_csignal.h \
	std_cstdarg.h std_cstddef.h std_cstdio.h std_cstdlib.h std_cstring.h \
	std_ctime.h std_cwchar.h std_cwctype.h \
	cmath.tcc

STDHDRS=std_algorithm.h std_bitset.h std_complex.h std_deque.h std_fstream.h \
	std_functional.h std_iomanip.h std_ios.h std_iosfwd.h std_iostream.h \
	std_istream.h std_iterator.h std_limits.h std_list.h std_locale.h \
	std_map.h std_memory.h std_numeric.h std_ostream.h std_queue.h \
	std_set.h std_sstream.h std_stack.h std_stdexcept.h std_streambuf.h \
	std_string.h std_utility.h std_valarray.h std_vector.h

TGTHDRS=ctype_base.h ctype_inline.h ctype_noninline.h os_defines.h \
	atomicity.h cpu_limits.h 

# These extra_target_headers files are all built with ad hoc naming rules.
XTGHDRS=basic_file.h c++config.h c++io.h c++locale.h messages_members.h \
	codecvt_specializations.h 

THDHDRS=gthr.h gthr-single.h gthr-posix.h gthr-default.h 

BASEDHDRS_renamed=	${BASEDHDRS:Mstd_*:S/std_//g:S/.h//g}
STDHDRS_renamed=	${STDHDRS:Mstd_*:S/std_//g:S/.h//g}

INCS=		_G_config.h ${HDRS:S;^;${SRCDIR}/;} ${IHDRS} ${STLHDRS}

INCSGROUPS=	INCS STDHDRS
INCSDIR=	${INCLUDEDIR}/g++
STDHDRSDIR=	${INCLUDEDIR}/g++/std

.include <bsd.lib.mk>
