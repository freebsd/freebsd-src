LIB = man

.if exists(${.OBJDIR})
CONFH=  ${.OBJDIR}/config.h
.else
CONFH=  ${.CURDIR}/config.h
.endif

NOPROFILE=	YES

CFLAGS+= -I${.CURDIR} -DSTDC_HEADERS -DPOSIX -DHAS_TROFF -DDO_COMPRESS -DALT_SYSTEMS
CLEANFILES+=	${CONFH}
SRCS =	util.c gripes.c ${CONFH}

libman.a:: ${CONFH}

install:
	@echo -n

${CONFH}: ${.CURDIR}/config.h_dist ${.CURDIR}/../Makefile.inc
	sed -e 's,%apropos%,${apropos},' -e 's,%whatis%,${whatis},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%nroff%,${nroff},' -e 's,%tbl%,${tbl},' \
	    -e 's,%col%,${col},' -e 's,%pic%,${pic},' \
	    -e 's,%eqn%,${eqn},' -e 's,%neqn%,${neqn},' \
	    -e 's,%vgrind%,${vgrind},' -e 's,%refer%,${refer},' \
	    -e 's,%grap%,${grap},' -e 's,%zcat%,${zcat},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
	    -e 's,%compress%,${compress},' \
	    -e 's,%compext%,${compext},' \
        ${.CURDIR}/config.h_dist > ${CONFH}

.include <bsd.lib.mk>
