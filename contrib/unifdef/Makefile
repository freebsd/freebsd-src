# Makefile for unifdef

prefix =	${HOME}
bindir =	${prefix}/bin
mandir =	${prefix}/share/man
man1dir=	${mandir}/man1

bindest=	${DESTDIR}${bindir}
man1dest=	${DESTDIR}${man1dir}

all: unifdef

unifdef: unifdef.c unifdef.h version.h
	${CC} ${CFLAGS} ${LDFLAGS} -o unifdef unifdef.c

version.h: version.sh
version.sh::
	scripts/reversion.sh

test: unifdef
	scripts/runtests.sh tests

install: unifdef unifdefall.sh unifdef.1
	: commands
	install -m 755 -d  ${bindest}
	install -m 755 unifdef  ${bindest}/
	install -m 755 unifdefall.sh  ${bindest}/unifdefall
	: manual
	install -m 755 -d  ${man1dest}
	install -m 644 unifdef.1  ${man1dest}/
	ln -s unifdef.1  ${man1dest}/unifdefall.1

clean:
	rm -f unifdef version.h
	rm -f tests/*.out tests/*.err tests/*.rc

realclean: clean
	rm -f unifdef.txt
	[ ! -d .git ] || rm -f Changelog version.sh
	find . -name .git -prune -o \( \
		-name '*~' -o -name '.#*' -o \
		-name '*.orig' -o -name '*.core' -o \
		-name 'xterm-*' -o -name 'xterm.tar.gz' \
		\) -delete

DISTEXTRA= version.h version.sh unifdef.txt Changelog

release: ${DISTEXTRA}
	scripts/copycheck.sh
	scripts/release.sh ${DISTEXTRA}

unifdef.txt: unifdef.1
	nroff -Tascii -mdoc unifdef.1 | col -bx >unifdef.txt

Changelog: version.sh scripts/gitlog2changelog.sh
	scripts/gitlog2changelog.sh >Changelog

# eof
