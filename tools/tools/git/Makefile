SCRIPTS=	git-arc.sh git-mfc
PREFIX?=	/usr/local
BINDIR=		${PREFIX}/bin
MAN=		git-arc.1 git-mfc.1

.if ${.MAKE.OS} == "FreeBSD"
MANDIR?=	${PREFIX}/share/man/man
.else
# Work with vanilla bmake on alternate OSes, e.g., Linux, macOS, etc.
MANDIR?=	${PREFIX}/share/man

.include <scripts.mk>
.endif

.include <bsd.prog.mk>
