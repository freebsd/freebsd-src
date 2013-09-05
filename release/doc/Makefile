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

SVN?=		/usr/local/bin/svn
SVNFLAGS?=	-r HEAD

update:
.if (defined(CVS_UPDATE) || defined(SUP_UPDATE)) && !defined(SVN_UPDATE)
	@echo "--------------------------------------------------------------"
	@echo "CVS_UPDATE and SUP_UPDATE are no longer supported."
	@echo "Please see: https://wiki.freebsd.org/CvsIsDeprecated"
	@echo "--------------------------------------------------------------"
	@exit 1
.endif
.if defined(SVN_UPDATE)
. if !exists(${SVN})
	@echo "--------------------------------------------------------------"
	@echo ">>> Updating ${RELN_ROOT} requires ${SVN}."
	@echo "--------------------------------------------------------------"
	@exit 1
. endif
	@echo "--------------------------------------------------------------"
	@echo ">>> Updating ${.CURDIR} using Subversion"
	@echo "--------------------------------------------------------------"
	@(cd ${.CURDIR} && ${SVN} update ${SVNFLAGS})
.else
	@echo "Error: Please define SVN_UPDATE first."
.endif

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.subdir.mk"
