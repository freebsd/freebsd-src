# @(#)Makefile	8.4 (Berkeley) 8/18/96

ROFF=	groff
TBL=	tbl

edittut.ps: edittut.ms
	${TBL} edittut.ms | ${ROFF} -ms > $@
	chmod 444 $@

clean:
	rm -f edittut.ps
