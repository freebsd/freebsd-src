PROG=	malloc
NOMAN=	sorry

libcmalloc:
	make clean 
	make "CFLAGS=-O2"
	mv malloc libcmalloc
	@echo
	@csh -x -c "time ./libcmalloc      500000  2000 8192"
	@csh -x -c "time ./libcmalloc    50000000  2000 8192"
	@csh -x -c "time ./libcmalloc      500000 14000 8192"
	@csh -x -c "time ./libcmalloc    20000000 20000 2048"

gnumalloc:
	make clean 
	make "CFLAGS=-lgnumalloc -O2"
	mv malloc gnumalloc
	@csh -x -c "time ./gnumalloc   500000  2000 8192"
	@csh -x -c "time ./gnumalloc 50000000  2000 8192"
	@csh -x -c "time ./gnumalloc   500000 14000 8192"
	@csh -x -c "time ./gnumalloc 20000000 20000 2048"

.include <bsd.prog.mk>
