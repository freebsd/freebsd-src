# $FreeBSD$
#
# The user can override the default language to build and install
# with the RELNOTES_LANG variable.
# 
.if defined(RELNOTES_LANG) && !empty(RELNOTES_LANG)
SUBDIR = 	${RELNOTES_LANG}
.else
SUBDIR =	en_US.ISO8859-1
.endif

RELN_ROOT?= ${.CURDIR}

.if defined(SUPHOST)
SUPFLAGS+=	-h ${SUPHOST}
.endif
update:
.if defined(SUP_UPDATE)
.if !defined(DOCSUPFILE)
	@echo "Error: Please define DOCSUPFILE before doing make update."
	@exit 1
.endif
	@echo "--------------------------------------------------------------"
	@echo ">>> Running ${SUP}"
	@echo "--------------------------------------------------------------"
	@${SUP} ${SUPFLAGS} ${DOCSUPFILE}
.elif defined(CVS_UPDATE)
	@echo "--------------------------------------------------------------"
	@echo ">>> Updating ${.CURDIR} from cvs repository" ${CVSROOT}
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; cvs -q update -P -d
.else
	@echo "Error: Please define either SUP_UPDATE or CVS_UPDATE first."
.endif

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.subdir.mk"
