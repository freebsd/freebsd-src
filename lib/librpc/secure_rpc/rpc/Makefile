#
# @(#)Makefile	2.3 88/08/15 4.0 RPCSRC
#
#
# Copyright (c) 1987 by Sun Microsystems, Inc.
#

DESTDIR=

CFLAGS = -I..

SRCS = auth_des.c authdes_prot.c key_call.c key_prot.c netname.c svcauth_des.c \
	openchild.c rtime.c publickey.c xcrypt.c \
	auth_none.c auth_unix.c  authunix_prot.c bindresvport.c \
	clnt_generic.c clnt_perror.c clnt_raw.c clnt_simple.c clnt_tcp.c \
	clnt_udp.c rpc_dtablesize.c get_myaddress.c getrpcent.c getrpcport.c \
	pmap_clnt.c pmap_getmaps.c pmap_getport.c pmap_prot.c \
	pmap_prot2.c pmap_rmt.c rpc_prot.c rpc_commondata.c rpc_callmsg.c \
	svc.c svc_auth.c svc_auth_unix.c svc_raw.c svc_run.c svc_simple.c \
	svc_tcp.c svc_udp.c xdr.c xdr_array.c xdr_float.c xdr_mem.c \
	xdr_rec.c xdr_reference.c xdr_stdio.c

OBJS = auth_des.o authdes_prot.o key_call.o key_prot.o netname.o svcauth_des.o \
	openchild.o rtime.o publickey.o xcrypt.o \
	auth_none.o auth_unix.o  authunix_prot.o bindresvport.o \
	clnt_generic.o clnt_perror.o clnt_raw.o clnt_simple.o clnt_tcp.o \
	clnt_udp.o rpc_dtablesize.o get_myaddress.o getrpcent.o getrpcport.o \
	pmap_clnt.o pmap_getmaps.o pmap_getport.o pmap_prot.o \
	pmap_prot2.o pmap_rmt.o rpc_prot.o rpc_commondata.o rpc_callmsg.o \
	svc.o svc_auth.o svc_auth_unix.o svc_raw.o svc_run.o svc_simple.o \
	svc_tcp.o svc_udp.o xdr.o xdr_array.o xdr_float.o xdr_mem.o \
	xdr_rec.o xdr_reference.o xdr_stdio.o

HDRS = key_prot.h \
	auth.h auth_unix.h auth_des.h clnt.h netdb.h pmap_clnt.h \
	pmap_prot.h pmap_rmt.h rpc.h rpc_msg.h svc.h svc_auth.h types.h xdr.h

XFILES= key_prot.x

all rpclib:	librpclib.a

librpclib.a: ${OBJS}
	@echo "building librpclib.a"
	@ar cru librpclib.a ${OBJS}

install: $(HDRS) librpclib.a
	@echo "Creating RPC header directory"
	-mkdir ${DESTDIR}/usr/include/rpc && \
		chown bin ${DESTDIR}/usr/include/rpc && \
		chmod 755 ${DESTDIR}/usr/include/rpc
	@echo "Installing RPC header files"
	-set -x;for i in $(HDRS) $(XFILES) ; do \
		(install -c -m 644 $$i ${DESTDIR}/usr/include/rpc) done
	@echo "Installing RPC library"
	install -c -m 644 librpclib.a ${DESTDIR}/usr/lib
	ranlib ${DESTDIR}/usr/lib/librpclib.a

tags: $(SRCS) $(HDRS)
	ctags -tw $(SRCS) $(HDRS)

ref: tags
	sed 's,	/.*,,' tags | \
	awk ' { printf("%-26s%-16s%s\n", $$1, $$2, $$3) }' > ref

lint: $(SRCS) $(HDRS)
	$(LINT.c) $(SRCS)

clean: rm -f *.o rpclib.a

.SUFFIXES: .x .x~

.x.c:
	rpcgen -c $< | \
	sed 's/^#include \"$*\.h\"/#include <rpc\/$*\.h>/' > $@

.x.h:
	rpcgen -h $< > $@

depend: $(SRCS) $(HDRS)
	@${CC} ${CFLAGS} -M ${SRCS} > makedep
	@echo '/^# DO NOT DELETE THIS LINE/+1,$$d' >eddep
	@echo '$$r makedep' >>eddep
	@echo 'w' >>eddep
	@cp Makefile makefile.bak
	@ed - Makefile < eddep
	@rm eddep makedep makefile.bak

depend.42BSD depend.42bsd:
	cp /dev/null x.c
	for i in $(SRCS) ; do \
              (/bin/grep '^#[         ]*include' x.c $$i | sed \
                      -e '/\.\.\/h/d' \
                      -e '/\.\.\/ufs/d' \
                      -e 's,<\(.*\)>,"/usr/include/\1",' \
                      -e 's/:[^"]*"\([^"]*\)".*/: \1/' \
	                  -e 's/\.c/\.o/' >>makedep); done
	echo '/^# DO NOT DELETE THIS LINE/+2,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	cp Makefile Makefile.bak
	ed - Makefile < eddep
	rm eddep makedep x.c
	echo '# DEPENDENCIES MUST END AT END OF FILE' >> Makefile
	echo '# IF YOU PUT STUFF HERE IT WILL GO AWAY' >> Makefile
	echo '# see make depend above' >> Makefile

# DO NOT DELETE THIS LINE


