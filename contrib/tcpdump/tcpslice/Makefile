#	@(#)Makefile	0.1 (RWGrimes) 3/24/93

PROG=	tcpslice
CFLAGS+=-DCSLIP -I. -I$(.CURDIR)/../tcpdump
MAN1=	tcpslice.1
SRCS=	version.c tcpslice.c gwtm2secs.c search.c \
	savefile.c bpf_filter.c md.c util.c
.PATH:	${.CURDIR}/../tcpdump /sys/net
CLEANFILES+=	version.c version.h

version.c version.h: $(.CURDIR)/../tcpdump/VERSION
	rm -f version.c ; \
	sed 's/.*/char version[] = "&";/' $(.CURDIR)/../tcpdump/VERSION > version.c
	set `sed 's/\([0-9]*\)\.\([0-9]*\).*/\1 \2/' $(.CURDIR)/../tcpdump/VERSION` ; \
		{ echo '#define VERSION_MAJOR' $$1 ; \
		  echo '#define VERSION_MINOR' $$2 ; } > version.h

.include <bsd.prog.mk>
