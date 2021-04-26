# $FreeBSD$

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
INCSGROUPS=	STD MEM EXP EXT

STD_HEADERS+=	__availability
STD_HEADERS+=	__bit_reference
STD_HEADERS+=	__bits
STD_HEADERS+=	__bsd_locale_defaults.h
STD_HEADERS+=	__bsd_locale_fallbacks.h
STD_HEADERS+=	__debug
STD_HEADERS+=	__errc
STD_HEADERS+=	__functional_03
STD_HEADERS+=	__functional_base
STD_HEADERS+=	__functional_base_03
STD_HEADERS+=	__hash_table
STD_HEADERS+=	__libcpp_version
STD_HEADERS+=	__locale
STD_HEADERS+=	__mutex_base
STD_HEADERS+=	__node_handle
STD_HEADERS+=	__nullptr
STD_HEADERS+=	__split_buffer
STD_HEADERS+=	__sso_allocator
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
STD_HEADERS+=	mutex
STD_HEADERS+=	new
STD_HEADERS+=	numbers
STD_HEADERS+=	numeric
STD_HEADERS+=	optional
STD_HEADERS+=	ostream
STD_HEADERS+=	queue
STD_HEADERS+=	random
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

RT_HEADERS+=	cxxabi.h
RT_HEADERS+=	unwind-arm.h
RT_HEADERS+=	unwind-itanium.h
RT_HEADERS+=	unwind.h

.for hdr in ${STD_HEADERS}
STD+=		${HDRDIR}/${hdr}
INCSLINKS+=	../${hdr} ${CXXINCLUDEDIR}/tr1/${hdr}
.endfor
.for hdr in ${RT_HEADERS}
STD+=		${_LIBCXXRTDIR}/${hdr}
.endfor
STDDIR=		${CXXINCLUDEDIR}

# Special case for __config, which as of libc++ 12.0.0 is produced by
# concatenating the locally generated __config_site and the upstream __config
# files.
CONFIG_HEADER=	__config

${CONFIG_HEADER}: ${.CURDIR}/__config_site ${HDRDIR}/__config
	cat ${.ALLSRC} > ${.TARGET}
STD+=		${CONFIG_HEADER}
CLEANFILES+=	${CONFIG_HEADER}

MEM_HEADERS+=	allocator_traits.h
MEM_HEADERS+=	base.h
MEM_HEADERS+=	pointer_traits.h
MEM_HEADERS+=	utilities.h

.for hdr in ${MEM_HEADERS}
MEM+=		${HDRDIR}/__memory/${hdr}
.endfor
MEMDIR=		${CXXINCLUDEDIR}/__memory

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
EXPDIR=		${CXXINCLUDEDIR}/experimental

EXT_HEADERS+=	__hash
EXT_HEADERS+=	hash_map
EXT_HEADERS+=	hash_set

.for hdr in ${EXT_HEADERS}
EXT+=		${HDRDIR}/ext/${hdr}
.endfor
EXTDIR=		${CXXINCLUDEDIR}/ext

.include <bsd.lib.mk>
