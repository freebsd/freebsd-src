#
# $Id: Makefile,v 1.1.1.1 1998/03/01 23:41:09 jdp Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/libiberty

LIB=		iberty
SRCS=		argv.c basename.c choose-temp.c concat.c cplus-dem.c \
		fdmatch.c fnmatch.c getopt.c getopt1.c getruntime.c \
		hex.c floatformat.c objalloc.c obstack.c spaces.c \
		strerror.c strsignal.c xatexit.c xexit.c xmalloc.c \
		xstrdup.c xstrerror.c insque.c
NOPROFILE=	true
NOPIC=		true
INTERNALLIB=	true
INTERNALSTATICLIB=true

.include <bsd.lib.mk>
