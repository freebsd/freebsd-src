#
# $Id: Makefile,v 1.8 1998/02/25 01:23:28 bde Exp $
#

GPPDIR=	${.CURDIR}/../../../contrib/libg++

.PATH:	${GPPDIR}/libstdc++ ${GPPDIR}/libstdc++/stl ${GPPDIR}/libio \
	${GPPDIR}/libiberty

LIB=		stdc++
SHLIB_MAJOR=	2
SHLIB_MINOR=	0

CFLAGS+=	-I${GPPDIR}/include -I${DESTDIR}/usr/include/g++ -I.
CXXFLAGS+=	-I${GPPDIR}/include -I${GPPDIR}/libio -I${GPPDIR}/libstdc++
CXXFLAGS+=	-fno-implicit-templates

DIR=		${GPPDIR}/libstdc++

HDRS=	cassert cctype cerrno cfloat ciso646 climits clocale cmath complex \
	csetjmp csignal cstdarg cstddef cstdio cstdlib cstring ctime \
	cwchar cwctype new stddef string exception stdexcept typeinfo \
	algorithm deque list map queue set stack vector utility functional \
	iterator memory numeric \
	complex.h new.h stl.h

SHDRS=	bastring.cc bastring.h cassert.h cctype.h cerrno.h cfloat.h cinst.h \
	ciso646.h climits.h clocale.h cmath.h complex.h complext.cc \
	complext.h csetjmp.h csignal.h cstdarg.h cstddef.h cstdio.h \
	cstdlib.h cstring.h ctime.h cwchar.h cwctype.h dcomplex.h exception.h \
	fcomplex.h ldcomplex.h new.h sinst.h stddef.h stdexcept.h straits.h \
	string.h typeinfo.h

STLHDRS=algo.h algobase.h bool.h bvector.h defalloc.h deque.h faralloc.h \
	fdeque.h flist.h fmap.h fmultmap.h fmultset.h fset.h function.h \
	hdeque.h heap.h hlist.h hmap.h hmultmap.h hmultset.h hset.h \
	hugalloc.h hvector.h iterator.h lbvector.h ldeque.h list.h llist.h \
	lmap.h lmultmap.h lmultset.h lngalloc.h lset.h map.h multimap.h \
	multiset.h neralloc.h nmap.h nmultmap.h nmultset.h nset.h pair.h \
	projectn.h set.h stack.h tempbuf.h tree.h vector.h

SRCS+=	newi.cc cstringi.cc stddefi.cc typeinfoi.cc exceptioni.cc \
	stdexcepti.cc cstdlibi.cc cmathi.cc

# stl sources.
SRCS+=	tempbuf.cc tree.cc random.cc

# "standard" C parts of libiberty (strerror is a superset of the libc version)
SRCS+=	insque.c strerror.c

# Headers for iostream
IHDRS=	PlotFile.h SFile.h builtinbuf.h editbuf.h floatio.h fstream.h \
	indstream.h iolibio.h iomanip.h iostdio.h iostream.h iostreamP.h \
	istream.h libio.h libioP.h ostream.h parsestream.h pfstream.h \
	procbuf.h stdiostream.h stream.h streambuf.h strfile.h strstream.h

# C++ parts of iostream
SRCS+=	PlotFile.cc SFile.cc builtinbuf.cc editbuf.cc filebuf.cc fstream.cc \
	indstream.cc ioassign.cc ioextend.cc iomanip.cc iostream.cc \
	isgetline.cc isgetsb.cc isscan.cc osform.cc parsestream.cc \
	pfstream.cc procbuf.cc sbform.cc sbgetline.cc sbscan.cc \
	stdiostream.cc stdstrbufs.cc stdstreams.cc stream.cc streambuf.cc \
	strstream.cc

# C parts of iostream
SRCS+=	cleanup.c filedoalloc.c fileops.c floatconv.c genops.c iofclose.c \
	iofgetpos.c iofread.c iofscanf.c iofsetpos.c iogetdelim.c iogetline.c \
	ioignore.c iopadn.c iopopen.c ioprims.c ioprintf.c ioseekoff.c \
	ioseekpos.c iostrerror.c ioungetc.c iovfprintf.c iovfscanf.c \
	outfloat.c strops.c

SRCS+=	config.h
CLEANFILES+=	config.h

config.h:
	touch config.h

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/_G_config.h \
		${HDRS:S;^;${GPPDIR}/libstdc++/;} \
		${IHDRS:S;^;${GPPDIR}/libio/;} \
		${STLHDRS:S;^;${GPPDIR}/libstdc++/stl/;} \
		${DESTDIR}/usr/include/g++
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${SHDRS:S;^;${GPPDIR}/libstdc++/std/;} \
		${DESTDIR}/usr/include/g++/std

# Lots of special cases

# ======== typeinfoi.o, stdexcepti.o ========
.for file in typeinfoi stdexcepti
${file}.o: ${file}.cc
	${CXX} ${CXXFLAGS} -frtti -c ${DIR}/${file}.cc -o ${.TARGET}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

${file}.so: ${file}.cc
	${CXX} ${PICFLAG} -DPIC ${CXXFLAGS} -frtti -c ${DIR}/${file}.cc -o ${.TARGET}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

${file}.po: ${file}.cc
	${CXX} -p ${CXXFLAGS} -frtti -c ${DIR}/${file}.cc -o ${.TARGET}
	@${LD} -O ${.TARGET} -X -r ${.TARGET}
.endfor

# ======== exceptioni.o ========
.for file in exceptioni
${file}.o: ${file}.cc
	${CXX} ${CXXFLAGS} -O0 -frtti -fhandle-exceptions -c ${DIR}/${file}.cc -o ${.TARGET}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

${file}.so: ${file}.cc
	${CXX} ${PICFLAG} -DPIC ${CXXFLAGS} -O0 -frtti -fhandle-exceptions -c ${DIR}/${file}.cc -o ${.TARGET}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

${file}.po: ${file}.cc
	${CXX} -p ${CXXFLAGS} -O0 -frtti -fhandle-exceptions -c ${DIR}/${file}.cc -o ${.TARGET}
	@${LD} -O ${.TARGET} -X -r ${.TARGET}
.endfor

# ======== cstrmain.o ========
SRCS+=	cstrmain.cc
CLEANFILES+=	cstrmain.cc

cstrmain.cc: sinst.cc
	echo "#define C 1" > ${.TARGET}
.for i in REP MAIN TRAITS ADDSS ADDPS ADDCS ADDSP ADDSC  EQSS EQPS EQSP NESS \
    NEPS NESP LTSS LTPS LTSP GTSS GTPS GTSP  LESS LEPS LESP GESS GEPS GESP
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
    DIVCF DIVFC PLUS MINUS EQCC EQCF EQFC NECC NECF NEFC ABS ARG POLAR \
    CONJ NORM COS COSH EXP LOG POWCC POWCF POWCI POWFC SIN SINH SQRT
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
    DIVCF DIVFC PLUS MINUS EQCC EQCF EQFC NECC NECF NEFC ABS ARG POLAR \
    CONJ NORM COS COSH EXP LOG POWCC POWCF POWCI POWFC SIN SINH SQRT
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
    DIVCF DIVFC PLUS MINUS EQCC EQCF EQFC NECC NECF NEFC ABS ARG POLAR \
    CONJ NORM COS COSH EXP LOG POWCC POWCF POWCI POWFC SIN SINH SQRT
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

.include <bsd.lib.mk>
