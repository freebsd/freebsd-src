PROG=		diff3
SRCS=		diff3.c getopt.c getopt1.c version.c
CFLAGS+=	-I$(.CURDIR)/../diff  -DHAVE_CONFIG_H \
		-DDIFF_PROGRAM=\"/usr/bin/diff\"
MAN=		diff3.1

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../diff
