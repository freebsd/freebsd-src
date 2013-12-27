AUTOMAKE_OPTIONS=	../util/ansi2knr

bindir=		${exec_prefix}/${BINSUBDIR}
bin_PROGRAMS=	@MAKE_ADJTIMED@
EXTRA_PROGRAMS=	adjtimed
AM_CPPFLAGS=	-I$(top_srcdir)/include
LDADD=		../libntp/libntp.a
ETAGS_ARGS=	Makefile.am

include ../bincheck.mf
