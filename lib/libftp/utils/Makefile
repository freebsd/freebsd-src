#
# A rather bogus Makefile, but one intended to be used by hand anyway..
#

CFLAGS = -I${.CURDIR} -I${.CURDIR}/.. -DREADLINE
LDADD+=	-L${.CURDIR}
DPADD+=	libetc.a

.if exists(${.CURDIR}/../obj)
LDADD+=	-L${.CURDIR}/../obj
DPADD+= ${.CURDIR}/../obj/libftp.a
.else
LDADD+=	-L${.CURDIR}/..
DPADD+= ${.CURDIR}/../libftp.a
.endif

all: ftptry mirror uftp

ftptry:	ftptry.o
	$(CC) $(CFLAGS) -o ftptry ftptry.o -lftp

uftp:	uftp.o uftpcmd.o libetc.a
	$(CC) $(CFLAGS) -o uftp uftp.o uftpcmd.o -lftp -letc

mirror:	mirror.o
	$(CC) $(CFLAGS) -o mirror mirror.o  -lftp

clean: 
	rm -f ftptry mirror uftp *~ *.o *.a

LIBOBJS= readline.o glob.o	
libetc.a: $(LIBOBJS)	
	ar qc libetc.a  $(LIBOBJS)
	ranlib libetc.a
