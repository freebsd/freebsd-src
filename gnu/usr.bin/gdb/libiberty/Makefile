LIB= iberty
SRCS= argv.c basename.c concat.c cplus-dem.c fdmatch.c getopt.c \
      getopt1.c ieee-float.c obstack.c spaces.c strerror.c strsignal.c \
      xmalloc.c

CFLAGS+= -I$(.CURDIR)/../gdb/.
NOPROFILE=no
NOPIC=no

install:
	@echo -n

.include <bsd.lib.mk>
