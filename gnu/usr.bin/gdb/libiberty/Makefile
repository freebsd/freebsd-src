LIB= iberty

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.PATH:	${GDBDIR}/libiberty

SRCS= argv.c basename.c concat.c cplus-dem.c fdmatch.c getopt.c \
      getopt1.c getruntime.c hex.c floatformat.c obstack.c spaces.c \
      strerror.c strsignal.c vasprintf.c xatexit.c xexit.c \
      xmalloc.c xstrdup.c xstrerror.c insque.c

CFLAGS+= -I$(.CURDIR) -I$(.CURDIR)/../gdb/.
NOPROFILE=no
NOPIC=no

install:
	@echo -n

.include <bsd.lib.mk>
