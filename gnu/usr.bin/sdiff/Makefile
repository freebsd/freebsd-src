PROG=		sdiff
SRCS=		sdiff.c getopt.c getopt1.c version.c
CFLAGS+=	-I$(.CURDIR)/../diff\
		-DDIRENT=1 -DHAVE_UNISTD_H=1 -DHAVE_DUP2=1 -DHAVE_MEMCHR=1\
		-DHAVE_STRERROR=1 -DHAVE_WAITPID=1 -DHAVE_FCNTL_H=1\
		-DHAVE_STRING_H=1 -DHAVE_SYS_WAIT_H=1 -DHAVE_TIME_H=1\
		-DHAVE_ST_BLKSIZE=1 -DDIFF_PROGRAM=\"/usr/bin/diff\"
MAN=		sdiff.1

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../diff
