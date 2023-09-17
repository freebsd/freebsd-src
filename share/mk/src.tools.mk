# Various tools used by the FreeBSD make installworld / distrib-dirs /
# distribution / installkernel targets. Also called "bootstrap tools"
# historically, however that name seemed to be ambiguous, as those tools
# merely help distributing the OS build artefacts into staging / production
# area.
#
# Very tiny subset of "itools", if you are old enough to know what it is.
#
# Please keep the list short, this file may and will be included from
# many places within the source tree. Rule of thumb: if the above mentioned
# targets survive with MYTOOL_CMD=false, then MYTOOL_CMD probably
# does not belong here. Stick it somewhere else, thank you very much!
#

.if !target(__<src.tools.mk>__)

INSTALL_CMD?=	install
MTREE_CMD?=	mtree
PWD_MKDB_CMD?=	pwd_mkdb
SERVICES_MKDB_CMD?=	services_mkdb
CAP_MKDB_CMD?=	cap_mkdb
TIC_CMD?=	tic

__<src.tools.mk>__:
.endif  # !target(__<tools>__)
