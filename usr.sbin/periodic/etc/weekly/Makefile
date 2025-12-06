.include <src.opts.mk>

PACKAGE=	periodic

CONFS=	340.noid \
	450.status-security \
	999.local

# NB: keep these sorted by MK_* knobs

CONFGROUPS.${MK_LOCATE}+=	LOCATE
LOCATE=	310.locate

CONFGROUPS.${MK_MAN_UTILS}+=	WHATIS
WHATISPACKAGE=			mandoc
WHATIS=	320.whatis

.include <bsd.prog.mk>
