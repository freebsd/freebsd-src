# $FreeBSD$

SHLIBDIR?=	/lib

.include <src.opts.mk>

PACKAGE=	clibs
_LIBCXXRTDIR=	${SRCTOP}/contrib/libcxxrt
HDRDIR=		${SRCTOP}/contrib/llvm-project/libcxx/include
SRCDIR=		${SRCTOP}/contrib/llvm-project/libcxx/src
CXXINCLUDEDIR=	${INCLUDEDIR}/c++/v${SHLIB_MAJOR}

.PATH: ${SRCDIR}

LIB=		c++
SHLIB_MAJOR=	1
SHLIB_LDSCRIPT=	libc++.ldscript

SRCS+=		algorithm.cpp
SRCS+=		any.cpp
SRCS+=		atomic.cpp
SRCS+=		barrier.cpp
SRCS+=		bind.cpp
SRCS+=		charconv.cpp
SRCS+=		chrono.cpp
SRCS+=		condition_variable.cpp
SRCS+=		condition_variable_destructor.cpp
SRCS+=		debug.cpp
SRCS+=		exception.cpp
SRCS+=		filesystem/directory_iterator.cpp
SRCS+=		filesystem/int128_builtins.cpp
SRCS+=		filesystem/operations.cpp
SRCS+=		format.cpp
SRCS+=		functional.cpp
SRCS+=		future.cpp
SRCS+=		hash.cpp
SRCS+=		ios.cpp
SRCS+=		ios.instantiations.cpp
SRCS+=		iostream.cpp
SRCS+=		locale.cpp
SRCS+=		memory.cpp
SRCS+=		mutex.cpp
SRCS+=		mutex_destructor.cpp
SRCS+=		new.cpp
SRCS+=		optional.cpp
SRCS+=		random.cpp
SRCS+=		random_shuffle.cpp
SRCS+=		regex.cpp
SRCS+=		shared_mutex.cpp
SRCS+=		stdexcept.cpp
SRCS+=		string.cpp
SRCS+=		strstream.cpp
SRCS+=		system_error.cpp
SRCS+=		thread.cpp
SRCS+=		typeinfo.cpp
SRCS+=		utility.cpp
SRCS+=		valarray.cpp
SRCS+=		variant.cpp
SRCS+=		vector.cpp

CXXRT_SRCS+=	auxhelper.cc
CXXRT_SRCS+=	dynamic_cast.cc
CXXRT_SRCS+=	exception.cc
CXXRT_SRCS+=	guard.cc
CXXRT_SRCS+=	libelftc_dem_gnu3.c
CXXRT_SRCS+=	memory.cc
CXXRT_SRCS+=	stdexcept.cc
CXXRT_SRCS+=	terminate.cc
CXXRT_SRCS+=	typeinfo.cc

.for _S in ${CXXRT_SRCS}
CLEANFILES+=	cxxrt_${_S}
STATICOBJS+=	cxxrt_${_S:R}.o
cxxrt_${_S}: ${_LIBCXXRTDIR}/${_S} .NOMETA
	ln -sf ${.ALLSRC} ${.TARGET}
.endfor

WARNS?=		0
CFLAGS+=	-isystem ${.CURDIR}
CFLAGS+=	-isystem ${HDRDIR}
CFLAGS+=	-isystem ${_LIBCXXRTDIR}
CFLAGS+=	-nostdinc++
CFLAGS+=	-nostdlib
CFLAGS+=	-D_LIBCPP_BUILDING_LIBRARY
CFLAGS+=	-DLIBCXXRT
CFLAGS+=	-ffunction-sections
CFLAGS+=	-fdata-sections
CXXSTD?=	c++14

LIBADD+=	cxxrt

INCSGROUPS+=	STD
STDDIR=		${CXXINCLUDEDIR}
STD_HEADERS+=	__availability
STD_HEADERS+=	__bit_reference
STD_HEADERS+=	__bits
STD_HEADERS+=	__bsd_locale_defaults.h
STD_HEADERS+=	__bsd_locale_fallbacks.h
STD_HEADERS+=	__config
STD_HEADERS+=	__debug
STD_HEADERS+=	__errc
STD_HEADERS+=	__function_like.h
STD_HEADERS+=	__functional_base
STD_HEADERS+=	__hash_table
STD_HEADERS+=	__libcpp_version
STD_HEADERS+=	__locale
STD_HEADERS+=	__mutex_base
STD_HEADERS+=	__node_handle
STD_HEADERS+=	__nullptr
STD_HEADERS+=	__split_buffer
STD_HEADERS+=	__std_stream
STD_HEADERS+=	__string
STD_HEADERS+=	__threading_support
STD_HEADERS+=	__tree
STD_HEADERS+=	__tuple
STD_HEADERS+=	__undef_macros
STD_HEADERS+=	algorithm
STD_HEADERS+=	any
STD_HEADERS+=	array
STD_HEADERS+=	atomic
STD_HEADERS+=	barrier
STD_HEADERS+=	bit
STD_HEADERS+=	bitset
STD_HEADERS+=	cassert
STD_HEADERS+=	ccomplex
STD_HEADERS+=	cctype
STD_HEADERS+=	cerrno
STD_HEADERS+=	cfenv
STD_HEADERS+=	cfloat
STD_HEADERS+=	charconv
STD_HEADERS+=	chrono
STD_HEADERS+=	cinttypes
STD_HEADERS+=	ciso646
STD_HEADERS+=	climits
STD_HEADERS+=	clocale
STD_HEADERS+=	cmath
STD_HEADERS+=	codecvt
STD_HEADERS+=	compare
STD_HEADERS+=	complex
STD_HEADERS+=	complex.h
STD_HEADERS+=	concepts
STD_HEADERS+=	condition_variable
STD_HEADERS+=	csetjmp
STD_HEADERS+=	csignal
STD_HEADERS+=	cstdarg
STD_HEADERS+=	cstdbool
STD_HEADERS+=	cstddef
STD_HEADERS+=	cstdint
STD_HEADERS+=	cstdio
STD_HEADERS+=	cstdlib
STD_HEADERS+=	cstring
STD_HEADERS+=	ctgmath
STD_HEADERS+=	ctime
STD_HEADERS+=	ctype.h
STD_HEADERS+=	cwchar
STD_HEADERS+=	cwctype
STD_HEADERS+=	deque
STD_HEADERS+=	errno.h
STD_HEADERS+=	exception
STD_HEADERS+=	execution
STD_HEADERS+=	fenv.h
STD_HEADERS+=	filesystem
STD_HEADERS+=	float.h
STD_HEADERS+=	format
STD_HEADERS+=	forward_list
STD_HEADERS+=	fstream
STD_HEADERS+=	functional
STD_HEADERS+=	future
STD_HEADERS+=	initializer_list
STD_HEADERS+=	inttypes.h
STD_HEADERS+=	iomanip
STD_HEADERS+=	ios
STD_HEADERS+=	iosfwd
STD_HEADERS+=	iostream
STD_HEADERS+=	istream
STD_HEADERS+=	iterator
STD_HEADERS+=	latch
STD_HEADERS+=	limits
STD_HEADERS+=	limits.h
STD_HEADERS+=	list
STD_HEADERS+=	locale
STD_HEADERS+=	locale.h
STD_HEADERS+=	map
STD_HEADERS+=	math.h
STD_HEADERS+=	memory
STD_HEADERS+=	module.modulemap
STD_HEADERS+=	mutex
STD_HEADERS+=	new
STD_HEADERS+=	numbers
STD_HEADERS+=	numeric
STD_HEADERS+=	optional
STD_HEADERS+=	ostream
STD_HEADERS+=	queue
STD_HEADERS+=	random
STD_HEADERS+=	ranges
STD_HEADERS+=	ratio
STD_HEADERS+=	regex
STD_HEADERS+=	scoped_allocator
STD_HEADERS+=	semaphore
STD_HEADERS+=	set
STD_HEADERS+=	setjmp.h
STD_HEADERS+=	shared_mutex
STD_HEADERS+=	span
STD_HEADERS+=	sstream
STD_HEADERS+=	stack
STD_HEADERS+=	stdbool.h
STD_HEADERS+=	stddef.h
STD_HEADERS+=	stdexcept
STD_HEADERS+=	stdint.h
STD_HEADERS+=	stdio.h
STD_HEADERS+=	stdlib.h
STD_HEADERS+=	streambuf
STD_HEADERS+=	string
STD_HEADERS+=	string.h
STD_HEADERS+=	string_view
STD_HEADERS+=	strstream
STD_HEADERS+=	system_error
STD_HEADERS+=	tgmath.h
STD_HEADERS+=	thread
STD_HEADERS+=	tuple
STD_HEADERS+=	type_traits
STD_HEADERS+=	typeindex
STD_HEADERS+=	typeinfo
STD_HEADERS+=	unordered_map
STD_HEADERS+=	unordered_set
STD_HEADERS+=	utility
STD_HEADERS+=	valarray
STD_HEADERS+=	variant
STD_HEADERS+=	vector
STD_HEADERS+=	version
STD_HEADERS+=	wchar.h
STD_HEADERS+=	wctype.h
.for hdr in ${STD_HEADERS}
STD+=		${HDRDIR}/${hdr}
.endfor

# Special case for __config_site, which as of libc++ 12.0.0 is generated by
# CMake, and as of 13.0.0 installed side-by-side with__config.
STD+=		${.CURDIR}/__config_site

RT_HEADERS+=	cxxabi.h
.for hdr in ${RT_HEADERS}
STD+=		${_LIBCXXRTDIR}/${hdr}
.endfor

INCSGROUPS+=	ALG
ALGDIR=		${CXXINCLUDEDIR}/__algorithm
ALG_HEADERS+=	adjacent_find.h
ALG_HEADERS+=	all_of.h
ALG_HEADERS+=	any_of.h
ALG_HEADERS+=	binary_search.h
ALG_HEADERS+=	clamp.h
ALG_HEADERS+=	comp.h
ALG_HEADERS+=	comp_ref_type.h
ALG_HEADERS+=	copy.h
ALG_HEADERS+=	copy_backward.h
ALG_HEADERS+=	copy_if.h
ALG_HEADERS+=	copy_n.h
ALG_HEADERS+=	count.h
ALG_HEADERS+=	count_if.h
ALG_HEADERS+=	equal.h
ALG_HEADERS+=	equal_range.h
ALG_HEADERS+=	fill.h
ALG_HEADERS+=	fill_n.h
ALG_HEADERS+=	find.h
ALG_HEADERS+=	find_end.h
ALG_HEADERS+=	find_first_of.h
ALG_HEADERS+=	find_if.h
ALG_HEADERS+=	find_if_not.h
ALG_HEADERS+=	for_each.h
ALG_HEADERS+=	for_each_n.h
ALG_HEADERS+=	generate.h
ALG_HEADERS+=	generate_n.h
ALG_HEADERS+=	half_positive.h
ALG_HEADERS+=	includes.h
ALG_HEADERS+=	inplace_merge.h
ALG_HEADERS+=	is_heap.h
ALG_HEADERS+=	is_heap_until.h
ALG_HEADERS+=	is_partitioned.h
ALG_HEADERS+=	is_permutation.h
ALG_HEADERS+=	is_sorted.h
ALG_HEADERS+=	is_sorted_until.h
ALG_HEADERS+=	iter_swap.h
ALG_HEADERS+=	lexicographical_compare.h
ALG_HEADERS+=	lower_bound.h
ALG_HEADERS+=	make_heap.h
ALG_HEADERS+=	max.h
ALG_HEADERS+=	max_element.h
ALG_HEADERS+=	merge.h
ALG_HEADERS+=	min.h
ALG_HEADERS+=	min_element.h
ALG_HEADERS+=	minmax.h
ALG_HEADERS+=	minmax_element.h
ALG_HEADERS+=	mismatch.h
ALG_HEADERS+=	move.h
ALG_HEADERS+=	move_backward.h
ALG_HEADERS+=	next_permutation.h
ALG_HEADERS+=	none_of.h
ALG_HEADERS+=	nth_element.h
ALG_HEADERS+=	partial_sort.h
ALG_HEADERS+=	partial_sort_copy.h
ALG_HEADERS+=	partition.h
ALG_HEADERS+=	partition_copy.h
ALG_HEADERS+=	partition_point.h
ALG_HEADERS+=	pop_heap.h
ALG_HEADERS+=	prev_permutation.h
ALG_HEADERS+=	push_heap.h
ALG_HEADERS+=	remove.h
ALG_HEADERS+=	remove_copy.h
ALG_HEADERS+=	remove_copy_if.h
ALG_HEADERS+=	remove_if.h
ALG_HEADERS+=	replace.h
ALG_HEADERS+=	replace_copy.h
ALG_HEADERS+=	replace_copy_if.h
ALG_HEADERS+=	replace_if.h
ALG_HEADERS+=	reverse.h
ALG_HEADERS+=	reverse_copy.h
ALG_HEADERS+=	rotate.h
ALG_HEADERS+=	rotate_copy.h
ALG_HEADERS+=	sample.h
ALG_HEADERS+=	search.h
ALG_HEADERS+=	search_n.h
ALG_HEADERS+=	set_difference.h
ALG_HEADERS+=	set_intersection.h
ALG_HEADERS+=	set_symmetric_difference.h
ALG_HEADERS+=	set_union.h
ALG_HEADERS+=	shift_left.h
ALG_HEADERS+=	shift_right.h
ALG_HEADERS+=	shuffle.h
ALG_HEADERS+=	sift_down.h
ALG_HEADERS+=	sort.h
ALG_HEADERS+=	sort_heap.h
ALG_HEADERS+=	stable_partition.h
ALG_HEADERS+=	stable_sort.h
ALG_HEADERS+=	swap_ranges.h
ALG_HEADERS+=	transform.h
ALG_HEADERS+=	unique.h
ALG_HEADERS+=	unique_copy.h
ALG_HEADERS+=	unwrap_iter.h
ALG_HEADERS+=	upper_bound.h
.for hdr in ${ALG_HEADERS}
ALG+=		${HDRDIR}/__algorithm/${hdr}
.endfor

INCSGROUPS+=	FMT
FMTDIR=		${CXXINCLUDEDIR}/__format
FMT_HEADERS+=	format_error.h
FMT_HEADERS+=	format_parse_context.h
.for hdr in ${FMT_HEADERS}
FMT+=		${HDRDIR}/__format/${hdr}
.endfor

INCSGROUPS+=	FUN
FUNDIR=		${CXXINCLUDEDIR}/__functional
FUN_HEADERS+=	binary_function.h
FUN_HEADERS+=	binary_negate.h
FUN_HEADERS+=	bind.h
FUN_HEADERS+=	bind_front.h
FUN_HEADERS+=	binder1st.h
FUN_HEADERS+=	binder2nd.h
FUN_HEADERS+=	default_searcher.h
FUN_HEADERS+=	function.h
FUN_HEADERS+=	hash.h
FUN_HEADERS+=	identity.h
FUN_HEADERS+=	invoke.h
FUN_HEADERS+=	is_transparent.h
FUN_HEADERS+=	mem_fn.h
FUN_HEADERS+=	mem_fun_ref.h
FUN_HEADERS+=	not_fn.h
FUN_HEADERS+=	operations.h
FUN_HEADERS+=	perfect_forward.h
FUN_HEADERS+=	pointer_to_binary_function.h
FUN_HEADERS+=	pointer_to_unary_function.h
FUN_HEADERS+=	ranges_operations.h
FUN_HEADERS+=	reference_wrapper.h
FUN_HEADERS+=	unary_function.h
FUN_HEADERS+=	unary_negate.h
FUN_HEADERS+=	unwrap_ref.h
FUN_HEADERS+=	weak_result_type.h
.for hdr in ${FUN_HEADERS}
FUN+=		${HDRDIR}/__functional/${hdr}
.endfor

INCSGROUPS+=	IT
ITDIR=		${CXXINCLUDEDIR}/__iterator
IT_HEADERS+=	access.h
IT_HEADERS+=	advance.h
IT_HEADERS+=	back_insert_iterator.h
IT_HEADERS+=	common_iterator.h
IT_HEADERS+=	concepts.h
IT_HEADERS+=	counted_iterator.h
IT_HEADERS+=	data.h
IT_HEADERS+=	default_sentinel.h
IT_HEADERS+=	distance.h
IT_HEADERS+=	empty.h
IT_HEADERS+=	erase_if_container.h
IT_HEADERS+=	front_insert_iterator.h
IT_HEADERS+=	incrementable_traits.h
IT_HEADERS+=	insert_iterator.h
IT_HEADERS+=	istream_iterator.h
IT_HEADERS+=	istreambuf_iterator.h
IT_HEADERS+=	iter_move.h
IT_HEADERS+=	iter_swap.h
IT_HEADERS+=	iterator.h
IT_HEADERS+=	iterator_traits.h
IT_HEADERS+=	move_iterator.h
IT_HEADERS+=	next.h
IT_HEADERS+=	ostream_iterator.h
IT_HEADERS+=	ostreambuf_iterator.h
IT_HEADERS+=	prev.h
IT_HEADERS+=	projected.h
IT_HEADERS+=	readable_traits.h
IT_HEADERS+=	reverse_access.h
IT_HEADERS+=	reverse_iterator.h
IT_HEADERS+=	size.h
IT_HEADERS+=	wrap_iter.h
.for hdr in ${IT_HEADERS}
IT+=		${HDRDIR}/__iterator/${hdr}
.endfor

INCSGROUPS+=	MEM
MEMDIR=		${CXXINCLUDEDIR}/__memory
MEM_HEADERS+=	addressof.h
MEM_HEADERS+=	allocation_guard.h
MEM_HEADERS+=	allocator.h
MEM_HEADERS+=	allocator_arg_t.h
MEM_HEADERS+=	allocator_traits.h
MEM_HEADERS+=	auto_ptr.h
MEM_HEADERS+=	compressed_pair.h
MEM_HEADERS+=	construct_at.h
MEM_HEADERS+=	pointer_safety.h
MEM_HEADERS+=	pointer_traits.h
MEM_HEADERS+=	raw_storage_iterator.h
MEM_HEADERS+=	shared_ptr.h
MEM_HEADERS+=	temporary_buffer.h
MEM_HEADERS+=	uninitialized_algorithms.h
MEM_HEADERS+=	unique_ptr.h
MEM_HEADERS+=	uses_allocator.h
.for hdr in ${MEM_HEADERS}
MEM+=		${HDRDIR}/__memory/${hdr}
.endfor

INCSGROUPS+=	RND
RNDDIR=		${CXXINCLUDEDIR}/__random
RND_HEADERS+=	uniform_int_distribution.h
.for hdr in ${RND_HEADERS}
RND+=		${HDRDIR}/__random/${hdr}
.endfor

INCSGROUPS+=	RNG
RNGDIR=		${CXXINCLUDEDIR}/__ranges
RNG_HEADERS+=	access.h
RNG_HEADERS+=	all.h
RNG_HEADERS+=	common_view.h
RNG_HEADERS+=	concepts.h
RNG_HEADERS+=	copyable_box.h
RNG_HEADERS+=	dangling.h
RNG_HEADERS+=	data.h
RNG_HEADERS+=	drop_view.h
RNG_HEADERS+=	empty.h
RNG_HEADERS+=	empty_view.h
RNG_HEADERS+=	enable_borrowed_range.h
RNG_HEADERS+=	enable_view.h
RNG_HEADERS+=	non_propagating_cache.h
RNG_HEADERS+=	ref_view.h
RNG_HEADERS+=	size.h
RNG_HEADERS+=	subrange.h
RNG_HEADERS+=	transform_view.h
RNG_HEADERS+=	view_interface.h
.for hdr in ${RNG_HEADERS}
RNG+=		${HDRDIR}/__ranges/${hdr}
.endfor

INCSGROUPS+=	UTL
UTLDIR=		${CXXINCLUDEDIR}/__utility
UTL_HEADERS+=	__decay_copy.h
UTL_HEADERS+=	as_const.h
UTL_HEADERS+=	cmp.h
UTL_HEADERS+=	declval.h
UTL_HEADERS+=	exchange.h
UTL_HEADERS+=	forward.h
UTL_HEADERS+=	in_place.h
UTL_HEADERS+=	integer_sequence.h
UTL_HEADERS+=	move.h
UTL_HEADERS+=	pair.h
UTL_HEADERS+=	piecewise_construct.h
UTL_HEADERS+=	rel_ops.h
UTL_HEADERS+=	swap.h
UTL_HEADERS+=	to_underlying.h
.for hdr in ${UTL_HEADERS}
UTL+=		${HDRDIR}/__utility/${hdr}
.endfor

INCSGROUPS+=	VAR
VAR_HEADERS+=	monostate.h
VARDIR=	${CXXINCLUDEDIR}/__variant
.for hdr in ${VAR_HEADERS}
VAR+=		${HDRDIR}/__variant/${hdr}
.endfor

INCSGROUPS+=	EXP
EXPDIR=		${CXXINCLUDEDIR}/experimental
EXP_HEADERS+=	__config
EXP_HEADERS+=	__memory
EXP_HEADERS+=	algorithm
EXP_HEADERS+=	coroutine
EXP_HEADERS+=	deque
EXP_HEADERS+=	filesystem
EXP_HEADERS+=	forward_list
EXP_HEADERS+=	functional
EXP_HEADERS+=	iterator
EXP_HEADERS+=	list
EXP_HEADERS+=	map
EXP_HEADERS+=	memory_resource
EXP_HEADERS+=	propagate_const
EXP_HEADERS+=	regex
EXP_HEADERS+=	set
EXP_HEADERS+=	simd
EXP_HEADERS+=	string
EXP_HEADERS+=	type_traits
EXP_HEADERS+=	unordered_map
EXP_HEADERS+=	unordered_set
EXP_HEADERS+=	utility
EXP_HEADERS+=	vector
.for hdr in ${EXP_HEADERS}
EXP+=		${HDRDIR}/experimental/${hdr}
.endfor

INCSGROUPS+=	EXT
EXTDIR=		${CXXINCLUDEDIR}/ext
EXT_HEADERS+=	__hash
EXT_HEADERS+=	hash_map
EXT_HEADERS+=	hash_set
.for hdr in ${EXT_HEADERS}
EXT+=		${HDRDIR}/ext/${hdr}
.endfor

.include <bsd.lib.mk>
