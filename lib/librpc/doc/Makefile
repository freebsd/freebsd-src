#
# @(#)Makefile	2.1 88/08/04 4.0 RPCSRC
#
#
#   The targets all.nroff and all.troff will make monolithic documents
#   with nroff and troff, respectively.  The other *.nroff and *.troff
#   targets will make individual documents
#
TROFF=	ditroff
TOPTS=  -t
NROFF=	nroff
NOPTS=
PIC=	pic
TBL=	tbl
EQN=	eqn

SRC=	 rpc.prog.ms rpcgen.ms xdr.nts.ms xdr.rfc.ms rpc.rfc.ms nfs.rfc.ms

all default: all.nroff

install:	all.nroff
	@echo "Nothing installed."

all.nroff:	${SRC}
	${TBL} ${SRC} | ${EQN} | ${NROFF} ${NOPTS} -ms >all.nroff

all.troff:	${SRC}
	${TBL} ${SRC} | ${PIC} | ${EQN} | ${TROFF} ${TOPTS} -ms >all.troff

#

rpc.prog.nroff: rpc.prog.ms
	${TBL} rpc.prog.ms | ${NROFF} ${NOPTS} -ms >rpc.prog.nroff

rpc.prog.troff:	rpc.prog.ms
	${TBL} rpc.prog.ms | ${PIC} | ${TROFF} ${TOPTS} -ms >rpc.prog.troff

#

rpcgen.troff:	rpcgen.ms
	${TBL} rpcgen.ms | ${TROFF} ${TOPTS} -ms >rpcgen.troff

rpcgen.nroff:	rpcgen.ms
	${TBL} rpcgen.ms | ${NROFF} ${NOPTS} -ms >rpcgen.nroff

#

xdr.nts.troff: xdr.nts.ms
	${TBL} xdr.nts.ms | ${EQN} | ${TROFF} ${TOPTS} -ms >xdr.nts.troff

xdr.nts.nroff: xdr.nts.ms
	${TBL} xdr.nts.ms | ${EQN} | ${NROFF} ${NOPTS} -ms >xdr.nts.nroff

#

xdr.rfc.troff: xdr.rfc.ms
	${TBL} xdr.rfc.ms | ${TROFF} ${TOPTS} -ms >xdr.rfc.troff

xdr.rfc.nroff: xdr.rfc.ms
	${TBL} xdr.rfc.ms | ${NROFF} ${NOPTS} -ms >xdr.rfc.nroff

#

rpc.rfc.troff: rpc.rfc.ms
	${TBL} rpc.rfc.ms | ${TROFF} ${TOPTS} -ms >rpc.rfc.troff

rpc.rfc.nroff: rpc.rfc.ms
	${TBL} rpc.rfc.ms | ${NROFF} ${NOPTS} -ms >rpc.rfc.nroff

#

nfs.rfc.troff: nfs.rfc.ms
	${TBL} nfs.rfc.ms | ${TROFF} ${TOPTS} -ms >nfs.rfc.troff

nfs.rfc.nroff: nfs.rfc.ms
	${TBL} nfs.rfc.ms | ${NROFF} ${NOPTS} -ms >nfs.rfc.nroff

clean:
	rm -f *.nroff *.troff

spell:	${SRC}
	@for i in ${SRC}; do \
		echo $$i; spell $$i | sort | comm -23 - spell.ok > $$i.spell; \
	done
