#
PROG    = fdformat

# the -I's seem to be confusing, but necessery this way
# (so the right <unistd.h> will be found in /usr/include, and the
# "../i386/isa/ic/nec765.h" included from fdreg.h is accessible, too)
CFLAGS+= -Wall -I/usr/include -I/sys/sys

.include <bsd.prog.mk>
