#
# $Id$
#
SRCS=	bitand.c bitany.c bitblt.c bitclear.c bitcopy.c bitcount.c \
	bitinvert.c bitlcomp.c bitset1.c bitxor.c cleanup.c except.c \
	filedoalloc.c fileops.c floatconv.c genops.c iofclose.c iofgetpos.c \
	iofread.c iofscanf.c iofsetpos.c iogetdelim.c iogetline.c ioignore.c \
	iopadn.c iopopen.c ioprims.c ioprintf.c ioseekoff.c ioseekpos.c \
	iostrerror.c ioungetc.c iovfprintf.c iovfscanf.c outfloat.c \
	rx.c strerror.c strops.c strsignal.c timer.c
SRCS+=	ACG.cc AllocRing.cc Binomial.cc BitSet.cc BitString.cc Complex.cc \
	CursesW.cc DLList.cc DiscUnif.cc Erlang.cc Fix.cc Fix16.cc Fix24.cc \
	Geom.cc GetOpt.cc HypGeom.cc Intdouble.cc Integer.cc LogNorm.cc \
	MLCG.cc NegExp.cc Normal.cc Obstack.cc PlotFile.cc Poisson.cc RNG.cc \
	Random.cc Rational.cc Regex.cc RndInt.cc SFile.cc SLList.cc \
	SmplHist.cc SmplStat.cc String.cc Uniform.cc Weibull.cc builtin.cc \
	builtinbuf.cc chr.cc compare.cc editbuf.cc error.cc filebuf.cc \
	fmtq.cc fstream.cc gcd.cc hash.cc indstream.cc ioassign.cc \
	ioextend.cc iomanip.cc ioob.cc iostream.cc isgetline.cc isgetsb.cc \
	isscan.cc lg.cc math.cc new.cc osform.cc parsestream.cc pfstream.cc \
	pow.cc procbuf.cc sbform.cc sbgetline.cc sbscan.cc sqrt.cc \
	stdiostream.cc stdstrbufs.cc stdstreams.cc str.cc stream.cc \
	streambuf.cc strstream.cc
LIB=	g++
NOMAN=	noman
CFLAGS+=	-nostdinc -I${.CURDIR}/include -I/usr/include
CXXFLAGS+=	-I${.CURDIR}/include -I/usr/include/g++ -I/usr/include \
		-nostdinc++
LDADD+= -lcurses -lcompat
DPADD+=	${LIBCURSES} ${LIBCOMPAT}
.PATH:	${.CURDIR}/libiberty ${.CURDIR}/libio ${.CURDIR}/libg++ ${.CURDIR}/librx

beforeinstall:
	cd ${.CURDIR}/include; \
	for i in *.h; do \
		cmp -s $$i ${DESTDIR}/usr/include/g++/$$i || \
			install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
			${DESTDIR}/usr/include/g++/$$i; \
	done

.include <bsd.lib.mk>
.include <bsd.prog.mk>
