LIB=		g++

# This cruft is necessary due to the way shared library globals are
# dereferenced.  The libg++ encapsulation of curses makes this necessary.
SHARED_LDADD+= -lcurses

CC=		gcc

SRCS=	 	AllocRing.cc Obstack.cc builtin.cc \
		regex.cc Regex.cc String.cc  Integer.cc Rational.cc Complex.cc Random.cc \
		BitSet.cc BitString.cc LogNorm.cc SmplHist.cc SmplStat.cc \
		Normal.cc NegExp.cc Weibull.cc Erlang.cc DiscUnif.cc \
		Uniform.cc Poisson.cc HypGeom.cc Geom.cc Binomial.cc \
		RNG.cc ACG.cc MLCG.cc  RndInt.cc  \
		Fix.cc Fix16.cc Fix24.cc CursesW.cc GetOpt.cc $(EH_FILES) \
		new.cc chr.cc dtoa.cc error.cc gcd.cc hash.cc \
		lg.cc fmtq.cc ioob.cc pow.cc sqrt.cc str.cc timer.cc \
		math.cc compare.cc SLList.cc DLList.cc \
		streambuf.C stdstrbufs.C iostream.C stdstreams.C \
		strstream.C indstream.C PlotFile.C SFile.C fstream.C \
		parsestream.C stream.C makebuf.C editbuf.C filebuf.C \
		sgetline.C igetline.C igetsb.C procbuf.C sbufvform.C \
		sbufvscan.C stdiostream.C floatconv.C outfloat.C iomanip.C \
		insque.c strerror.c

CFLAGS+=        -I$(.CURDIR)/../libiberty
NOMAN=		noman
.PATH:		$(.CURDIR)/../iostream $(.CURDIR)/../libiberty

.include <bsd.lib.mk>
