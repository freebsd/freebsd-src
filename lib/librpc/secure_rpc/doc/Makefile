#
# @(#)Makefile	2.1 88/08/10 4.0 RPCSRC
#
#
TROFF=	ditroff
TOPTS=  -t
NROFF=	nroff
NOPTS=
PIC=	pic
TBL=	tbl
EQN=	eqn

SRC=	 nfs.secure.ms

all default: all.nroff

install:	all.nroff
	@echo "Nothing installed."

all.nroff:	${SRC}
	${TBL} ${SRC} | ${EQN} | ${NROFF} ${NOPTS} -ms >all.nroff

all.troff:	${SRC}
	${TBL} ${SRC} | ${PIC} | ${EQN} | ${TROFF} ${TOPTS} -ms >all.troff

#

nfs.secure.nroff: nfs.secure.ms
	${TBL} nfs.secure.ms | ${EQN} | ${NROFF} ${NOPTS} -ms >nfs.secure.nroff

nfs.secure.troff:	nfs.secure.ms
	${TBL} nfs.secure.ms|${PIC}|${EQN}| ${TROFF} ${TOPTS} -ms >nfs.secure.troff

clean:
	rm -f *.nroff *.troff

spell:	${SRC}
	@for i in ${SRC}; do \
		echo $$i; spell $$i | sort | comm -23 - spell.ok > $$i.spell; \
	done
