# $FreeBSD$

SRCDIR=	${.CURDIR}/../../../contrib/libstdc++.295

.PATH: ${SRCDIR} ${SRCDIR}/stl ${SRCDIR}/../libio

SUBDIR=	doc

LIB=		stdc++
SHLIB_MAJOR=	3
SHLIB_MINOR=	0

CFLAGS+=	-I${.CURDIR} -I${SRCDIR}/include -I${SRCDIR}/../libio -I.
CXXFLAGS+=	-I${SRCDIR}/stl -I${SRCDIR} -I${SRCDIR}/../gcc/cp/inc -I.
CXXFLAGS+=	-nostdinc++ -fno-implicit-templates

LDADD+=		-lm
DPADD+=		${LIBM}

HDRS=	cassert cctype cerrno cfloat ciso646 climits clocale cmath complex \
	csetjmp csignal cstdarg cstddef cstdio cstdlib cstring ctime \
	cwchar cwctype fstream iomanip iosfwd iostream sstring stdexcept \
	string strstream complex.h stl.h valarray

STDHDRS=bastring.cc bastring.h complext.cc complext.h dcomplex.h fcomplex.h \
	gslice.h gslice_array.h indirect_array.h ldcomplex.h mask_array.h \
	slice.h slice_array.h std_valarray.h straits.h valarray_array.h \
	valarray_array.tcc valarray_meta.h

STLHDRS=algo.h algobase.h algorithm alloc.h bitset bvector.h defalloc.h deque \
	deque.h function.h functional hash_map hash_map.h hash_set hash_set.h \
	hashtable.h heap.h iterator iterator.h list list.h map map.h memory \
	multimap.h multiset.h numeric pair.h pthread_alloc pthread_alloc.h \
	queue rope rope.h ropeimpl.h set set.h slist slist.h stack stack.h \
	stl_algo.h stl_algobase.h stl_alloc.h stl_bvector.h stl_config.h \
	stl_construct.h stl_deque.h stl_function.h stl_hash_fun.h \
	stl_hash_map.h stl_hash_set.h stl_hashtable.h stl_heap.h \
	stl_iterator.h stl_list.h stl_map.h stl_multimap.h stl_multiset.h \
	stl_numeric.h stl_pair.h stl_queue.h stl_raw_storage_iter.h \
	stl_relops.h stl_rope.h stl_set.h stl_slist.h stl_stack.h \
	stl_tempbuf.h stl_tree.h stl_uninitialized.h stl_vector.h \
	tempbuf.h tree.h type_traits.h utility vector vector.h

# libstdc++ sources
SRCS+=	cmathi.cc cstdlibi.cc cstringi.cc stdexcepti.cc stlinst.cc

# Headers for iostream / libio
IHDRS=	PlotFile.h SFile.h builtinbuf.h editbuf.h floatio.h fstream.h \
	indstream.h iolibio.h iomanip.h iostdio.h iostream.h iostreamP.h \
	istream.h libio.h libioP.h ostream.h parsestream.h pfstream.h \
	procbuf.h stdiostream.h stream.h streambuf.h strfile.h strstream.h

# C++ parts of iostream / libio
SRCS+=	PlotFile.cc SFile.cc builtinbuf.cc editbuf.cc filebuf.cc fstream.cc \
	indstream.cc ioassign.cc ioextend.cc iomanip.cc iostream.cc \
	isgetline.cc isgetsb.cc isscan.cc osform.cc parsestream.cc \
	pfstream.cc procbuf.cc sbform.cc sbgetline.cc sbscan.cc \
	stdiostream.cc stdstrbufs.cc stdstreams.cc stream.cc streambuf.cc \
	strstream.cc

# C parts of iostream / libio
SRCS+=	cleanup.c filedoalloc.c fileops.c floatconv.c genops.c iofclose.c \
	iofdopen.c iofeof.c ioferror.c iofflush.c iofflush_u.c iofgetpos.c \
	iofgets.c iofopen.c iofprintf.c iofputs.c iofread.c iofscanf.c \
	iofsetpos.c ioftell.c iofwrite.c iogetc.c iogetdelim.c iogetline.c \
	iogets.c ioignore.c iopadn.c ioperror.c iopopen.c ioprims.c ioprintf.c \
	ioputc.c ioputs.c ioscanf.c ioseekoff.c ioseekpos.c iosetbuffer.c \
	iosetvbuf.c iosprintf.c iosscanf.c iostrerror.c ioungetc.c \
	iovfprintf.c iovfscanf.c iovsprintf.c iovsscanf.c outfloat.c \
	peekc.c strops.c

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/_G_config.h \
	    ${HDRS:S;^;${SRCDIR}/;} \
	    ${IHDRS:S;^;${SRCDIR}/../libio/;} \
	    ${STLHDRS:S;^;${SRCDIR}/stl/;} \
	    ${DESTDIR}/usr/include/g++
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${STDHDRS:S;^;${SRCDIR}/std/;} \
	    ${DESTDIR}/usr/include/g++/std

CLEANFILES+=	dummy.h dummy.c dummy.C

# Lots of special cases

# ======== cstrmain.o ========
SRCS+=	cstrmain.cc
CLEANFILES+=	cstrmain.cc

cstrmain.cc: sinst.cc
	echo "#define C 1" > ${.TARGET}
.for i in REP MAIN TRAITS ADDSS ADDPS ADDCS ADDSP ADDSC  EQSS EQPS EQSP NESS \
    NEPS NESP LTSS LTPS LTSP GTSS GTPS GTSP LESS LEPS LESP GESS GEPS GESP
	echo "#define $i 1" >> ${.TARGET}
.endfor
	cat ${.ALLSRC} >> ${.TARGET}

# ======== cstrio.o ========
SRCS+=	cstrio.cc
CLEANFILES+=	cstrio.cc

cstrio.cc: sinst.cc
	echo "#define C 1" > ${.TARGET}
.for i in EXTRACT INSERT GETLINE
	echo "#define $i 1" >> ${.TARGET}
.endfor
	cat ${.ALLSRC} >> ${.TARGET}

# ======== fcomplex.o =======
SRCS+=	fcomplex.cc
CLEANFILES+=	fcomplex.cc

fcomplex.cc: cinst.cc
	echo "#define F 1" > ${.TARGET}
.for i in MAIN ADDCC ADDCF ADDFC SUBCC SUBCF SUBFC MULCC MULCF MULFC DIVCC \
    DIVCF DIVFC PLUS MINUS EQCC EQCF EQFC NECC NECF NEFC ABS ARG POLAR CONJ \
    NORM COS COSH EXP LOG POWCC POWCF POWCI POWFC SIN SINH SQRT
	echo "#define $i 1" >> ${.TARGET}
.endfor
	cat ${.ALLSRC} >> ${.TARGET}

# ======== fcomio.o =======
SRCS+=	fcomio.cc
CLEANFILES+=	fcomio.cc

fcomio.cc: cinst.cc
	echo "#define F 1" > ${.TARGET}
.for i in EXTRACT INSERT
	echo "#define $i 1" >> ${.TARGET}
.endfor
	cat ${.ALLSRC} >> ${.TARGET}

# ======== dcomplex.o =======
SRCS+=	dcomplex.cc
CLEANFILES+=	dcomplex.cc

dcomplex.cc: cinst.cc
	echo "#define D 1" > ${.TARGET}
.for i in MAIN ADDCC ADDCF ADDFC SUBCC SUBCF SUBFC MULCC MULCF MULFC DIVCC \
    DIVCF DIVFC PLUS MINUS EQCC EQCF EQFC NECC NECF NEFC ABS ARG POLAR CONJ \
    NORM COS COSH EXP LOG POWCC POWCF POWCI POWFC SIN SINH SQRT
	echo "#define $i 1" >> ${.TARGET}
.endfor
	cat ${.ALLSRC} >> ${.TARGET}

# ======== dcomio.o =======
SRCS+=	dcomio.cc
CLEANFILES+=	dcomio.cc

dcomio.cc: cinst.cc
	echo "#define D 1" > ${.TARGET}
.for i in EXTRACT INSERT
	echo "#define $i 1" >> ${.TARGET}
.endfor
	cat ${.ALLSRC} >> ${.TARGET}

# ======== ldcomplex.o =======
SRCS+=	ldcomplex.cc
CLEANFILES+=	ldcomplex.cc

ldcomplex.cc: cinst.cc
	echo "#define LD 1" > ${.TARGET}
.for i in MAIN ADDCC ADDCF ADDFC SUBCC SUBCF SUBFC MULCC MULCF MULFC DIVCC \
    DIVCF DIVFC PLUS MINUS EQCC EQCF EQFC NECC NECF NEFC ABS ARG POLAR CONJ \
    NORM COS COSH EXP LOG POWCC POWCF POWCI POWFC SIN SINH SQRT
	echo "#define $i 1" >> ${.TARGET}
.endfor
	cat ${.ALLSRC} >> ${.TARGET}

# ======== ldcomio.o =======
SRCS+=	ldcomio.cc
CLEANFILES+=	ldcomio.cc

ldcomio.cc: cinst.cc
	echo "#define LD 1" > ${.TARGET}
.for i in EXTRACT INSERT
	echo "#define $i 1" >> ${.TARGET}
.endfor
	cat ${.ALLSRC} >> ${.TARGET}

# ======== _G_config.h generation =======
# In case you want or need to generate a _G_config.h file.
make_G_config_h:
	CC="${CC}" CXX="${CXX}" CONFIG_NM="nm" \
	    CPP="${CC} -E -nostdinc -idirafter ${DESTDIR}/usr/include" \
	    sh ${SRCDIR}/../libio/gen-params \
	    LIB_VERSION=${SHLIB_MAJOR}.${SHLIB_MINOR}.0 >${.TARGET}

.include <bsd.lib.mk>
