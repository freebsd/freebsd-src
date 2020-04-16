BASE=	../..
include ${BASE}/Makefile.inc
include Makefile.common
clean: afterclean
afterclean:
	for D in ${SUBDIR}; do (cd $$D && make clean); done
