#
#	@(#)Makefile	2.1 88/08/10 4.0 RPCSRC
#
DESTDIR=
CFLAGS=	-O
RPCLIB= -lrpclib

# C programs that live in the current directory and need explicit make lines.
# (make depend has special rules for these files)
#
NSTD=	chkey keylogin

all:	${NSTD}

chkey: chkey.c
	${CC} ${CFLAGS} -o chkey chkey.c -lrpcsvc -lmp ${RPCLIB}

keylogin: keylogin.c
	${CC} ${CFLAGS} -o keylogin keylogin.c -lrpcsvc ${RPCLIB}

install:
	-for i in ${NSTD}; do \
		(install -s $$i ${DESTDIR}/usr/bin/$$i); done

clean:
	rm -f a.out core *.s *.o
	rm -f ${NSTD}

depend:
	for i in ${NSTD}; do \
	    cc -M ${INCPATH} $$i.c | sed 's/\.o//' | \
	    awk ' { if ($$1 != prev) \
		{ if (rec != "") print rec; rec = $$0; prev = $$1; } \
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

# Files listed in ${NSTD} have explicit make lines given below.

# DO NOT DELETE THIS LINE -- make depend uses it

