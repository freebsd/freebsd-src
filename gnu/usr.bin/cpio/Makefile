PROG= cpio
CFLAGS+= -I${.CURDIR} -DRETSIGTYPE=void -DHAVE_SYS_MTIO_H=1 -DSTDC_HEADERS=1 -DHAVE_UNISTD_H=1 -DHAVE_STRING_H=1 -DHAVE_FCNTL_H=1 -DHAVE_UTIME_H=1 -DHAVE_STRERROR=1 -DHAVE_VPRINTF=1 -DDIRENT=1

SRCS =	copyin.c copyout.c copypass.c defer.c dstring.c fnmatch.c global.c \
	main.c tar.c util.c error.c filemode.c getopt.c getopt1.c version.c \
	rtapelib.c dirname.c idcache.c makepath.c xmalloc.c stripslash.c \
	userspec.c xstrdup.c

.include <bsd.prog.mk>
