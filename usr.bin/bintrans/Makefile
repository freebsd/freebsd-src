#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.include <src.opts.mk>

PROG=	bintrans
SRCS=	bintrans.c uuencode.c uudecode.c qp.c
MAN=	bintrans.1 uuencode.format.5
LINKS+=	${BINDIR}/bintrans ${BINDIR}/uuencode
LINKS+=	${BINDIR}/bintrans ${BINDIR}/b64encode
LINKS+=	${BINDIR}/bintrans ${BINDIR}/uudecode
LINKS+=	${BINDIR}/bintrans ${BINDIR}/b64decode
LINKS+=	${BINDIR}/bintrans ${BINDIR}/base64
MLINKS=	bintrans.1 uudecode.1 \
	bintrans.1 uuencode.1 \
	uuencode.format.5 uuencode.5 \
	bintrans.1 b64encode.1 \
	b64encode.1 b64decode.1 \
	b64encode.1 base64.1

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
