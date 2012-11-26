#
# $FreeBSD$
#
# For multiple programs using a single source file each,
# we can just define 'progs' and create custom targets.
PROGS	=	pkt-gen bridge testpcap libnetmap.so

CLEANFILES = $(PROGS) pcap.o
NO_MAN=
CFLAGS += -Werror -Wall -nostdinc -I/usr/include -I../../../sys
CFLAGS += -Wextra

LDFLAGS += -lpthread -lpcap

.include <bsd.prog.mk>
.include <bsd.lib.mk>

all: $(PROGS)

testpcap: pcap.c libnetmap.so
	$(CC) $(CFLAGS) -L. -lnetmap -o ${.TARGET} pcap.c
	
libnetmap.so:	pcap.c
	$(CC) $(CFLAGS) -fpic -c ${.ALLSRC}
	$(CC) -shared -o ${.TARGET} ${.ALLSRC:.c=.o}
