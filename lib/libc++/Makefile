# $FreeBSD$

LIBCXXRTDIR=    ${.CURDIR}/../../contrib/libcxxrt
HDRDIR=         ${.CURDIR}/../../contrib/libc++/include
SRCDIR=         ${.CURDIR}/../../contrib/libc++/src
CXXINCLUDEDIR=  ${INCLUDEDIR}/c++/v${SHLIB_MAJOR}

.PATH: ${SRCDIR}

LIB=            c++
SHLIB_MAJOR=    1

SRCS+=          algorithm.cpp\
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
                random.cpp\
                regex.cpp\
                stdexcept.cpp\
                string.cpp\
                strstream.cpp\
                system_error.cpp\
                thread.cpp\
                typeinfo.cpp\
                utility.cpp\
                valarray.cpp

WARNS=          0
CXXFLAGS+=      -I${HDRDIR} -I${LIBCXXRTDIR} -std=c++0x -nostdlib -DLIBCXXRT

DPADD=          ${LIBCXXRT}
LDADD=          -L${.OBJDIR}/../libcxxrt/ -lcxxrt
LDFLAGS+=       --verbose
INCSGROUPS=     STD EXT

STD_HEADERS=    __bit_reference\
                __config\
                __debug\
                __functional_03\
                __functional_base\
                __functional_base_03\
                __hash_table\
                __locale\
                __mutex_base\
                __split_buffer\
                __sso_allocator\
                __std_stream\
                __tree\
                __tuple\
                __tuple_03\
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

.for hdr in ${STD_HEADERS}
STD+=           ${HDRDIR}/${hdr}
.endfor
STDDIR=         ${CXXINCLUDEDIR}

EXT_HEADERS=    __hash\
                hash_map\
                hash_set

.for hdr in ${EXT_HEADERS}
EXT+=           ${HDRDIR}/ext/${hdr}
.endfor
EXTDIR=         ${CXXINCLUDEDIR}/ext

.include <bsd.lib.mk>
