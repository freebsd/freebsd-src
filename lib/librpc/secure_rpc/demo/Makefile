#
# @(#)Makefile	2.2 88/08/15 4.0 RPCSRC
#
BIN = whoami_svc rme
GEN = whoami_clnt.c whoami_svc.c whoami_xdr.c whoami.h
LIB = -lrpclib
#use this line on SunOS or NFSSRC-based systems.
#LIB = -lrpcsvc
RPCCOM = rpcgen

all: $(BIN)

$(GEN): whoami.x
	$(RPCCOM) whoami.x

whoami_svc: whoami_proc.o whoami_svc.o whoami_xdr.o
	$(CC) -o $@ whoami_proc.o whoami_svc.o whoami_xdr.o $(LIB)

rme: rme.o whoami_clnt.o whoami_xdr.o
	$(CC) -o $@ rme.o whoami_clnt.o whoami_xdr.o $(LIB)

rme.o: rme.c whoami.h

whoami_proc.o: whoami_proc.c whoami.h

clean cleanup:
	rm -f $(GEN) *.o $(BIN)

