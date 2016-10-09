LIB=	hnj
SRCS=	hnjalloc.c \
	hyphen.c

FILESDIR=	${HYPDIR}
FILES=	hyph_de_DE.dic \
	hyph_de_DE@traditional.dic \
	hyph_en_US.dic \
	hyph_fr_FR.dic \
	hyph_it_IT.dic \
	hyph_la_VA.dic \
	hyph_nl_NL.dic

INTERNALLIB=	yes
MAN=
CFLAGS+=	-I${DOCTOOLSDIR}/troff/libhnj

.include <bsd.lib.mk>

.PATH:	${DOCTOOLSDIR}/troff/libhnj
.PATH:	${DOCTOOLSDIR}/troff/libhnj/hyphen.d
