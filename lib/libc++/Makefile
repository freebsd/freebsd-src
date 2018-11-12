# $FreeBSD$

.include <src.opts.mk>

LIBCXXRTDIR=	${.CURDIR}/../../contrib/libcxxrt
HDRDIR=		${.CURDIR}/../../contrib/libc++/include
SRCDIR=		${.CURDIR}/../../contrib/libc++/src
CXXINCLUDEDIR=	${INCLUDEDIR}/c++/v${SHLIB_MAJOR}

.PATH: ${SRCDIR}

LIB=		c++
SHLIB_MAJOR=	1
SHLIB_LDSCRIPT=	libc++.ldscript

SRCS+=		algorithm.cpp\
		bind.cpp\
		chrono.cpp\
		condition_variable.cpp\
		debug.cpp\
		exception.cpp\
		future.cpp\
		hash.cpp\
		ios.cpp\
		iostream.cpp\
		locale.cpp\
		memory.cpp\
		mutex.cpp\
		new.cpp\
		optional.cpp\
		random.cpp\
		regex.cpp\
		shared_mutex.cpp\
		stdexcept.cpp\
		string.cpp\
		strstream.cpp\
		system_error.cpp\
		thread.cpp\
		typeinfo.cpp\
		utility.cpp\
		valarray.cpp

CXXRT_SRCS+=	libelftc_dem_gnu3.c\
		terminate.cc\
		dynamic_cast.cc\
		memory.cc\
		auxhelper.cc\
		exception.cc\
		stdexcept.cc\
		typeinfo.cc\
		guard.cc

.for _S in ${CXXRT_SRCS}
STATICOBJS+=	cxxrt_${_S:R}.o
cxxrt_${_S}:
	ln -sf ${LIBCXXRTDIR}/${_S} ${.TARGET}
.endfor

WARNS=		0
CFLAGS+=	-I${HDRDIR} -I${LIBCXXRTDIR} -nostdlib -DLIBCXXRT
.if empty(CXXFLAGS:M-std=*)
CXXFLAGS+=	-std=c++11
.endif

LIBADD+=	cxxrt
LDFLAGS+=	--verbose
INCSGROUPS=	STD EXP EXT

STD_HEADERS=	__bit_reference\
		__config\
		__debug\
		__functional_03\
		__functional_base\
		__functional_base_03\
		__hash_table\
		__locale\
		__mutex_base\
		__refstring\
		__split_buffer\
		__sso_allocator\
		__std_stream\
		__tree\
		__tuple\
		__undef___deallocate\
		__undef_min_max\
		algorithm\
		array\
		atomic\
		bitset\
		cassert\
		ccomplex\
		cctype\
		cerrno\
		cfenv\
		cfloat\
		chrono\
		cinttypes\
		ciso646\
		climits\
		clocale\
		cmath\
		codecvt\
		complex\
		complex.h\
		condition_variable\
		csetjmp\
		csignal\
		cstdarg\
		cstdbool\
		cstddef\
		cstdint\
		cstdio\
		cstdlib\
		cstring\
		ctgmath\
		ctime\
		cwchar\
		cwctype\
		deque\
		exception\
		forward_list\
		fstream\
		functional\
		future\
		initializer_list\
		iomanip\
		ios\
		iosfwd\
		iostream\
		istream\
		iterator\
		limits\
		list\
		locale\
		map\
		memory\
		mutex\
		new\
		numeric\
		ostream\
		queue\
		random\
		ratio\
		regex\
		scoped_allocator\
		set\
		shared_mutex\
		sstream\
		stack\
		stdexcept\
		streambuf\
		string\
		strstream\
		system_error\
		tgmath.h\
		thread\
		tuple\
		type_traits\
		typeindex\
		typeinfo\
		unordered_map\
		unordered_set\
		utility\
		valarray\
		vector
RT_HEADERS=	cxxabi.h\
		unwind.h\
		unwind-arm.h\
		unwind-itanium.h

.for hdr in ${STD_HEADERS}
STD+=		${HDRDIR}/${hdr}
INCSLINKS+=	${CXXINCLUDEDIR}/${hdr} ${CXXINCLUDEDIR}/tr1/${hdr}
.endfor
.for hdr in ${RT_HEADERS}
STD+=		${LIBCXXRTDIR}/${hdr}
.endfor
STDDIR=		${CXXINCLUDEDIR}

EXP_HEADERS=	__config\
		algorithm\
		chrono\
		dynarray\
		optional\
		ratio\
		string_view\
		system_error\
		tuple\
		type_traits\
		utility

.for hdr in ${EXP_HEADERS}
EXP+=		${HDRDIR}/experimental/${hdr}
.endfor
EXPDIR=		${CXXINCLUDEDIR}/experimental

EXT_HEADERS=	__hash\
		hash_map\
		hash_set

.for hdr in ${EXT_HEADERS}
EXT+=		${HDRDIR}/ext/${hdr}
.endfor
EXTDIR=		${CXXINCLUDEDIR}/ext

.if ${MK_GNUCXX} == "no" && ${COMPILER_TYPE} == "gcc"
CLEANFILES+=	libstdc++.so libstdc++.a

afterinstall:
	ln -sf ${DESTDIR}${LIBDIR}/lib${LIB}.so \
		${.OBJDIR}/libstdc++.so
	ln -sf ${DESTDIR}${LIBDIR}/lib${LIB}.a \
		${.OBJDIR}/libstdc++.a
.endif

# avoid cyclic dependency
CFLAGS+= -I${LIBCXXRTDIR}
GENDIRDEPS_FILTER= N*/libcxxrt

.include <bsd.lib.mk>
