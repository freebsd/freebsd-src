#
# @(#)Makefile	2.4 88/08/15 4.0 RPCSRC; from 1.9 87/11/29 SMI
#
CFLAGS= -O
RPCLIB= -lrpclib
#RPCLIB= -lrpcsvc

KEYSERV_OBJS = keyserv.o setkey.o detach.o mp.o
KEYENVOY_OBJS = keyenvoy.o
SRCS = keyserv.c setkey.c detach.c keyenvoy.c mp.c

ALL= keyserv keyenvoy

all: $(ALL)

keyserv: $(KEYSERV_OBJS)
	$(CC) $(CFLAGS) $(KEYSERV_OBJS) -lmp $(LIBC) $(RPCLIB) -o $@

keyenvoy:	$(KEYENVOY_OBJS)
	$(CC) $(CFLAGS) $(KEYENVOY_OBJS) $(LIBC) $(RPCLIB) -o $@

clean:
	rm -f $(ALL) $(KEYSERV_OBJS) $(KEYENVOY_OBJS)

install: $(ALL)
	install -s -m  755 keyserv  $(DESTDIR)/usr/etc
	install -s -m 4755 keyenvoy $(DESTDIR)/usr/etc

depend:
	rm -f makedep
	for i in ${SRCS}; do \
	    ${CC} -M ${INCPATH} $$i | \
	    awk ' { if ($$1 != prev) { print rec; rec = $$0; prev = $$1; } \
		else { if (length(rec $$2) > 78) { print rec; rec = $$0; } \
		else rec = rec " " $$2 } } \
		END { print rec } ' >> makedep; done
	echo '/^# DO NOT DELETE THIS LINE/+2,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	cp Makefile Makefile.bak
	ed - Makefile < eddep
	rm eddep makedep
	echo '# DEPENDENCIES MUST END AT END OF FILE' >> Makefile
	echo '# IF YOU PUT STUFF HERE IT WILL GO AWAY' >> Makefile
	echo '# see make depend above' >> Makefile

# DO NOT DELETE THIS LINE -- make depend uses it


