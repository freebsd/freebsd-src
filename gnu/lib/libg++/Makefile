#
# $FreeBSD$
#

SRCS=	vasprintf.c strerror.c strsignal.c
SRCS+=	filedoalloc.c floatconv.c genops.c fileops.c iovfprintf.c iovfscanf.c ioignore.c iopadn.c iofgetpos.c iofread.c iofscanf.c iofsetpos.c iogetline.c ioprintf.c ioseekoff.c ioseekpos.c outfloat.c strops.c iofclose.c iopopen.c ioungetc.c builtinbuf.cc filebuf.cc fstream.cc indstream.cc isgetline.cc isgetsb.cc isscan.cc ioextend.cc iomanip.cc iostream.cc osform.cc procbuf.cc sbform.cc sbgetline.cc sbscan.cc stdiostream.cc stdstrbufs.cc stdstreams.cc stream.cc streambuf.cc strstream.cc PlotFile.cc SFile.cc parsestream.cc pfstream.cc editbuf.cc ioprims.c iostrerror.c cleanup.c
SRCS+=	AllocRing.cc Obstack.cc builtin.cc regex.cc Regex.cc String.cc Intdouble.cc Integer.cc Rational.cc Complex.cc Random.cc BitSet.cc BitString.cc LogNorm.cc SmplHist.cc SmplStat.cc Normal.cc NegExp.cc Weibull.cc Erlang.cc DiscUnif.cc Uniform.cc Poisson.cc HypGeom.cc Geom.cc Binomial.cc RNG.cc ACG.cc MLCG.cc RndInt.cc Fix.cc Fix16.cc Fix24.cc CursesW.cc GetOpt.cc new.cc chr.cc error.cc gcd.cc hash.cc lg.cc fmtq.cc ioob.cc pow.cc sqrt.cc str.cc timer.c math.cc compare.cc bitand.c bitany.c bitblt.c bitclear.c bitcopy.c bitcount.c bitinvert.c bitlcomp.c bitset1.c bitxor.c SLList.cc DLList.cc
LIB=	g++
NOMAN=	noman
CFLAGS+=	-nostdinc -I${.CURDIR}/include -I/usr/include
CXXFLAGS+=	-fexternal-templates -I${.CURDIR}/include
.PATH:	${.CURDIR}/libiberty ${.CURDIR}/libio ${.CURDIR}/libg++

beforeinstall:
	@-if [ ! -d ${DESTDIR}/usr/include/g++ ]; then  mkdir ${DESTDIR}/usr/include/g++;  chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/g++;  chmod 755 ${DESTDIR}/usr/include/g++;  fi
	@(cd ${.CURDIR}/include ; for j in *.h; do  cmp -s $$j ${DESTDIR}/usr/include/g++/$$j ||  install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$j  ${DESTDIR}/usr/include/$$j;  done)

.include <bsd.lib.mk>
