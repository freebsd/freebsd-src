LIB = man

CFLAGS+= -I${.CURDIR} -DSTDC_HEADERS -DPOSIX -DHAS_TROFF -DDO_UNCOMPRESS -DALT_SYSTEMS
CLEANFILES+=	${.CURDIR}/config.h
SRCS =	util.c gripes.c

libman.a::	${.CURDIR}/config.h

install:
	@echo -n

${.CURDIR}/config.h:	${.CURDIR}/config.h_dist ../Makefile.inc
	sed -e 's,%apropos%,${apropos},' -e 's,%whatis%,${whatis},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%nroff%,${nroff},' -e 's,%tbl%,${tbl},' \
	    -e 's,%col%,${col},' -e 's,%pic%,${pic},' \
	    -e 's,%eqn%,${eqn},' -e 's,%neqn%,${neqn},' \
	    -e 's,%vgrind%,${vgrind},' -e 's,%refer%,${refer},' \
	    -e 's,%grap%,${grap},' -e 's,%zcat%,${zcat},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
        ${.CURDIR}/config.h_dist > ${.CURDIR}/config.h

.include <bsd.lib.mk>
