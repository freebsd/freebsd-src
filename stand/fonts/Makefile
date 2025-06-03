.include <bsd.init.mk>

.PATH: ${SRCTOP}/contrib/spleen
.PATH: ${SRCTOP}/contrib/terminus

FONTS=	\
	6x12.fnt.gz \
	8x14.fnt.gz \
	8x14v.fnt.gz \
	8x16.fnt.gz \
	8x16b.fnt.gz \
	8x16v.fnt.gz \
	10x18.fnt.gz \
	10x20.fnt.gz \
	11x22.fnt.gz \
	12x24.fnt.gz \
	14x28.fnt.gz \
	16x32.fnt.gz \
	32x64.fnt.gz \

FILES=	${FONTS} INDEX.fonts
FILESDIR=	/boot/fonts

CLEANFILES+=	${FONTS} ${FONTS:T:S/${COMPRESS_EXT}//g}

6x12.fnt.gz: 6x12.fnt
	${COMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
8x14.fnt.gz: 8x14.fnt
	${COMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
8x14v.fnt.gz: 8x14v.fnt
	${COMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
8x16.fnt.gz: 8x16.fnt
	${COMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
8x16b.fnt.gz: 8x16b.fnt
	${COMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
8x16v.fnt.gz: 8x16v.fnt
	${COMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
10x18.fnt.gz: 10x18.fnt
	${COMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
10x20.fnt.gz: 10x20.fnt
	${COMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
11x22.fnt.gz: 11x22.fnt
	${COMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
12x24.fnt.gz: 12x24.fnt
	${COMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
14x28.fnt.gz: 14x28.fnt
	${COMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
16x32.fnt.gz: 16x32.fnt
	${COMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
32x64.fnt.gz: 32x64.fnt
	${COMPRESS_CMD} ${.ALLSRC} > ${.TARGET}

6x12.fnt: ter-u12n.bdf ter-u12b.bdf
	vtfontcvt -o ${.TARGET} ${.ALLSRC}

8x14.fnt: ter-u14n.bdf ter-u14b.bdf
	vtfontcvt -o ${.TARGET} ${.ALLSRC}

8x14v.fnt: ter-u14v.bdf
	vtfontcvt -o ${.TARGET} ${.ALLSRC}

8x16.fnt: ter-u16n.bdf ter-u16b.bdf
	vtfontcvt -o ${.TARGET} ${.ALLSRC}

8x16b.fnt: ter-u16b.bdf
	vtfontcvt -o ${.TARGET} ${.ALLSRC}

8x16v.fnt: ter-u16v.bdf
	vtfontcvt -o ${.TARGET} ${.ALLSRC}

10x18.fnt: ter-u18n.bdf ter-u18b.bdf
	vtfontcvt -o ${.TARGET} ${.ALLSRC}

10x20.fnt: ter-u20n.bdf ter-u20b.bdf
	vtfontcvt -o ${.TARGET} ${.ALLSRC}

11x22.fnt: ter-u22n.bdf ter-u22b.bdf
	vtfontcvt -o ${.TARGET} ${.ALLSRC}

12x24.fnt: ter-u24n.bdf ter-u24b.bdf
	vtfontcvt -o ${.TARGET} ${.ALLSRC}

14x28.fnt: ter-u28n.bdf ter-u28b.bdf
	vtfontcvt -o ${.TARGET} ${.ALLSRC}

16x32.fnt: ter-u32n.bdf ter-u32b.bdf
	vtfontcvt -o ${.TARGET} ${.ALLSRC}

32x64.fnt: spleen-32x64.bdf
	vtfontcvt -o ${.TARGET} ${.ALLSRC}

.include <bsd.prog.mk>
