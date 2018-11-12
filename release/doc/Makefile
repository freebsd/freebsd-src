# $FreeBSD$
#
# The user can override the default language to build and install
# with the RELNOTES_LANG variable.
# 
.if defined(RELNOTES_LANG) && !empty(RELNOTES_LANG)
SUBDIR+= 	${RELNOTES_LANG}
.else
SUBDIR+=	en_US.ISO8859-1
.endif
SUBDIR+=	share/xml

RELN_ROOT?= ${.CURDIR}

.if exists(/usr/local/bin/svn)
SVN?=		/usr/local/bin/svn
.elif exists(/usr/bin/svn)
SVN?=		/usr/bin/svn
.else
SVN?=		/usr/bin/svnlite
.endif

SVNFLAGS?=	-r HEAD

update:
.if !exists(${SVN})
	@echo "--------------------------------------------------------------"
	@echo ">>> Updating ${RELN_ROOT} requires ${SVN}."
	@echo "--------------------------------------------------------------"
	@exit 1
.endif
	@echo "--------------------------------------------------------------"
	@echo ">>> Updating ${.CURDIR}"
	@echo "--------------------------------------------------------------"
	@(cd ${.CURDIR} && ${SVN} update ${SVNFLAGS})

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.subdir.mk"
