PROG=		sdiff
SRCS=		sdiff.c getopt.c getopt1.c version.c
CFLAGS+=	-I$(.CURDIR)/../diff -DHAVE_CONFIG_H \
		-DDIFF_PROGRAM=\"/usr/bin/diff\"
.PATH: $(.CURDIR)/../diff
MAN=		sdiff.1

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../diff
