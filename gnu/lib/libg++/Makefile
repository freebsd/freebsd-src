#
# $Id: Makefile,v 1.16.2.1 1997/05/23 08:35:08 asami Exp $
#

GPPDIR=	${.CURDIR}/../../../contrib/libg++

.PATH:	${GPPDIR}/libiberty ${GPPDIR}/libio ${GPPDIR}/libg++/src ${GPPDIR}/librx

SUBDIR+=	doc

LIB=		g++
SHLIB_MAJOR=	4
SHLIB_MINOR=	0

CFLAGS+=	-I${GPPDIR}/include -I${DESTDIR}/usr/include/g++
CXXFLAGS+=	-I${GPPDIR}/include -I${GPPDIR}/libio -I${GPPDIR}/libstdc++

# C source
SRCS=	bitand.c bitany.c bitblt.c bitclear.c bitcopy.c bitcount.c \
	bitinvert.c bitlcomp.c bitset1.c bitxor.c timer.c

# C source for librx
SRCS+=	rx.c

# C++ source
SRCS+=	ACG.cc AllocRing.cc Binomial.cc BitSet.cc BitString.cc DLList.cc \
	DiscUnif.cc Erlang.cc Fix.cc Fix16.cc Fix24.cc Geom.cc GetOpt.cc \
	HypGeom.cc Intdouble.cc Integer.cc LogNorm.cc MLCG.cc NegExp.cc \
	Normal.cc Obstack.cc Poisson.cc RNG.cc Random.cc Rational.cc \
	Regex.cc RndInt.cc SLList.cc SmplHist.cc SmplStat.cc String.cc \
	Uniform.cc Weibull.cc \
	builtin.cc compare.cc error.cc fmtq.cc gcd.cc hash.cc ioob.cc \
	lg.cc pow.cc sqrt.cc

# "normal" headers  (no CursesW.h[its useless], no bool.h[in stdc++])
HDRS=	ACG.h AllocRing.h Binomial.h BitSet.h BitString.h Complex.h \
	DLList.h DiscUnif.h Erlang.h Fix.h Fix16.h Fix24.h Geom.h GetOpt.h \
	HypGeom.h Incremental.h Integer.h LogNorm.h MLCG.h NegExp.h Normal.h \
	Obstack.h Pix.h Poisson.h RNG.h Random.h Rational.h Regex.h RndInt.h \
	SLList.h SmplHist.h SmplStat.h String.h Uniform.h Weibull.h \
	bitdo1.h bitdo2.h bitprims.h builtin.h compare.h generic.h \
	getpagesize.h libc.h minmax.h osfcn.h std.h strclass.h swap.h \
	sysent.h typemacros.h

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${HDRS:S;^;${GPPDIR}/libg++/src/;} \
		${GPPDIR}/librx/rx.h \
		${DESTDIR}/usr/include/g++

.include <bsd.lib.mk>
