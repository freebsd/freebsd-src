LIB= iberty
INTERNALLIB= true
INTERNALSTATICLIB= true

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.PATH:	${GDBDIR}/libiberty

SRCS= argv.c basename.c concat.c cplus-dem.c fdmatch.c getopt.c \
      getopt1.c getruntime.c hex.c floatformat.c obstack.c spaces.c \
      strsignal.c xatexit.c xexit.c \
      xmalloc.c xstrdup.c xstrerror.c insque.c

CFLAGS+= -I$(.CURDIR) -I$(.CURDIR)/../gdb/.

.include <bsd.lib.mk>
