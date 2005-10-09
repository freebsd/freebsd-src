
SUBDIRS=	C et finnish french german greek italian ja pl russian \
		spanish ukrainian
#GENCAT=	gencat --new
#CHARSET=	charset
GENCAT=		gencat

catalogs:
	@for i in ${SUBDIRS} ; \
	do \
	    echo "$$i"; \
	    cd $$i; \
	    cat ${CHARSET} set[0-9] set[0-9][0-9] > ../../tcsh.$$i.m; \
	    cd ..; \
	    ${GENCAT} ../tcsh.$$i.cat ../tcsh.$$i.m; \
	done

clean:
	@for i in ${SUBDIRS} ; \
	do \
	    rm -f $$i/*~; \
	    rm -f ../tcsh.$$i.cat ../tcsh.$$i.m; \
	done
