FILES=	Acknowledgements Bibliography BugList ChangeLog CharGen \
	Charsets EscapeSequences Keyboard.HP Keyboard.VT \
	Manifest NotesAndHints TestedHardware ToDo

beforeinstall:
	for file in ${FILES}; \
	do \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/$$file ${DESTDIR}${BINDIR}/Doc/$$file ; \
	done

afterdistribute: beforeinstall

.include "../Makefile.inc"
.include <bsd.subdir.mk>
