
PROG=	diff
SRCS=	diff.c analyze.c io.c context.c ed.c normal.c ifdef.c util.c dir.c \
	version.c regex.c getopt.c getopt1.c side.c
CFLAGS+=-DDIRENT=1 -DHAVE_UNISTD_H=1 -DHAVE_DUP2=1 -DHAVE_MEMCHR=1 \
	-DHAVE_STRERROR=1 -DHAVE_WAITPID=1 -DHAVE_FCNTL_H=1 \
	-DHAVE_STRING_H=1 -DHAVE_SYS_WAIT_H=1 -DHAVE_TIME_H=1 \
	-DHAVE_ST_BLKSIZE=1 
NOMAN=noman

.include <bsd.prog.mk>
