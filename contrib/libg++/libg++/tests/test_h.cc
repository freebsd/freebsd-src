// Use all the g++ headerfiles

// $Author: jason $
// $Revision: 1.17 $
// $Date: 1995/06/11 19:23:13 $

#include <_G_config.h>
// If we have the old iostream library, it defines _OLD_STREAMS
#include <stream.h>

#include <std.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#if _G_HAVE_SYS_WAIT
#include <sys/wait.h>
#endif
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/times.h>

#ifdef _OLD_STREAMS
#include <PlotFile.h>
#include <File.h>
#include <Filebuf.h>
#include <Fmodes.h>
#include <filebuf.h>
#include <SFile.h>
#endif

#include <ACG.h>
#include <Fix.h>
#include <MLCG.h>
#include <AllocRing.h>
#include <Binomial.h>
#include <BitSet.h>
#include <BitString.h>
#include <Complex.h>
#include <DiscUnif.h>
#include <Erlang.h>
#include <GetOpt.h>
#include <Fix16.h>
#include <Fix24.h>
#include <Geom.h>
#include <Rational.h>
#include <HypGeom.h>
#include <Integer.h>
#include <Incremental.h>
#include <LogNorm.h>
#include <NegExp.h>
#include <Normal.h>
#include <Obstack.h>
#include <Pix.h>
#include <SmplHist.h>
#include <Poisson.h>
#include <RNG.h>
#include <Random.h>
#include <SmplStat.h>
#include <Regex.h>
#include <RndInt.h>
#include <builtin.h>
#include <String.h>
#include <Uniform.h>
#include <Weibull.h>

#include <assert.h>
#include <libc.h>
#include <compare.h>
#include <ctype.h>
#include <errno.h>
#include <generic.h>
#include <grp.h>
#include <getpagesize.h>
#include <time.h>
#include <math.h>
#include <minmax.h>
#include <new.h>
#include <osfcn.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strclass.h>
#include <string.h>
#include <swap.h>
#include <unistd.h>
#include <limits.h>
#ifdef _IO_MAGIC
#include <istream.h>
#include <streambuf.h>
#include <ostream.h>
#endif

main()
{
    cout << "Could include all g++-include files\n";
    exit (0);
}
