# $FreeBSD$

GCCDIR=	${.CURDIR}/../../../contrib/gcc
SRCDIR=	${.CURDIR}/../../../contrib/libstdc++
SUPDIR=	${SRCDIR}/libsupc++

.PATH: ${SRCDIR}/src ${SRCDIR}/libmath ${SRCDIR}/config/io \
       ${SRCDIR}/config/locale/generic ${SRCDIR}/include \
       ${SUPDIR} ${GCCDIR}

LIB=		stdc++
SHLIB_MAJOR=	5

CFLAGS+=	-DIN_GLIBCPP_V3 -DHAVE_CONFIG_H
CFLAGS+=	-I${.CURDIR} -I${SUPDIR} -I${GCCDIR} -I${SRCDIR}/include
CFLAGS+=	-frandom-seed=RepeatabilityConsideredGood
CXXFLAGS+=	-fno-implicit-templates -ffunction-sections -fdata-sections \
		-Wno-deprecated

DPADD=		${LIBM}
LDADD=		-lm

# libstdc++ sources
SRCS+= 	allocator.cc codecvt.cc complex_io.cc ctype.cc debug.cc debug_list.cc \
	functexcept.cc globals_locale.cc globals_io.cc ios.cc ios_failure.cc \
	ios_init.cc ios_locale.cc limits.cc debug_list.cc list.cc locale.cc \
	locale_init.cc locale_facets.cc localename.cc stdexcept.cc \
	strstream.cc tree.cc allocator-inst.cc concept-inst.cc fstream-inst.cc \
	ext-inst.cc io-inst.cc istream-inst.cc locale-inst.cc \
	locale-misc-inst.cc misc-inst.cc ostream-inst.cc sstream-inst.cc \
	streambuf-inst.cc string-inst.cc valarray-inst.cc wlocale-inst.cc \
	wstring-inst.cc 

# target sources
SRCS+=  atomicity.cc codecvt_members.cc collate_members.cc ctype_members.cc \
	messages_members.cc monetary_members.cc numeric_members.cc \
	time_members.cc

# target sources extra
SRCS+=  basic_file_stdio.cc c_locale.cc

# C parts of math
SRCS+=	signbitf.c signbitl.c stubs.c

# Embedded copy of libsupc++
SRCS+=	del_op.cc del_opnt.cc del_opv.cc del_opvnt.cc eh_alloc.cc \
	eh_aux_runtime.cc eh_catch.cc eh_exception.cc eh_globals.cc \
	eh_personality.cc eh_term_handler.cc eh_terminate.cc eh_throw.cc \
	eh_type.cc eh_unex_handler.cc guard.cc new_handler.cc new_op.cc \
	new_opnt.cc new_opv.cc new_opvnt.cc pure.cc tinfo.cc tinfo2.cc \
	vec.cc vterminate.cc

# from libiberty:
SRCS+=	cp-demangle.c

# MD headers location
.if ${MACHINE_ARCH} == "sparc64"
MARCHDIR=	sparc
.elif ${MACHINE_ARCH} == "i386" && ${MACHINE_CPU} != 'i386'
MARCHDIR=	i486
.elif ${MACHINE_ARCH} == "amd64"
MARCHDIR=	i486
.else
MARCHDIR=	${MACHINE_ARCH}
.endif

ATOMICITY_H=	atomicity.h
.if exists(${SRCDIR}/config/cpu/${MARCHDIR}/atomicity.h)
ATOMICITY_H=	${SRCDIR}/config/cpu/${MARCHDIR}/atomicity.h
.else
ATOMICITY_H=	${SRCDIR}/config/cpu/generic/atomicity.h
.endif

atomicity.cc:	${ATOMICITY_H}
	ln -sf ${.ALLSRC} ${.TARGET}
CLEANFILES+=	atomicity.cc

INCSGROUPS=	BITSHDRS BKWHDRS EXTHDRS BASEHDRS BASEXHDRS STDHDRS \
		TARGETHDRS THRHDRS DEBUGHDRS
CXXINCLUDEDIR=	${INCLUDEDIR}/c++/3.4

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
STDHDRSDIR=	${CXXINCLUDEDIR}

BITSHDRS=	allocator.h atomicity.h basic_ios.h basic_ios.tcc \
		basic_string.h basic_string.tcc boost_concept_check.h \
		char_traits.h codecvt.h concept_check.h concurrence.h \
		cpp_type_traits.h deque.tcc fstream.tcc functexcept.h \
		gslice.h gslice_array.h indirect_array.h ios_base.h \
		istream.tcc list.tcc locale_classes.h locale_facets.h \
		locale_facets.tcc localefwd.h mask_array.h ostream.tcc \
		postypes.h stream_iterator.h streambuf_iterator.h \
		slice_array.h sstream.tcc stl_algo.h stl_algobase.h \
		stl_bvector.h stl_construct.h stl_deque.h stl_function.h \
		stl_heap.h stl_iterator.h stl_iterator_base_funcs.h \
		stl_iterator_base_types.h stl_list.h stl_map.h stl_multimap.h \
		stl_multiset.h stl_numeric.h stl_pair.h stl_queue.h \
		stl_raw_storage_iter.h stl_relops.h stl_set.h stl_stack.h \
		stl_tempbuf.h stl_threads.h stl_tree.h stl_uninitialized.h \
		stl_vector.h streambuf.tcc stringfwd.h type_traits.h \
		valarray_array.h valarray_array.tcc valarray_before.h \
		valarray_after.h vector.tcc
BITSHDRS:=	${BITSHDRS:S;^;${SRCDIR}/include/bits/;}
BITSHDRSDIR=	${CXXINCLUDEDIR}/bits

BKWHDRS=	complex.h iomanip.h istream.h ostream.h stream.h streambuf.h \
		algo.h algobase.h alloc.h bvector.h defalloc.h deque.h \
		function.h hash_map.h hash_set.h hashtable.h heap.h iostream.h \
		iterator.h list.h map.h multimap.h new.h multiset.h pair.h \
		queue.h rope.h set.h slist.h stack.h tempbuf.h tree.h \
		vector.h fstream.h strstream backward_warning.h
BKWHDRS:=	${BKWHDRS:S;^;${SRCDIR}/include/backward/;}
BKWHDRSDIR=	${CXXINCLUDEDIR}/backward

EXTHDRS=	algorithm bitmap_allocator.h debug_allocator.h demangle.h \
		enc_filebuf.h stdio_filebuf.h stdio_sync_filebuf.h functional \
		hash_map hash_set iterator malloc_allocator.h memory \
		mt_allocator.h new_allocator.h numeric pod_char_traits.h \
		pool_allocator.h rb_tree rope ropeimpl.h slist hash_fun.h \
		hashtable.h
EXTHDRS:=	${EXTHDRS:S;^;${SRCDIR}/include/ext/;}
EXTHDRSDIR=	${CXXINCLUDEDIR}/ext

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
BASEHDRSDIR=	${CXXINCLUDEDIR}

# Some of the different "C" header models need extra files.
BASEXHDRS=	${SRCDIR}/include/c_std/cmath.tcc
BASEXHDRSDIR=	${CXXINCLUDEDIR}/bits

DEBUGHDRS=	bitset debug.h deque formatter.h hash_map hash_map.h \
		hash_multimap.h hash_multiset.h hash_set hash_set.h list \
		map map.h multimap.h multiset.h safe_base.h safe_iterator.h \
		safe_iterator.tcc safe_sequence.h set set.h string vector
DEBUGHDRS:=	${DEBUGHDRS:S;^;${SRCDIR}/include/debug/;}
DEBUGHDRSDIR=	${CXXINCLUDEDIR}/debug

TARGETHDRS=	allocator/new_allocator_base.h \
		io/basic_file_stdio.h \
		io/c_io_stdio.h \
		locale/generic/c_locale.h \
		locale/generic/c++locale_internal.h \
		locale/generic/messages_members.h \
		locale/generic/time_members.h \
		locale/generic/codecvt_specializations.h \
		os/bsd/freebsd/ctype_base.h \
		os/bsd/freebsd/ctype_inline.h \
		os/bsd/freebsd/ctype_noninline.h \
		os/bsd/freebsd/os_defines.h
TARGETHDRS:=	${TARGETHDRS:S;^;${SRCDIR}/config/;}
TARGETHDRS+=	c++config.h
TARGETHDRSNAME_basic_file_stdio.h= basic_file.h
TARGETHDRSNAME_c_io_stdio.h= c++io.h
TARGETHDRSNAME_c_locale.h= c++locale.h
TARGETHDRSNAME_new_allocator_base.h= c++allocator.h
TARGETHDRSDIR=	${CXXINCLUDEDIR}/bits

MARCHHDRS=	atomic_word.h
.for h in ${MARCHHDRS}
.if exists(${SRCDIR}/config/cpu/${MARCHDIR}/${h})
TARGETHDRS+=	${SRCDIR}/config/cpu/${MARCHDIR}/${h}
.else
TARGETHDRS+=	${SRCDIR}/config/cpu/generic/${h}
.endif
.endfor

THRHDRS=	gthr.h gthr-single.h gthr-posix.h gthr-default.h
THRHDRSDIR=	${CXXINCLUDEDIR}/bits
uppercase =	[ABCDEFGHIJKLMNOPQRSTUVWXYZ_]

gthr.h: ${GCCDIR}/gthr.h
	sed -e '/^#/s/\(${uppercase}${uppercase}*\)/_GLIBCXX_\1/g' \
	    -e 's/_GLIBCXX_SUPPORTS_WEAK/__GXX_WEAK__/g' \
	    -e 's,^#include "\(.*\)",#include <bits/\1>,g' \
	    < ${.ALLSRC} > ${.TARGET}

gthr-single.h: ${GCCDIR}/gthr-single.h
	sed -e 's/\(UNUSED\)/_GLIBCXX_\1/g' \
	    -e 's/\(GCC${uppercase}*_H\)/_GLIBCXX_\1/g' \
	    < ${.ALLSRC} > ${.TARGET}

gthr-posix.h: ${GCCDIR}/gthr-posix.h
	sed -e 's/\(UNUSED\)/_GLIBCXX_\1/g' \
	    -e 's/\(GCC${uppercase}*_H\)/_GLIBCXX_\1/g' \
	    -e 's/SUPPORTS_WEAK/__GXX_WEAK__/g' \
	    -e 's/\(${uppercase}*USE_WEAK\)/_GLIBCXX_\1/g' \
	    < ${.ALLSRC} > ${.TARGET}

gthr-default.h: ${GCCDIR}/gthr-posix.h
	sed -e 's/\(UNUSED\)/_GLIBCXX_\1/g' \
	    -e 's/\(GCC${uppercase}*_H\)/_GLIBCXX_\1/g' \
	    -e 's/SUPPORTS_WEAK/__GXX_WEAK__/g' \
	    -e 's/\(${uppercase}*USE_WEAK\)/_GLIBCXX_\1/g' \
	    -e 's,^#include "\(.*\)",#include <bits/\1>,g' \
	    < ${.ALLSRC} > ${.TARGET}

CLEANFILES+=	${THRHDRS}

c++config.h: ${.CURDIR}/config.h ${SRCDIR}/include/bits/c++config
	cat ${SRCDIR}/include/bits/c++config > ${.TARGET} && \
	sed -e 's/HAVE_/_GLIBCXX_HAVE_/g' \
	    -e 's/PACKAGE/_GLIBCXX_PACKAGE/g' \
	    -e 's/VERSION/_GLIBCXX_VERSION/g' \
	    -e 's/WORDS_/_GLIBCXX_WORDS_/g' \
	    < ${.CURDIR}/config.h >> ${.TARGET} && \
	echo "#endif // _CXXCONFIG_" >> ${.TARGET}
CLEANFILES+=	c++config.h

.include <bsd.lib.mk>
