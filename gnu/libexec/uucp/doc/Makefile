DOC = uucp
BINDIR = /usr/share/info
PRINTER = texi           # hack for bsd.doc.mk all target

all:
	makeinfo -o ${DOC}.info ${.CURDIR}/${DOC}.texi

.include <bsd.doc.mk>
