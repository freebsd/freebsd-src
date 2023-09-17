
OSNAME?=	FreeBSD
SUBDIR=	distextract distfetch partedit runconsoles scripts
SUBDIR_PARALLEL=
SUBDIR_DEPEND_distfetch = distextract
SUBDIR_DEPEND_partedit = distextract
SCRIPTS= bsdinstall
MAN= bsdinstall.8
PACKAGE=	bsdinstall

SCRIPTS+=	startbsdinstall
SCRIPTSDIR_startbsdinstall=	${LIBEXECDIR}/bsdinstall


.include <bsd.prog.mk>
